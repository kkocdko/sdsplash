#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <vector>

#include "xf86drm.h"
#include "xf86drmMode.h"

#include "thorvg.h"

#include "rapidjson/document.h"

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
    tvg::Initializer::init(0, tvg::CanvasEngine::Sw);

    std::vector<std::vector<uint32_t>> frames;

    size_t v_w = 400;
    size_t v_h = 400;
    std::vector<uint32_t> frame(v_w * v_h);

    auto canvas = tvg::SwCanvas::gen();
    canvas->target(frame.data(), v_w, v_w, v_h, tvg::ColorSpace::ARGB8888);

    auto animation = tvg::Animation::gen();
    auto picture = animation->picture();
    picture->load("lottie.json");
    picture->size(400, 400);
    canvas->push(picture);

    for (size_t i = 0, l = animation->totalFrame(); i < l; i++) {
      memset(frame.data(), 0, frame.size() * sizeof(uint32_t));
      animation->frame(i);
      canvas->update();
      canvas->draw();
      canvas->sync();
      frames.push_back(frame);
    }

    for (uint32_t i = 0; i < animation->totalFrame(); i++) {
      for (size_t y = 0; y != v_h; y++) {
        memcpy(fb_vaddr + y * mode->hdisplay, frames[i].data() + y * v_w,
               v_w * sizeof(uint32_t));
      }

      drmVBlank vbl = {};
      vbl.request.type = DRM_VBLANK_RELATIVE;
      vbl.request.sequence = 1;
      if (drmWaitVBlank(fd, &vbl) != 0) {
        perror("Failed to call drmWaitVBlank");
        goto cleanup;
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

// goals:
// * apng or webp or gif or lottie or rive support
// * use linux simpledrm
// * lightweight and high fps

// https://gitlab.freedesktop.org/plymouth/plymouth
// https://gitlab.freedesktop.org/mesa/drm
// https://github.com/Samsung/rlottie
// https://github.com/dvdhrm/docs
// https://gitlab.freedesktop.org/plymouth/plymouth/-/archive/main/plymouth-main.tar.gz
// https://wiki.archlinux.org/title/Plymouth
// https://wiki.archlinux.org/title/Silent_boot
// https://wiki.archlinux.org/title/Dynamic_Kernel_Module_Support
// https://wiki.archlinux.org/title/Kernel_mode_setting
// https://www.kernel.org/doc/html/v6.12/gpu/drm-internals.html
// https://www.kernel.org/doc/html/v6.12/gpu/drm-uapi.html
// https://fedoraproject.org/wiki/Changes/ReplaceFbdevDrivers
// https://blog.csdn.net/fengchaochao123/article/details/135262216
// https://docs.nvidia.com/jetson/l4t-multimedia/group__direct__rendering__manager.html

// gcc sdsplash.c -o sdsplash -I/usr/include/drm -ldrm -Wall -Wextra -g -Og
// -fsanitize=address,undefined -fno-omit-frame-pointer
