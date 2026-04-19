
source_file = []
pixman_args = []

if host_machine.cpu_family() == 'arm'
  source_file += 'pixman-arm-neon-asm.S'
  pixman_args += '-DLOTTIE_PIXMAN_ARM_NEON=1'
endif

pixman_dep = declare_dependency(
                                 include_directories : include_directories('.'),
                                  sources : source_file,
                                  compile_args : pixman_args
                                 )
