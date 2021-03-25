add_executable(lottie2gif "lottie2gif.cpp")

if(MSVC)
    target_compile_options(lottie2gif
                           PRIVATE
                           /std:c++14)
else()
    target_compile_options(lottie2gif
                           PRIVATE
                           -std=c++14)
endif()

target_link_libraries(lottie2gif rlottie)

target_include_directories(lottie2gif
                           PRIVATE
                           "${CMAKE_CURRENT_LIST_DIR}/../inc/")
