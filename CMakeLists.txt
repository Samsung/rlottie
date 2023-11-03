cmake_minimum_required(VERSION 3.3)

# declare project
project(
  rlottie
  VERSION 0.2
  LANGUAGES C CXX ASM)

include(GNUInstallDirs)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE MinSizeRel)
endif()

if(NOT DEFINED BUILD_SHARED_LIBS)
  # Keep the previous behavior of the build system, consistent with Meson.
  set(BUILD_SHARED_LIBS ON)
endif()

# declare target
add_library(rlottie)
set_target_properties(rlottie PROPERTIES DEFINE_SYMBOL RLOTTIE_BUILD)

# declare version of the target
set(PLAYER_VERSION_MAJOR 0)
set(PLAYER_VERSION_MINOR 2)
set(PLAYER_VERSION ${PLAYER_VERSION_MAJOR}.${PLAYER_VERSION_MINOR})
set_target_properties(rlottie PROPERTIES VERSION ${PLAYER_VERSION}
                                         SOVERSION ${PLAYER_VERSION_MAJOR})

# declare alias so that library can be used inside the build tree, e.g. when
# testing
add_library(rlottie::rlottie ALIAS rlottie)

option(LOTTIE_MODULE "Enable LOTTIE MODULE SUPPORT" ON)
option(LOTTIE_THREAD "Enable LOTTIE THREAD SUPPORT" ON)
option(LOTTIE_CACHE "Enable LOTTIE CACHE SUPPORT" ON)
option(LOTTIE_TEST "Build LOTTIE AUTOTESTS" OFF)
option(LOTTIE_CCACHE "Enable LOTTIE ccache SUPPORT" OFF)
option(LOTTIE_ASAN "Compile with asan" OFF)

set(LOTTIE_MODULE_PATH
    "${CMAKE_SHARED_LIBRARY_PREFIX}rlottie-image-loader${CMAKE_SHARED_LIBRARY_SUFFIX}"
    CACHE STRING "Absolute or relative path to dynamic loader plugin.")

configure_file(${CMAKE_CURRENT_LIST_DIR}/cmake/config.h.in config.h)

target_include_directories(
  rlottie
  PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/inc>
  PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")

# declare common target compilation options
if(MSVC)
  target_compile_options(
    rlottie
    PUBLIC
    PRIVATE /std:c++14 /EHs-c- # disable exceptions
            /GR- # disable RTTI
            /W3)
else()
  target_compile_options(
    rlottie
    PUBLIC
    PRIVATE -std=c++14
            -fno-exceptions
            -fno-unwind-tables
            -fno-asynchronous-unwind-tables
            -fno-rtti
            -Wall
            -fvisibility=hidden
            -Wnon-virtual-dtor
            -Woverloaded-virtual
            -Wno-unused-parameter)
endif()

if(WIN32 AND NOT BUILD_SHARED_LIBS)
  target_compile_definitions(rlottie PUBLIC -DRLOTTIE_BUILD=0)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
endif()

# declare dependancy
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads)

target_link_libraries(rlottie PUBLIC "${CMAKE_THREAD_LIBS_INIT}")

if(NOT APPLE AND NOT WIN32)
  target_link_libraries(
    rlottie
    PRIVATE "-Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/rlottie.expmap")
endif()

if(LOTTIE_MODULE)
  # for dlopen, dlsym and dlclose dependancy
  target_link_libraries(rlottie PRIVATE ${CMAKE_DL_LIBS})
endif()

if(NOT LOTTIE_ASAN)
  if(APPLE)
    target_link_libraries(rlottie PUBLIC "-Wl, -undefined error")
  elseif(NOT MSVC)
    target_link_libraries(rlottie PUBLIC "-Wl,--no-undefined")
  endif()
endif()

if(LOTTIE_CCACHE)
  find_program(CCACHE_FOUND ccache)
  if(CCACHE_FOUND)
    message(STATUS "Found ccache")
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
  else()
    message(STATUS "Could NOT find ccache (this is NOT an error)")
  endif()
endif()

if(LOTTIE_ASAN)
  target_compile_options(rlottie PUBLIC -fsanitize=address)
  target_link_options(rlottie PUBLIC -fsanitize=address)
endif()

# declare source and include files
add_subdirectory(inc)
add_subdirectory(src)
add_subdirectory(example)

include(CTest)

if(BUILD_TESTING)
  add_subdirectory(test)
endif()

configure_file(${PROJECT_NAME}.pc.in ${PROJECT_NAME}.pc)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.pc
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

# install header
install(FILES inc/rlottie.h inc/rlottie_capi.h inc/rlottiecommon.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# install lib
install(
  TARGETS rlottie
  EXPORT rlottie-targets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# install config file.

install(
  EXPORT rlottie-targets
  FILE rlottieTargets.cmake
  NAMESPACE rlottie::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rlottie)

# Create a ConfigVersion.cmake file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/rlottieConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY AnyNewerVersion)

configure_package_config_file(
  ${CMAKE_CURRENT_LIST_DIR}/cmake/rlottieConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/rlottieConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rlottie)

# Install the config, configversion and custom find modules
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/rlottieConfig.cmake
              ${CMAKE_CURRENT_BINARY_DIR}/rlottieConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/rlottie)

export(
  EXPORT rlottie-targets
  FILE ${CMAKE_CURRENT_BINARY_DIR}/rlottieTargets.cmake
  NAMESPACE rlottie::)

# Register package in user's package registry
export(PACKAGE rlottie)
