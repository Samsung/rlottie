if(LOTTIE_MODULE)
    add_library(rlottie-image-loader SHARED
                stb_image.cpp
               )
    target_compile_options(rlottie-image-loader PRIVATE
                           -fvisibility=hidden
                          )

    get_filename_component(LOTTIE_MODULE_FILENAME ${LOTTIE_MODULE_PATH} NAME)
    get_filename_component(LOTTIE_MODULE_DIR ${LOTTIE_MODULE_PATH} DIRECTORY)
    if (NOT LOTTIE_MODULE_DIR)
        set(LOTTIE_MODULE_DIR ${LIB_INSTALL_DIR})
    endif()

    set_target_properties(rlottie-image-loader PROPERTIES
                          DEFINE_SYMBOL LOT_BUILD
                          PREFIX ""
                          SUFFIX ""
                          OUTPUT_NAME ${LOTTIE_MODULE_FILENAME}
                         )
    install(TARGETS rlottie-image-loader
            LIBRARY DESTINATION ${LOTTIE_MODULE_DIR}
           )
else()
    target_sources(rlottie
        PRIVATE
            "${CMAKE_CURRENT_LIST_DIR}/stb_image.cpp"
        )
endif()
