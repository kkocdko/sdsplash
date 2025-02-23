# sdsplash

```json
{
  "clangd.fallbackFlags": [
    "-I",
    "/lib/gcc/x86_64-redhat-linux/14/include",
    "-I",
    "/usr/include/drm"
  ]
}
```

- Double buffered.
- Lottie using rlottie.
- https://github.com/Samsung/rlottie/archive/e3026b1e1a516fff3c22d2b1b9f26ec864f89a82.tar.gz
- multi thread soft render
- LOTTIE_THREAD_SUPPORT = off
- bench: rlottie = 1439ms
- https://github.com/Samsung/rlottie/issues/551

```
cmake -B build -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=True && cmake --build build

meson setup build
meson setup builddir -Dloaders="lottie" -Dextra="" --reconfigure

meson setup builddir
meson compile -C builddir

rm -rf build ; meson setup --reconfigure build -Dbuildtype=release
meson compile -C build
```
