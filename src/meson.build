compiler_flags = ['-DRLOTTIE_BUILD']

cc = meson.get_compiler('cpp')
if (cc.get_id() != 'msvc')
    compiler_flags += ['-fno-exceptions', '-fno-rtti',
                       '-fno-unwind-tables' , '-fno-asynchronous-unwind-tables',
                       '-Woverloaded-virtual', '-Wno-unused-parameter']
endif

linker_flags = []
if (host_machine.system() not in ['darwin', 'windows'])
    linker_flags += ['-Wl,--version-script=@0@/../rlottie.expmap'.format(meson.current_source_dir())]
endif

subdir('vector')
subdir('lottie')
subdir('binding')

rlottie_lib_dep  = [ vector_dep, lottie_dep, binding_dep]

if get_option('thread') == true
    rlottie_lib_dep  += dependency('threads')
endif

rlottie_lib = library('rlottie',
                      include_directories   : inc,
                      version               : meson.project_version(),
                      dependencies          : rlottie_lib_dep,
                      install               : true,
                      cpp_args              : compiler_flags,
                      link_args             : linker_flags,
                      gnu_symbol_visibility : 'hidden',
                     )

# Make rlottie library usable as a Meson subproject.
rlottie_dep = declare_dependency(
                                 include_directories: inc,
                                 link_with : rlottie_lib)

if (cc.get_id() == 'emscripten')

  subdir('wasm')

  executable('rlottie-wasm',
              [],
              dependencies : [rlottie_dep, rlottie_wasm_dep],
            )
endif

pkg_mod = import('pkgconfig')

pkg_mod.generate( libraries   : rlottie_lib,
                  version     : meson.project_version(),
                  name        : 'librlottie',
                  filebase    : 'rlottie',
                  description : 'A Library for rendering lottie files.'
                )
