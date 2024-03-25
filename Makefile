BUILD_DIR = build
CONAN_PROFILE = default
# CONAN_PROFILE = debug

.PHONY: test package clean

$(BUILD_DIR):
	conan build . -of=${BUILD_DIR} -b=missing -pr=${CONAN_PROFILE}

debug:
	conan build . -of=${BUILD_DIR} -b=missing -pr=${CONAN_PROFILE}

package:
	conan create . -u -b=missing
	
clean:
	@rm -rf CMakeUserPresets.json
	@rm -rf $(BUILD_DIR)
