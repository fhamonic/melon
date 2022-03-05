macro(run_conan)
    # Download automatically, you can also just copy the conan.cmake file
    if(NOT EXISTS "${CMAKE_BINARY_DIR}/conan.cmake")
        message(
            STATUS
                "Downloading conan.cmake from https://github.com/conan-io/cmake-conan"
        )
        file(
            DOWNLOAD
            "https://raw.githubusercontent.com/conan-io/cmake-conan/v0.16.1/conan.cmake"
            "${CMAKE_BINARY_DIR}/conan.cmake"
            EXPECTED_HASH
                SHA256=396e16d0f5eabdc6a14afddbcfff62a54a7ee75c6da23f32f7a31bc85db23484
            TLS_VERIFY ON)
    endif()

    # set (ENV{CONAN_REVISIONS_ENABLED} 1)
    include(${CMAKE_BINARY_DIR}/conan.cmake)

    conan_cmake_configure(REQUIRES range-v3/0.11.0 gtest/cci.20210126
                          GENERATORS cmake)

    conan_cmake_autodetect(settings)

    conan_cmake_install(
        PATH_OR_REFERENCE
        .
        BUILD
        missing
        REMOTE
        conancenter
        SETTINGS
        ${settings})

    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake) 
    conan_basic_setup(TARGETS) 

endmacro()
