string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" RLOTTIE_SYSTEM_PROCESSOR)
string(TOLOWER "${ARCH}" RLOTTIE_TARGET_ARCH)

if (RLOTTIE_TARGET_ARCH STREQUAL "arm" OR
    (RLOTTIE_SYSTEM_PROCESSOR MATCHES "^arm" AND
     NOT RLOTTIE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$"))
    set(CMAKE_ASM_FLAGS "${CFLAGS} -x assembler-with-cpp")
    target_sources(rlottie
        PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/pixman-arm-neon-asm.S"
        )
    target_compile_definitions(rlottie PRIVATE LOTTIE_PIXMAN_ARM_NEON=1)
endif()

target_include_directories(rlottie
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}"
    )
