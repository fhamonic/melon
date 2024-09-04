# CONAN_PROFILE = clang
CONAN_PROFILE = default
# CONAN_PROFILE = debug

.PHONY: build package clean

build:
	conan build . -b=missing -pr=${CONAN_PROFILE}

package:
	conan create . -u -b=missing -pr=${CONAN_PROFILE}
	
clean:
	@rm -rf CMakeUserPresets.json
	@rm -rf build
