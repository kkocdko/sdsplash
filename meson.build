project('sdsplash', 'cpp',
  default_options : [
    'cpp_std=c++14',
    'b_lto=true' # Enable LTO
  ])

# enable asan
# add_project_arguments('-fsanitize=address', language : ['c', 'cpp'])
# add_project_link_arguments('-fsanitize=address', language : ['c', 'cpp'])

# use system libdrm
drm_dep = dependency('libdrm')

# use subproject thorvg
thorvg_dep = dependency('thorvg',
  static: true,
  fallback: ['thorvg', 'thorvg_dep'], # fallback to subproject
  default_options: [
      'examples=false',
      'loaders=lottie',
      'engines=sw',
      'threads=false',
      'simd=true',
      'extra='
  ]
)

executable('sdsplash', 'sdsplash.cc',
  dependencies: [thorvg_dep, drm_dep],
  install: false)
