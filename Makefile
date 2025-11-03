.PHONY: all build test clean format format-check size

# Default target
all: build test

# Build
build:
	@echo "🔨 Building..."
	@cmake -B build -DCMAKE_BUILD_TYPE=Debug -DV4LINK_BUILD_TESTS=ON
	@cmake --build build -j

# Release build
release:
	@echo "🚀 Building release..."
	@cmake -B build-release -DCMAKE_BUILD_TYPE=Release -DV4LINK_BUILD_TESTS=ON
	@cmake --build build-release -j

# Run tests
test: build
	@echo "🧪 Running tests..."
	@cd build && ctest --output-on-failure

# Clean
clean:
	@echo "🧹 Cleaning..."
	@rm -rf build build-release build-debug build-asan build-ubsan

# Apply formatting
format:
	@echo "✨ Formatting C/C++ code..."
	@find src include tests -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' -o -name '*.c' \) \
		-not -path "*/vendor/*" -exec clang-format -i {} \;
	@echo "✨ Formatting CMake files..."
	@find . -name 'CMakeLists.txt' -o -name '*.cmake' | xargs cmake-format -i
	@echo "✅ Formatting complete!"

# Format check
format-check:
	@echo "🔍 Checking C/C++ formatting..."
	@find src include tests -type f \( -name '*.cpp' -o -name '*.hpp' -o -name '*.h' -o -name '*.c' \) \
		-not -path "*/vendor/*" | xargs clang-format --dry-run --Werror || \
		(echo "❌ C/C++ formatting check failed." && exit 1)
	@echo "🔍 Checking CMake formatting..."
	@find . -name 'CMakeLists.txt' -o -name '*.cmake' | xargs cmake-format --check || \
		(echo "❌ CMake formatting check failed." && exit 1)
	@echo "✅ All formatting checks passed!"

# Sanitizer build
asan: clean
	@echo "🛡️  Building with AddressSanitizer..."
	@cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DV4LINK_BUILD_TESTS=ON \
		-DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer -g"
	@cmake --build build-asan -j
	@echo "🧪 Running tests with AddressSanitizer..."
	@cd build-asan && ctest --output-on-failure

ubsan: clean
	@echo "🛡️  Building with UndefinedBehaviorSanitizer..."
	@cmake -B build-ubsan -DCMAKE_BUILD_TYPE=Debug -DV4LINK_BUILD_TESTS=ON \
		-DCMAKE_CXX_FLAGS="-fsanitize=undefined -fno-omit-frame-pointer -g"
	@cmake --build build-ubsan -j
	@echo "🧪 Running tests with UndefinedBehaviorSanitizer..."
	@cd build-ubsan && ctest --output-on-failure

# Build release and show stripped binary sizes
size:
	@echo "📦 Building release with size optimization..."
	@cmake -B build-release -DCMAKE_BUILD_TYPE=Release -DV4LINK_BUILD_TESTS=ON -DV4LINK_OPTIMIZE_SIZE=ON -DV4LINK_ENABLE_LTO=ON -DV4_ENABLE_LTO=ON
	@cmake --build build-release -j
	@echo ""
	@echo "📊 Stripping and measuring binary sizes..."
	@echo ""
	@echo "=== Library ==="
	@if [ -f build-release/libv4link.a ]; then \
		cp build-release/libv4link.a build-release/libv4link.stripped.a && \
		strip --strip-debug build-release/libv4link.stripped.a && \
		ls -lh build-release/libv4link.a build-release/libv4link.stripped.a | awk '{print $$9 ": " $$5}'; \
	fi
	@echo ""
	@echo "=== Test Executable (stripped) ==="
	@if [ -f "build-release/tests/v4link_test" ] && [ -x "build-release/tests/v4link_test" ]; then \
		cp "build-release/tests/v4link_test" "build-release/tests/v4link_test.stripped" && \
		strip "build-release/tests/v4link_test.stripped" && \
		echo "v4link_test: $$(ls -lh build-release/tests/v4link_test.stripped | awk '{print $$5}')"; \
	fi
	@echo ""
	@echo "=== Minimal Binary (stripped) ==="
	@if [ -f "build-release/tests/v4link_minimal" ] && [ -x "build-release/tests/v4link_minimal" ]; then \
		cp "build-release/tests/v4link_minimal" "build-release/tests/v4link_minimal.stripped" && \
		strip "build-release/tests/v4link_minimal.stripped" && \
		echo "v4link_minimal: $$(ls -lh build-release/tests/v4link_minimal.stripped | awk '{print $$5}')"; \
		echo ""; \
		echo "Binary composition:"; \
		size "build-release/tests/v4link_minimal.stripped" | awk 'NR==1 || NR==2 {print}'; \
	fi
	@echo ""
	@echo "✅ Size measurement complete!"
