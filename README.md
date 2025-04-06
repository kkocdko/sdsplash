# sdsplash

- lightweight and high fps
- use linux simpledrm
- multi thread soft render

```sh
printf -- 'CompileFlags:\n  Add:\n    - "-I/lib/gcc/x86_64-linux-gnu/14/include"\n' > .clangd

# begin
rm -rf subprojects
mkdir -p subprojects/thorvg
curl -L https://github.com/thorvg/thorvg/archive/44acb11fdc8304543b6c247fa4cddacf9ff6b13f.tar.gz | tar -xz --strip-components 1 -C subprojects/thorvg
rm -rf subprojects/thorvg/examples subprojects/thorvg/res

# todo: try skia skottie
rm -rf skia
mkdir -p skia
[ ! -e skia.tar.gz ] && curl -o skia.tar.gz -L https://github.com/google/skia/archive/f2fc833dee5447ddc7b3b370d8a639bf967fa854.tar.gz
cat skia.tar.gz | tar -xz --strip-components 1 -C skia
rm -rf skia/resources skia/site skia/tests skia/demos.skia.org

# https://github.com/google/skia/archive/f2fc833dee5447ddc7b3b370d8a639bf967fa854.tar.gz
```

- https://gitlab.freedesktop.org/plymouth/plymouth
- https://gitlab.freedesktop.org/mesa/drm
- https://github.com/Samsung/rlottie
- https://github.com/dvdhrm/docs

- https://wiki.archlinux.org/title/Plymouth
- https://wiki.archlinux.org/title/Silent_boot
- https://wiki.archlinux.org/title/Dynamic_Kernel_Module_Support
- https://wiki.archlinux.org/title/Kernel_mode_setting
- https://www.kernel.org/doc/html/v6.12/gpu/drm-internals.html
- https://www.kernel.org/doc/html/v6.12/gpu/drm-uapi.html
- https://fedoraproject.org/wiki/Changes/ReplaceFbdevDrivers
- https://blog.csdn.net/fengchaochao123/article/details/135262216
- https://docs.nvidia.com/jetson/l4t-multimedia/group__direct__rendering__manager.html
