#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <thorvg.h>
#include <time.h>
#include <unistd.h>
#include <vector>
#include <xf86drm.h>
#include <xf86drmMode.h>

int main() {
  // Open DRM device
  int fd = -1;
  char dev_path[] = "/dev/dri/card*";
  for (char i = 0; i < 9 && fd == -1; i++) {
    dev_path[sizeof(dev_path) - 1 - 1] = '0' + i;
    fd = open(dev_path, O_RDWR | O_CLOEXEC);
  }
  if (fd == -1) {
    perror("Failed to open DRM device");
    return 1;
  }

  // Define many vars
  drmModeRes *res = NULL;
  drmModeConnector *conn = NULL;
  drmModeModeInfo *mode = NULL;
  drmModeCrtc *crtc = NULL;
  uint32_t fb_handle = 0;
  uint32_t fb_pitch = 0;
  uint64_t fb_size = 0;
  uint32_t fb_id = 0;
  uint64_t fb_offset = 0;
  uint32_t *fb_vaddr = NULL;

  // Get DRM resources
  res = drmModeGetResources(fd);
  if (!res) {
    perror("Failed to get DRM resources");
    goto cleanup;
  }

  // Find connected connector
  for (int i = 0; i < res->count_connectors; i++) {
    conn = drmModeGetConnector(fd, res->connectors[i]);
    if (conn->connection == DRM_MODE_CONNECTED) {
      break;
    } else {
      drmModeFreeConnector(conn);
      conn = NULL;
    }
  }
  if (conn == NULL) {
    perror("No connected display found");
    goto cleanup;
  }
  if (conn->count_modes == 0) {
    perror("No valid mode for connector");
    goto cleanup;
  }

  // Use the first mode
  mode = &conn->modes[0];

  // Store CRTC
  crtc = drmModeGetCrtc(fd, res->crtcs[0]);

  // Create dumb buffer
  if (drmModeCreateDumbBuffer(fd, mode->hdisplay, mode->vdisplay, 32, 0,
                              &fb_handle, &fb_pitch, &fb_size) != 0) {
    perror("Failed to create dumb buffer");
    goto cleanup;
  }

  // Create framebuffer
  if (drmModeAddFB(fd, mode->hdisplay, mode->vdisplay, 24, 32, fb_pitch,
                   fb_handle, &fb_id) != 0) {
    perror("Failed to create framebuffer");
    goto cleanup;
  }

  // Map the buffer
  if (drmModeMapDumbBuffer(fd, fb_handle, &fb_offset) != 0) {
    perror("Failed to map dumb buffer");
    goto cleanup;
  }

  fb_vaddr = (uint32_t *)mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                              fd, fb_offset);
  if (fb_vaddr == MAP_FAILED) {
    perror("Failed to mmap buffer");
    goto cleanup;
  }

  // Clear screen (black)
  memset(fb_vaddr, 0, fb_size);

  // Set display mode
  if (drmModeSetCrtc(fd, crtc->crtc_id, fb_id, 0, 0, &conn->connector_id, 1,
                     mode) != 0) {
    perror("Failed to set mode");
    goto cleanup;
  }

  {
    // clang-format off
    #define vp_illegal(expr) if (expr) { perror("env var VIEWPORT is illegal"); goto cleanup; }
    // clang-format on
    // "w=256,h=256,x=0,y=0", "w=min*0.5,h=min*0.5,x=center,y=center"
    char *vp_env = getenv("VIEWPORT");
    vp_illegal(vp_env == NULL);
    uint16_t min_wh = std::min(mode->hdisplay, mode->vdisplay);
    char *vp_w_env = strstr(vp_env, "w=");
    vp_illegal(vp_w_env == NULL);
    vp_w_env += 2;
    char *vp_h_env = strstr(vp_env, "h=");
    vp_illegal(vp_h_env == NULL);
    vp_h_env += 2;
    char *vp_x_env = strstr(vp_env, "x=");
    vp_illegal(vp_x_env == NULL);
    vp_x_env += 2;
    char *vp_y_env = strstr(vp_env, "y=");
    vp_illegal(vp_y_env == NULL);
    vp_y_env += 2;
    for (char *p = vp_env; *p != '\0'; p++)
      if (*p == ',')
        *p = '\0';
    uint16_t vp_w = strncmp(vp_w_env, "min*", 4) == 0
                        ? atof(vp_w_env + 4) * min_wh
                        : atoi(vp_w_env);
    uint16_t vp_h = strncmp(vp_h_env, "min*", 4) == 0
                        ? atof(vp_h_env + 4) * min_wh
                        : atoi(vp_h_env);
    uint16_t vp_x = strcmp(vp_x_env, "center") == 0
                        ? (mode->hdisplay - vp_w) / 2
                        : atoi(vp_x_env);
    uint16_t vp_y = strcmp(vp_y_env, "center") == 0
                        ? (mode->vdisplay - vp_h) / 2
                        : atoi(vp_y_env);
    printf("vp_w=%d, vp_h=%d, vp_x=%d, vp_y=%d\n", vp_w, vp_h, vp_x, vp_y);
    char *lottie_file_env = getenv("LOTTIE_FILE");
    if (lottie_file_env == NULL) {
      perror("env var LOTTIE_FILE is illegal");
      goto cleanup;
    }
    char *lottie_speed_env = getenv("LOTTIE_SPEED");
    float lottie_speed = lottie_speed_env ? atof(lottie_speed_env) : 1.0;
    char *lottie_loop_env = getenv("LOTTIE_LOOP");
    int lottie_loop = lottie_loop_env ? atoi(lottie_loop_env) : 1;
    printf("lottie_file=%s, lottie_speed=%f\n", lottie_file_env, lottie_speed);

    tvg::Initializer::init(0, tvg::CanvasEngine::Sw);

    std::vector<std::vector<uint32_t>> frames;
    tvg::SwCanvas *canvas = tvg::SwCanvas::gen();
    std::vector<uint32_t> frame(vp_w * vp_h);
    canvas->target(frame.data(), vp_w, vp_w, vp_h, tvg::ColorSpace::ARGB8888);
    tvg::Animation *animation = tvg::Animation::gen();
    tvg::Picture *picture = animation->picture();
    picture->load(lottie_file_env);
    picture->size(vp_w, vp_h);
    canvas->push(picture);
    for (float i = 0, l = animation->totalFrame(); i < l; i += lottie_speed) {
      animation->frame(i);
      canvas->update();
      canvas->draw(true);
      canvas->sync();
      frames.push_back(frame);
    }

    for (int i = 0; i < lottie_loop; i++) {
      for (auto &frame : frames) {
        for (size_t y = 0; y != vp_h; y++)
          memcpy(fb_vaddr + (vp_y + y) * mode->hdisplay + vp_x,
                 frame.data() + y * vp_w, vp_w * sizeof(uint32_t));

        drmVBlank vbl = {};
        vbl.request.type = DRM_VBLANK_RELATIVE;
        vbl.request.sequence = 1;
        if (drmWaitVBlank(fd, &vbl) != 0) {
          perror("Failed to call drmWaitVBlank");
          goto cleanup;
        }
      }
    }

    tvg::Initializer::term();
  }

cleanup:
  if (crtc)
    drmModeSetCrtc(fd, crtc->crtc_id, crtc->buffer_id, crtc->x, crtc->y,
                   &conn->connector_id, 1, &crtc->mode);
  if (crtc)
    drmModeFreeCrtc(crtc);
  if (fb_vaddr)
    munmap(fb_vaddr, fb_size);
  if (fb_id)
    drmModeRmFB(fd, fb_id);
  if (fb_handle)
    drmModeDestroyDumbBuffer(fd, fb_handle);
  if (conn != NULL)
    drmModeFreeConnector(conn);
  if (res)
    drmModeFreeResources(res);
  close(fd);
  return 0;
}
