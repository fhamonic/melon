macro(run_conan)
    find_program(conanexecutable "conan")
    if(NOT conanexecutable)
        message(
            FATAL_ERROR
                "Conan package manager is not installed."
                "Check https://docs.conan.io/en/latest/installation.html")
    elseif(NOT EXISTS "${CMAKE_CURRENT_BINARY_DIR}/conan.cmake")
        message(
            STATUS
                "Downloading conan.cmake from https://github.com/conan-io/cmake-conan"
        )
        file(
            DOWNLOAD
            "https://raw.githubusercontent.com/conan-io/cmake-conan/v0.16.1/conan.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/conan.cmake"
            EXPECTED_HASH
                SHA256=396e16d0f5eabdc6a14afddbcfff62a54a7ee75c6da23f32f7a31bc85db23484
            TLS_VERIFY ON)
    endif()

    include(${CMAKE_CURRENT_BINARY_DIR}/conan.cmake)

    conan_cmake_configure(REQUIRES range-v3/0.11.0 gtest/cci.20210126
                          GENERATORS cmake_find_package)

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

endmacro()
