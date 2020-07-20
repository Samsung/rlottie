
override_default = ['warning_level=2', 'werror=false']

common_source  = files('evasapp.cpp')
common_source += files('lottieview.cpp')

demo_sources = files('demo.cpp')
demo_sources += common_source

executable('lottie2gif',
           'lottie2gif.cpp',
           include_directories : inc,
           override_options : override_default,
           link_with : rlottie_lib)

demo_dep = dependency('elementary', required : false, disabler : true)

executable('demo',
           demo_sources,
           include_directories : inc,
           override_options : override_default,
           link_with : rlottie_lib,
           dependencies : demo_dep)


demo_marker_sources = files('demo_marker.cpp')
demo_marker_sources += common_source

executable('demo_marker',
           demo_marker_sources,
           include_directories : inc,
           override_options : override_default,
           link_with : rlottie_lib,
           dependencies : demo_dep)

lottieview_test_src  = files('lottieviewtest.cpp')
lottieview_test_src += common_source

executable('lottieviewTest',
           lottieview_test_src,
           include_directories : inc,
           override_options : override_default,
           link_with : rlottie_lib,
           dependencies : demo_dep)

uxsample_test_src  = files('uxsampletest.cpp')
uxsample_test_src += common_source

executable('uxsampleTest',
           uxsample_test_src,
           include_directories : inc,
           override_options : override_default,
           link_with : rlottie_lib,
           dependencies : demo_dep)

lottieviewer_sources = files('lottieviewer.cpp')
lottieviewer_sources += common_source

executable('lottieviewer',
           lottieviewer_sources,
           include_directories : inc,
           override_options : override_default,
           link_with : rlottie_lib,
           dependencies : demo_dep)
