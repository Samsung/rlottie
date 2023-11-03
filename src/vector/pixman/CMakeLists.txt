if("${ARCH}" STREQUAL "arm")
  set(CMAKE_ASM_FLAGS "${CFLAGS} -x assembler-with-cpp")
  target_sources(rlottie
                 PRIVATE "${CMAKE_CURRENT_LIST_DIR}/pixman-arm-neon-asm.S")
endif("${ARCH}" STREQUAL "arm")

target_include_directories(rlottie PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
