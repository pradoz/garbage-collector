BUILD_DIR = build

.PHONY: all build test test-verbose example clean

all: build

build:
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake .. && $(MAKE)

test:
	@cd $(BUILD_DIR) && ctest --output-on-failure

test-verbose:
	@cd $(BUILD_DIR) && ctest -V

example: build
	@echo "=== Running Visualizer Demo ==="
	@./$(BUILD_DIR)/examples/visualizer_demo

clean:
	@rm -rf build

help:
	@echo "Available targets:"
	@echo "  make build         - Build the project"
	@echo "  make test          - Run tests (quiet visualizer tests)"
	@echo "  make test-verbose  - Run tests with full output"
	@echo "  make example       - Run visualizer demo"
	@echo "  make clean         - Clean build directory"
