#include <atomic>
#include <fcntl.h>
#include <signal.h>
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

struct Defer { // use like golang's defer
  std::function<void()> cleanup;
  explicit Defer(std::function<void()> func) : cleanup(std::move(func)) {}
  ~Defer() { cleanup(); }
  Defer(const Defer &) = delete; // avoid double cleanup
  Defer &operator=(const Defer &) = delete;
};

static std::atomic<bool> loop_finished{false};

static void signal_handler(int signal) {
  if (signal == SIGINT) {
    loop_finished = true;
    printf("Received SIGINT, finishing loop\n");
  }
}

int main(int argc, char *argv[]) {
  char *arg_file = NULL;
  float arg_speed = 1.0;
  int arg_loop = 1;
  char *arg_viewport = NULL;
  for (int i = 1; i + 1 < argc; i += 2) {
    if (strcmp("--file", argv[i]) == 0) {
      arg_file = argv[i + 1];
    } else if (strcmp("--speed", argv[i]) == 0) {
      arg_speed = atof(argv[i + 1]);
    } else if (strcmp("--loop", argv[i]) == 0) {
      arg_loop = atoi(argv[i + 1]);
    } else if (strcmp("--viewport", argv[i]) == 0) {
      arg_viewport = argv[i + 1];
    } else {
      printf("unknown argument: %s\n", argv[i]);
      return 1;
    }
  }
  if (arg_file == NULL) {
    printf("usage: sdsplash --file lottie.json [options]\n\n"
           "options:\n"
           "  --speed 1.0  # from 0.001 to 999.0\n"
           "  --loop 1     # from 1 to 999999\n"
           "  --viewport   'w=min*0.5,h=min*0.5,x=center,y=center'\n"
           "               'w=min*0.2,h=min*0.2,x=center,y=96'\n"
           "               'w=256,h=256,x=0,y=0'\n");
    return 1;
  }

  // todo: handle SIGINT to exit two-way playing

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
  Defer defer_fd([&]() { close(fd); });

  drmModeRes *res = drmModeGetResources(fd);
  if (!res) {
    perror("Failed to get DRM resources");
    return 1;
  }
  Defer defer_res([&]() { drmModeFreeResources(res); });

  drmModeConnector *conn = NULL;
  for (int i = 0; i < res->count_connectors; i++) {
    conn = drmModeGetConnector(fd, res->connectors[i]);
    if (conn->connection == DRM_MODE_CONNECTED)
      break;
    drmModeFreeConnector(conn);
    conn = NULL;
  }
  if (!conn) {
    perror("No connected display found");
    return 1;
  }
  Defer defer_conn([&]() { drmModeFreeConnector(conn); });
  if (conn->count_modes == 0) {
    perror("No valid mode for connector");
    return 1;
  }
  drmModeModeInfo *mode = &conn->modes[0]; // Use the first mode

  // Store CRTC
  if (res->count_crtcs == 0) {
    perror("No valid CRTC for connector");
    return 1;
  }
  drmModeCrtc *crtc = drmModeGetCrtc(fd, res->crtcs[0]);
  if (!crtc) {
    perror("Failed to get CRTC");
    return 1;
  }
  Defer defer_crtc([&]() {
    // Restore to origin CRTC
    drmModeSetCrtc(fd, crtc->crtc_id, crtc->buffer_id, crtc->x, crtc->y,
                   &conn->connector_id, 1, &crtc->mode);
    drmModeFreeCrtc(crtc);
  });

  // Create dumb buffer
  uint32_t fb_handle = 0;
  uint32_t fb_pitch = 0;
  uint64_t fb_size = 0;
  if (drmModeCreateDumbBuffer(fd, mode->hdisplay, mode->vdisplay, 32, 0,
                              &fb_handle, &fb_pitch, &fb_size) != 0) {
    perror("Failed to create dumb buffer");
    return 1;
  }
  Defer defer_fb_handle([&]() { drmModeDestroyDumbBuffer(fd, fb_handle); });

  // Create framebuffer
  uint32_t fb_id = 0;
  if (drmModeAddFB(fd, mode->hdisplay, mode->vdisplay, 24, 32, fb_pitch,
                   fb_handle, &fb_id) != 0) {
    perror("Failed to create framebuffer");
    return 1;
  }
  Defer defer_fb_id([&]() { drmModeRmFB(fd, fb_id); });

  // Map the buffer
  uint64_t fb_offset = 0;
  if (drmModeMapDumbBuffer(fd, fb_handle, &fb_offset) != 0) {
    perror("Failed to map dumb buffer");
    return 1;
  }

  uint32_t *fb_vaddr = (uint32_t *)mmap(0, fb_size, PROT_READ | PROT_WRITE,
                                        MAP_SHARED, fd, fb_offset);
  if (fb_vaddr == MAP_FAILED) {
    perror("Failed to mmap buffer");
    return 1;
  }
  Defer defer_fb_vaddr([&]() { munmap(fb_vaddr, fb_size); });

  // Clear screen (black)
  memset(fb_vaddr, 0, fb_size);

  // Set display mode
  if (drmModeSetCrtc(fd, crtc->crtc_id, fb_id, 0, 0, &conn->connector_id, 1,
                     mode) != 0) {
    perror("Failed to set mode");
    return 1;
  }

  if (!arg_viewport)
    arg_viewport = strdup("w=min*0.5,h=min*0.5,x=center,y=center"); // leaking
  uint16_t min_wh = std::min(mode->hdisplay, mode->vdisplay);
  char *vp_w_s = strstr(arg_viewport, "w=");
  char *vp_h_s = strstr(arg_viewport, "h=");
  char *vp_x_s = strstr(arg_viewport, "x=");
  char *vp_y_s = strstr(arg_viewport, "y=");
  if (!vp_w_s || !vp_h_s || !vp_x_s || !vp_y_s) {
    printf("Illegal viewport\n");
    return 1;
  }
  vp_w_s += 2, vp_h_s += 2, vp_x_s += 2, vp_y_s += 2;
  for (char *p = arg_viewport; *p != '\0'; p++)
    if (*p == ',')
      *p = '\0';
  uint16_t vp_w = strncmp(vp_w_s, "min*", 4) == 0 ? atof(vp_w_s + 4) * min_wh
                                                  : atoi(vp_w_s);
  uint16_t vp_h = strncmp(vp_h_s, "min*", 4) == 0 ? atof(vp_h_s + 4) * min_wh
                                                  : atoi(vp_h_s);
  uint16_t vp_x = strcmp(vp_x_s, "center") == 0 ? (mode->hdisplay - vp_w) / 2
                                                : atoi(vp_x_s);
  uint16_t vp_y = strcmp(vp_y_s, "center") == 0 ? (mode->vdisplay - vp_h) / 2
                                                : atoi(vp_y_s);
  printf("vp_w=%d, vp_h=%d, vp_x=%d, vp_y=%d\n", vp_w, vp_h, vp_x, vp_y);

  tvg::Initializer::init(0, tvg::CanvasEngine::Sw);
  Defer defer_tvg([&]() { tvg::Initializer::term(); });

  std::vector<std::vector<uint32_t>> frames;
  tvg::SwCanvas *canvas = tvg::SwCanvas::gen();
  std::vector<uint32_t> frame(vp_w * vp_h);
  canvas->target(frame.data(), vp_w, vp_w, vp_h, tvg::ColorSpace::ARGB8888);
  tvg::Animation *animation = tvg::Animation::gen();
  tvg::Picture *picture = animation->picture();
  picture->load(arg_file);
  picture->size(vp_w, vp_h);
  canvas->push(picture);
  for (float i = 0, l = animation->totalFrame(); i < l; i += arg_speed) {
    animation->frame(i);
    canvas->update();
    canvas->draw(true);
    canvas->sync();
    frames.push_back(frame);
  }

  signal(SIGINT, signal_handler);
  for (int i = 0; i < arg_loop && !loop_finished; i++) {
    for (auto &frame : frames) {
      for (size_t y = 0; y != vp_h; y++)
        memcpy(fb_vaddr + (vp_y + y) * mode->hdisplay + vp_x,
               frame.data() + y * vp_w, vp_w * sizeof(uint32_t));

      drmVBlank vbl = {};
      vbl.request.type = DRM_VBLANK_RELATIVE;
      vbl.request.sequence = 1;
      if (drmWaitVBlank(fd, &vbl) != 0) {
        perror("Failed to call drmWaitVBlank");
        return 1;
      }
    }
  }

  return 0;
}
