import os
import re
from conan import ConanFile
from conan.errors import ConanException
from conan.tools.files import copy, load
from conan.tools.cmake import cmake_layout, CMake
from conan.tools.build import check_min_cppstd

required_conan_version = ">=2.0"


class MelonConan(ConanFile):
    name = "melon"

    license = "BSL-1.0"
    # Same sentence as the CMake project() DESCRIPTION, so the two cannot
    # drift; this string is what Conan Center displays.
    description = "Modern and Efficient Library for Optimization in Networks."
    topics = ("graph", "header-only", "cpp23", "algorithms")
    homepage = "https://github.com/fhamonic/melon"
    url = "https://github.com/fhamonic/melon"

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
        components = {}
        for level in ("MAJOR", "MINOR", "PATCH"):
            match = re.search(
                rf"#define MELON_VERSION_{level} (\d+)", version_hpp
            )
            if match is None:
                # Mirrors the FATAL_ERROR in CMakeLists.txt; a bare
                # AttributeError here would not name the real problem.
                raise ConanException(
                    f"Failed to parse MELON_VERSION_{level} from version.hpp"
                )
            components[level] = match.group(1)
        self.version = "{MAJOR}.{MINOR}.{PATCH}".format(**components)

    def build_requirements(self):
        # Ungated, the test_requires makes skip_test builds fail on a
        # missing gtest binary that build() would never use.
        if not self.conf.get("tools.build:skip_test", default=False):
            self.test_requires("gtest/[>=1.10.0 <cci]")

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
            # Not cmake.test(cli_args=["CTEST_OUTPUT_ON_FAILURE=1"]): that
            # token reaches the native tool, and while make exports it to
            # ctest as an environment variable, MSBuild rejects a bare
            # NAME=VALUE argument and the MSVC job fails before testing.
            cmake.ctest(cli_args=["--output-on-failure"])

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
            # melon/experimental/ ships, but two headers are still unfinished
            excludes=(
                "melon/experimental/scapegoat_tree.hpp",
                "melon/experimental/doubly_connected_digraph.hpp",
            ),
        )

    def package_info(self):
        # The names melonConfig.cmake exports; CMakeDeps' defaults happen
        # to coincide today, but only the explicit properties keep a rename
        # on either side from silently changing what consumers link.
        self.cpp_info.set_property("cmake_file_name", "melon")
        self.cpp_info.set_property("cmake_target_name", "melon::melon")
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []

    def package_id(self):
        self.info.clear()
