BUILD_DIR = build
# BUILD_TYPE = default
BUILD_TYPE = debug

.PHONY: test package clean

$(BUILD_DIR):
	conan build . -of=${BUILD_DIR} -b=missing -pr=${BUILD_TYPE}

debug:
	conan build . -of=${BUILD_DIR} -b=missing -pr=${BUILD_TYPE}


test: $(BUILD_DIR)
	@cd $(BUILD_DIR) && \
	ctest --output-on-failure

package:
	conan create . -u
	
clean:
	@rm -rf CMakeUserPresets.json
	@rm -rf $(BUILD_DIR)
