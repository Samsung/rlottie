
source_file = [
    'lottieloader.cpp',
    'lottiemodel.cpp',
    'lottieproxymodel.cpp',
    'lottieanimation.cpp',
    'lottieitem.cpp',
    'lottieitem_capi.cpp',
    'lottiekeypath.cpp'
]

if get_option('json')
    source_file += ['lottieroparser.cpp']
else
    source_file += ['lottieparser.cpp']
endif


lottie_dep = declare_dependency(
                                  include_directories : include_directories('.'),
                                  sources : source_file
                               )
