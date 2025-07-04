# CONAN_PROFILE = default
# CONAN_PROFILE = debug
# CONAN_PROFILE = g++-11
# CONAN_PROFILE = clang
CONAN_PROFILE = default_c++26

.PHONY: build package clean

build:
	conan build . -b=missing -pr=${CONAN_PROFILE}

package:
	conan create . -u -b=missing -pr=${CONAN_PROFILE}

clean:
	@rm -rf CMakeUserPresets.json
	@rm -rf build
