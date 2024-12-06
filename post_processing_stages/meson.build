posproc_libdir = get_option('prefix') / get_option('libdir') / 'rpicam-apps-postproc'

conf_data = configuration_data()
conf_data.set('POSTPROC_LIB_DIR', '"' + posproc_libdir + '"')
configure_file(output : 'postproc_lib.h', configuration : conf_data)

# JSON (and other assets)
assets_dir = meson.project_source_root() / 'assets'
postproc_assets = []

# Core postprocessing framework files.
rpicam_app_src += files([
    'histogram.cpp',
    'post_processing_stage.cpp',
    'pwl.cpp',
])

# Core postprocessing stages.
core_postproc_src = files([
    'hdr_stage.cpp',
    'motion_detect_stage.cpp',
    'negate_stage.cpp',
])

# Core assets
postproc_assets += files([
    assets_dir / 'hdr.json',
    assets_dir / 'motion_detect.json',
    assets_dir / 'negate.json',
])

core_postproc_lib = shared_module('core-postproc', core_postproc_src,
                                  include_directories : '../',
                                  dependencies : libcamera_dep,
                                  cpp_args : cpp_arguments,
                                  install : true,
                                  install_dir : posproc_libdir,
                                  name_prefix : '',
                                 )

post_processing_headers = files([
    'histogram.hpp',
    'object_detect.hpp',
    'post_processing_stage.hpp',
    'pwl.hpp',
    'segmentation.hpp',
    'tf_stage.hpp',
])

install_headers(post_processing_headers, subdir: meson.project_name() / 'post_processing_stages')
install_data(postproc_assets, install_dir : get_option('datadir') / 'rpi-camera-assets')
