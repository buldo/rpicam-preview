rpicam_app_src += files([
    'preview.cpp',
])

preview_headers = files([
    'preview.hpp',
])

enable_drm = false
drm_deps = dependency('libdrm', required : get_option('enable_drm'))

if drm_deps.found()
    rpicam_app_dep += drm_deps
    rpicam_app_src += files('drm_preview.cpp')
    cpp_arguments += '-DLIBDRM_PRESENT=1'
    enable_drm = true
endif

enable_egl = false
epoxy_deps = dependency('epoxy', required : get_option('enable_egl'))
gbm_deps = dependency('gbm', required : get_option('enable_egl'))

if epoxy_deps.found() and gbm_deps.found()
    rpicam_app_dep += [epoxy_deps, gbm_deps]
    rpicam_app_src += files('egl_preview.cpp')
    cpp_arguments += '-DLIBEGL_PRESENT=1'
    enable_egl = true
endif

install_headers(preview_headers, subdir: meson.project_name() / 'preview')
