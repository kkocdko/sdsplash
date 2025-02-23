project('sdsplash', 'cpp',
  default_options : [
    'cpp_std=c++14',
    'b_lto=true' # Enable LTO
  ])

# --- Find libdrm ---
drm_dep = dependency('libdrm',
  static: true, # Request static linking for thorvg
  fallback: ['drm', 'drm_dep'],  # Fallback to subproject
)

# --- Find ThorVG, with fallback to subproject ---
thorvg_dep = dependency('thorvg',
  static: true, # Request static linking for thorvg
  fallback: ['thorvg', 'thorvg_dep'],  # Fallback to subproject
  default_options: [
      'examples=false',
      'loaders=lottie',
      'engines=sw',
      'threads=false',
      'simd=true',
      #'file=false',
      'extra='  # Or 'extra=[]' if it's a list option in ThorVG
  ]
)

# enable asan
#add_project_arguments('-fsanitize=address', language : ['c', 'cpp'])
#add_project_link_arguments('-fsanitize=address', language : ['c', 'cpp'])

# todo: add include dir ./subprojects/thorvg/src/loaders/lottie
incdir_thorvg_lottie = include_directories('./subprojects/thorvg/src/loaders/lottie')
incdir_thorvg_renderer = include_directories('./subprojects/thorvg/src/renderer')


# --- Create the executable ---
executable('sdsplash', 'sdsplash.cc',
  dependencies: [thorvg_dep, drm_dep],
  include_directories: [incdir_thorvg_lottie, incdir_thorvg_renderer],
  install: false)
