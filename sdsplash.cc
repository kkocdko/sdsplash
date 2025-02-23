#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <rlottie.h>

#include <vector>

int main() {
  // auto view = rlottie::
  auto player = rlottie::Animation::loadFromFile("lottie.json");
  // rlottie::Animation()
  // printf("frameRate=%f\n", player->frameRate());

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
  drmModeRes *drm_resources = NULL;
  drmModeConnector *drm_connector = NULL;
  drmModeModeInfo *drm_mode = NULL;
  drmModeCrtc *drm_crtc = NULL;
  uint32_t fb_handle = 0;
  uint32_t fb_pitch = 0;
  uint64_t fb_size = 0;
  uint32_t fb_id = 0;
  uint64_t fb_offset = 0;
  uint32_t *fb_vaddr = NULL;
  struct timespec t1 = {}, t2 = {};

  // Get DRM resources
  drm_resources = drmModeGetResources(fd);
  if (!drm_resources) {
    perror("Failed to get DRM resources");
    goto cleanup;
  }

  // Find connected connector
  for (int i = 0; i < drm_resources->count_connectors; i++) {
    drm_connector = drmModeGetConnector(fd, drm_resources->connectors[i]);
    if (drm_connector->connection == DRM_MODE_CONNECTED) {
      break;
    } else {
      drmModeFreeConnector(drm_connector);
      drm_connector = NULL;
    }
  }
  if (drm_connector == NULL) {
    perror("No connected display found");
    goto cleanup;
  }
  if (drm_connector->count_modes == 0) {
    perror("No valid mode for connector");
    goto cleanup;
  }

  // Use the first mode
  drm_mode = &drm_connector->modes[0];

  // Store CRTC
  drm_crtc = drmModeGetCrtc(fd, drm_resources->crtcs[0]);

  // Create dumb buffer
  if (drmModeCreateDumbBuffer(fd, drm_mode->hdisplay, drm_mode->vdisplay, 32, 0,
                              &fb_handle, &fb_pitch, &fb_size) != 0) {
    perror("Failed to create dumb buffer");
    goto cleanup;
  }

  // Create framebuffer
  if (drmModeAddFB(fd, drm_mode->hdisplay, drm_mode->vdisplay, 24, 32, fb_pitch,
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
  if (drmModeSetCrtc(fd, drm_crtc->crtc_id, fb_id, 0, 0,
                     &drm_connector->connector_id, 1, drm_mode) != 0) {
    perror("Failed to set mode");
    goto cleanup;
  }

  printf("totalFrame=%ld\n", player->totalFrame());
  {
     clock_gettime(CLOCK_MONOTONIC, &t1);
 
    std::vector<uint32_t> frame(drm_mode->hdisplay * drm_mode->vdisplay);
    rlottie::Surface surface(frame.data(), drm_mode->hdisplay,
                             drm_mode->vdisplay,
                             drm_mode->hdisplay * sizeof(uint32_t));
    surface.setDrawRegion(0, 0, 400, 400);
    std::vector<std::vector<uint32_t>> frames;
    for (size_t i = 0, l = player->totalFrame(); i < l; i++) {
      player->renderSync(i, surface);
      frames.push_back(frame);
    }

      clock_gettime(CLOCK_MONOTONIC, &t2);
    long long elapsed_ns =
        1000000000ll * (t2.tv_sec - t1.tv_sec) + (t2.tv_nsec - t1.tv_nsec);
  printf("elapsed_ms=%lld\n", elapsed_ns/1000000);
// for(int jj=0;jj<9;jj++){
//     for (size_t i = 0, l = player->totalFrame(); i < l; i++) {
//       memcpy((void *)fb_vaddr, frames[i].data(), fb_size);

//       // usleep(1000000 / 15);

//       drmVBlank vbl = {};
//       vbl.request.type = DRM_VBLANK_RELATIVE;
//       vbl.request.sequence = 1;
//       if (drmWaitVBlank(fd, &vbl) != 0) {
//         perror("Failed to call drmWaitVBlank");
//         goto cleanup;
//       }
//     }
// }
  }
  // clock_gettime(CLOCK_MONOTONIC, &t1);
  // for (uint32_t i = 0;; i++) {
  //   clock_gettime(CLOCK_MONOTONIC, &t2);
  //   long long elapsed_ns =
  //       1000000000ll * (t2.tv_sec - t1.tv_sec) + (t2.tv_nsec - t1.tv_nsec);
  //   if (elapsed_ns > 3000000000ll) {
  //     break;
  //   }

  //   for (uint32_t y = 100; y < 100 + 320; y++) {
  //     for (uint32_t x = 100; x < 100 + 240; x++) {
  //       uint32_t c = ((i + x) * 2 & 0xff) + (((i + y) * 2 & 0xff) << 8);
  //       fb_vaddr[y * (fb_pitch / sizeof(uint32_t)) + x] = c;
  //     }
  //   }

  //   drmVBlank vbl = {};
  //   vbl.request.type = DRM_VBLANK_RELATIVE;
  //   vbl.request.sequence = 1;
  //   if (drmWaitVBlank(fd, &vbl) != 0) {
  //     perror("Failed to call drmWaitVBlank");
  //     goto cleanup;
  //   }
  // }

cleanup:
  if (drm_crtc)
    drmModeSetCrtc(fd, drm_crtc->crtc_id, drm_crtc->buffer_id, drm_crtc->x,
                   drm_crtc->y, &drm_connector->connector_id, 1,
                   &drm_crtc->mode);
  if (drm_crtc)
    drmModeFreeCrtc(drm_crtc);
  if (fb_vaddr)
    munmap(fb_vaddr, fb_size);
  if (fb_id)
    drmModeRmFB(fd, fb_id);
  if (fb_handle)
    drmModeDestroyDumbBuffer(fd, fb_handle);
  if (drm_connector != NULL)
    drmModeFreeConnector(drm_connector);
  if (drm_resources)
    drmModeFreeResources(drm_resources);
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
