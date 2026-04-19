function(rlottie_configure_example target)
    if(MSVC)
        target_compile_options(${target}
                               PRIVATE
                               /std:c++14)
    else()
        target_compile_options(${target}
                               PRIVATE
                               -std=c++14)
    endif()

    target_link_libraries(${target} rlottie)

    target_include_directories(${target}
                               PRIVATE
                               "${CMAKE_CURRENT_LIST_DIR}/../inc/")
endfunction()

add_executable(lottie2gif "lottie2gif.cpp")
rlottie_configure_example(lottie2gif)

if (NOT WIN32)
    add_executable(lottieperf "lottieperf.cpp")
    rlottie_configure_example(lottieperf)
    target_compile_definitions(lottieperf
                               PRIVATE
                               DEMO_DIR="${CMAKE_SOURCE_DIR}/example/resource/")

    add_executable(lottiebench "lottiebench.cpp")
    rlottie_configure_example(lottiebench)
    target_compile_definitions(lottiebench
                               PRIVATE
                               DEMO_DIR="${CMAKE_SOURCE_DIR}/example/resource/")

    find_path(THORVG_INCLUDE_DIR
              NAMES thorvg.h
              HINTS
              "${CMAKE_SOURCE_DIR}/../thorvg/inc")
    find_library(THORVG_LIBRARY
                 NAMES thorvg-1.1 thorvg
                 HINTS
                 "${CMAKE_SOURCE_DIR}/../thorvg/build/src"
                 "${CMAKE_SOURCE_DIR}/../thorvg/builddir/src"
                 "${CMAKE_SOURCE_DIR}/../thorvg/build-tspan/src")
    if (THORVG_INCLUDE_DIR AND THORVG_LIBRARY)
        get_filename_component(THORVG_LIBRARY_DIR "${THORVG_LIBRARY}" DIRECTORY)
        add_executable(thorvgbench "thorvgbench.cpp")
        if(MSVC)
            target_compile_options(thorvgbench
                                   PRIVATE
                                   /std:c++14)
        else()
            target_compile_options(thorvgbench
                                   PRIVATE
                                   -std=c++14)
        endif()
        target_include_directories(thorvgbench
                                   PRIVATE
                                   "${THORVG_INCLUDE_DIR}")
        target_link_libraries(thorvgbench
                              PRIVATE
                              "${THORVG_LIBRARY}")
        set_target_properties(thorvgbench
                              PROPERTIES
                              BUILD_RPATH "${THORVG_LIBRARY_DIR}")
    endif()
endif()
