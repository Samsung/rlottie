
target_sources(rlottie
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/lottieitem.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/lottieitem_capi.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/lottieloader.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/lottiemodel.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/lottieproxymodel.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/lottieanimation.cpp"
        "${CMAKE_CURRENT_LIST_DIR}/lottiekeypath.cpp"
    )

if (LOTTIE_JSON)
target_sources(rlottie
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/lottieroparser.cpp"
    )
else ()
target_sources(rlottie
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/lottieparser.cpp"
    )
endif()


target_include_directories(rlottie
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}"
    )
