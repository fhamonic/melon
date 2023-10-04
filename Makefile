BUILD_DIR = build

.PHONY: test package clean

$(BUILD_DIR):
	conan build . -of=${BUILD_DIR} -b=missing -pr=default

debug:
	conan build . -of=${BUILD_DIR} -b=missing -pr=debug


test: $(BUILD_DIR)
	@cd $(BUILD_DIR) && \
	ctest --output-on-failure

package:
	conan create . -u
	
clean:
	@rm -rf CMakeUserPresets.json
	@rm -rf $(BUILD_DIR)
