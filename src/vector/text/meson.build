deps = []

if get_option('ext-font') == true
    deps += [dependency('freetype2'), dependency('fontconfig'), dependency('harfbuzz')]
endif

source_file = 'vtextshaper.cpp'

text_dep = declare_dependency(include_directories : include_directories('.'),
                              sources : source_file,
                              dependencies : deps
                              )
