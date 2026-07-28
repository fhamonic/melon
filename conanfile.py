import os
import re
from conan import ConanFile
from conan.tools.files import copy, load
from conan.tools.cmake import cmake_layout, CMake
from conan.tools.build import check_min_cppstd


class MelonConan(ConanFile):
    name = "melon"

    license = "BSL-1.0"
    description = (
        "A modern and efficient graph library using C++20 ranges and concepts."
    )
    homepage = "https://github.com/fhamonic/melon"
    url = "https://github.com/fhamonic/melon.git"

    settings = "os", "arch", "compiler", "build_type"
    package_type = "header-library"
    exports_sources = (
        "include/*",
        "cmake/*",
        "CMakeLists.txt",
        "test/*",
        "LICENSE",
    )
    no_copy_source = True
    generators = "CMakeToolchain", "CMakeDeps"

    def set_version(self):
        # include/melon/version.hpp is the single source of truth for the
        # version number (CMakeLists.txt parses it too).
        version_hpp = load(
            self,
            os.path.join(self.recipe_folder, "include", "melon", "version.hpp"),
        )
        components = {
            level: re.search(
                rf"#define MELON_VERSION_{level} (\d+)", version_hpp
            ).group(1)
            for level in ("MAJOR", "MINOR", "PATCH")
        }
        self.version = "{MAJOR}.{MINOR}.{PATCH}".format(**components)

    def requirements(self):
        self.test_requires("gtest/[>=1.10.0 <cci]")
        # self.test_requires("mppp/1.0.3")

    def validate(self):
        check_min_cppstd(self, 23)

    def layout(self):
        cmake_layout(self)

    def build(self):
        if not self.conf.get("tools.build:skip_test", default=False):
            cmake = CMake(self)
            cmake.configure(
                variables={
                    "MELON_BUILD_TESTS": "ON",
                    "MELON_FROM_CONAN": "ON",
                },
            )
            cmake.build()
            cmake.test(cli_args=["CTEST_OUTPUT_ON_FAILURE=1"])

    def package(self):
        copy(
            self,
            "LICENSE",
            self.source_folder,
            os.path.join(self.package_folder, "licenses"),
        )
        copy(
            self,
            "*.hpp",
            os.path.join(self.source_folder, "include"),
            os.path.join(self.package_folder, "include"),
            # melon/experimental/ ships, but the two headers that are still
            # unfinished (they do not compile) are held back; see their
            # file-level comments.
            excludes=(
                "melon/experimental/scapegoat_tree.hpp",
                "melon/experimental/doubly_connected_digraph.hpp",
            ),
        )

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []

    def package_id(self):
        self.info.clear()
