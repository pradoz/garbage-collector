BUILD_DIR = build

.PHONY: all clean test rebuild

all: $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. && make

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

test: all
	@cd $(BUILD_DIR) && ctest --output-on-failure

clean:
	@rm -rf $(BUILD_DIR)

rebuild: clean all
