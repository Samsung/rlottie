source_file = []
deps = []

if get_option('ext-font') == true
    deps += [dependency('freetype2'), dependency('fontconfig'), dependency('harfbuzz')]

    source_file += ['vtextshaper.cpp', 'vfont.cpp']
endif

text_dep = declare_dependency(
                                 include_directories : include_directories('.'),
                                  sources : source_file,
                                  dependencies : deps
                                 )
