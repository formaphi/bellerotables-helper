.PHONY: help setup sandbox-build sandbox-shell sandbox-down sandbox-clean-volumes \
	conan-install configure build rebuild clean \
	conan-install-unit configure-unit build-unit rebuild-unit test-unit test-one-unit clean-unit \
	conan-install-asan configure-asan build-asan rebuild-asan test-asan test-one-asan clean-asan \
	conan-install-tsan configure-tsan build-tsan rebuild-tsan test-tsan test-one-tsan clean-tsan \
	conan-install-valgrind configure-valgrind build-valgrind rebuild-valgrind test-valgrind test-one-valgrind clean-valgrind \
	check-contracts clean-coverage-report format format-check include-check tidy tidy-fix tidy-check

COMPOSE        := docker-compose -f sandbox/docker-compose.yml
RUN            := $(COMPOSE) run --rm sandbox
RUN_AS_HOST    := $(COMPOSE) run --rm --user $(shell id -u):$(shell id -g) sandbox
WORKSPACE      := /workspace
TEST_BUILDS    := /test-builds
CONAN_PROFILE  := $(WORKSPACE)/profiles/gcc13.conan
TOOLCHAIN_PATH := build/Release/generators/conan_toolchain.cmake

BUILD_DIR          := $(WORKSPACE)/production-builds/release
UNIT_BUILD_DIR     := $(TEST_BUILDS)/build-unit
ASAN_BUILD_DIR     := $(TEST_BUILDS)/build-asan
TSAN_BUILD_DIR     := $(TEST_BUILDS)/build-tsan
VALGRIND_BUILD_DIR := $(TEST_BUILDS)/build-valgrind

COVERAGE_DIR := $(WORKSPACE)/coverage-reports

SANITIZER_FLAGS = -fno-omit-frame-pointer -g

# Conan's libprotobuf is not ASan-built, but protobuf's headers annotate RepeatedField for ASan;
# the mix reports a false container-overflow in every binary holding generated protobuf code
# (tests, and protoc-gen-bvalidate while it runs during the build).
ASAN_ENV := env ASAN_OPTIONS=detect_container_overflow=0

# $(1) = output folder
define conan_install
	$(RUN) bash -c 'cd $(WORKSPACE) && conan install . --build=missing --no-remote \
	  --profile default --profile $(CONAN_PROFILE) --output-folder $(1)'
endef

help:
	@echo "bellerotables-helper targets (all run inside the sandbox container):"
	@echo ""
	@echo "setup                       Point git at .githooks/ (run once after cloning)"
	@echo "sandbox-build               Build the sandbox image"
	@echo "sandbox-shell               Shell in the sandbox"
	@echo "sandbox-down                Stop the sandbox (volumes kept)"
	@echo "sandbox-clean-volumes       Stop the sandbox and delete the test-builds volume"
	@echo ""
	@echo "configure | build | rebuild | clean"
	@echo "                            Release binary, BUILD_TESTING=OFF -> production-builds/release/"
	@echo ""
	@echo "<verb>-unit                 Tests + gcov; test-unit writes coverage-reports/html/index.html"
	@echo "<verb>-asan                 Tests under AddressSanitizer"
	@echo "<verb>-tsan                 Tests under ThreadSanitizer"
	@echo "<verb>-valgrind             Tests under Valgrind memcheck"
	@echo "    verbs: configure build rebuild test test-one(PATTERN=<p>) clean"
	@echo ""
	@echo "format | format-check       clang-format (apply | CI gate; format-check runs include-check)"
	@echo "include-check               Project headers must use \"\" and external headers <>"
	@echo "tidy | tidy-fix | tidy-check clang-tidy (report | fix | CI gate); needs 'make configure'"

setup:
	git config core.hooksPath .githooks
	@echo "Git hooks configured. Pre-commit hook is active."

# -- Sandbox ------------------------------------------------------------------

sandbox-build:
	$(COMPOSE) build

sandbox-shell:
	$(RUN)

sandbox-down:
	$(COMPOSE) down

sandbox-clean-volumes:
	$(COMPOSE) down -v

# The contracts submodule (pinned protobuf SSOT) must be checked out before any build.
check-contracts:
	@test -f contracts/proto/bellerophon/ngfw/redirection/desired.proto || { \
	  echo "ERROR: the 'contracts' submodule is not initialised."; \
	  echo "Run: git submodule update --init contracts"; exit 1; }

# -- Release ------------------------------------------------------------------
# The container runs as root without CAP_DAC_OVERRIDE (cap_drop: ALL), so host-side output dirs
# are created world-writable first. What it writes there is root-owned; deletes run through it.

conan-install: check-contracts
	mkdir -p production-builds/release && chmod 777 production-builds production-builds/release
	$(call conan_install,$(BUILD_DIR))

configure: conan-install
	$(RUN) cmake -S $(WORKSPACE) -B $(BUILD_DIR) -G Ninja \
	  -DCMAKE_TOOLCHAIN_FILE=$(BUILD_DIR)/$(TOOLCHAIN_PATH) \
	  -DCMAKE_BUILD_TYPE=Release \
	  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
	  -DBUILD_TESTING=OFF

build:
	$(RUN) cmake --build $(BUILD_DIR)

rebuild: clean configure build

clean:
	@if [ -d production-builds/release ]; then $(RUN) find $(BUILD_DIR) -mindepth 1 -delete; fi

# -- Unit tests + gcov --------------------------------------------------------

conan-install-unit: check-contracts
	$(call conan_install,$(UNIT_BUILD_DIR))

configure-unit: conan-install-unit
	$(RUN) cmake -S $(WORKSPACE) -B $(UNIT_BUILD_DIR) -G Ninja \
	  -DCMAKE_TOOLCHAIN_FILE=$(UNIT_BUILD_DIR)/$(TOOLCHAIN_PATH) \
	  -DCMAKE_BUILD_TYPE=Release \
	  -DBUILD_TESTING=ON \
	  -DENABLE_COVERAGE=ON

build-unit:
	$(RUN) cmake --build $(UNIT_BUILD_DIR)

rebuild-unit: clean-unit configure-unit build-unit

test-unit:
	mkdir -p coverage-reports && chmod 777 coverage-reports
	$(RUN) bash -c '\
	  lcov --zerocounters --directory $(UNIT_BUILD_DIR) && \
	  ctest --test-dir $(UNIT_BUILD_DIR) --output-on-failure && \
	  if [ -z "$$(find $(UNIT_BUILD_DIR) -name "*.gcda" -print -quit)" ]; then \
	    echo "No coverage data: nothing ran under test."; exit 0; fi && \
	  mkdir -p $(COVERAGE_DIR) && \
	  lcov --capture --directory $(UNIT_BUILD_DIR) --output-file $(COVERAGE_DIR)/coverage.info \
	       --ignore-errors mismatch,mismatch,gcov,gcov && \
	  lcov --remove $(COVERAGE_DIR)/coverage.info "/usr/*" "/root/.conan2/*" "*/test/*" \
	       "*_generated/*" "*/protoc-gen-bvalidate/src/*" \
	       --output-file $(COVERAGE_DIR)/coverage.filtered.info && \
	  genhtml $(COVERAGE_DIR)/coverage.filtered.info --output-directory $(COVERAGE_DIR)/html \
	          --title "bellerotables-helper coverage" --legend --demangle-cpp'

test-one-unit:
	@test -n "$(PATTERN)" || { echo "Usage: make test-one-unit PATTERN=pattern"; exit 1; }
	$(RUN) ctest --test-dir $(UNIT_BUILD_DIR) --output-on-failure -R $(PATTERN)

clean-unit: clean-coverage-report
	$(RUN) rm -rf $(UNIT_BUILD_DIR)

clean-coverage-report:
	@if [ -d coverage-reports ]; then $(RUN) find $(COVERAGE_DIR) -mindepth 1 -delete; fi

# -- AddressSanitizer ---------------------------------------------------------

conan-install-asan: check-contracts
	$(call conan_install,$(ASAN_BUILD_DIR))

configure-asan: conan-install-asan
	$(RUN) cmake -S $(WORKSPACE) -B $(ASAN_BUILD_DIR) -G Ninja \
	  -DCMAKE_TOOLCHAIN_FILE=$(ASAN_BUILD_DIR)/$(TOOLCHAIN_PATH) \
	  -DCMAKE_BUILD_TYPE=Release \
	  -DBUILD_TESTING=ON \
	  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined $(SANITIZER_FLAGS)" \
	  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"

build-asan:
	$(RUN) $(ASAN_ENV) cmake --build $(ASAN_BUILD_DIR)

rebuild-asan: clean-asan configure-asan build-asan

test-asan:
	$(RUN) $(ASAN_ENV) ctest --test-dir $(ASAN_BUILD_DIR) --output-on-failure

test-one-asan:
	@test -n "$(PATTERN)" || { echo "Usage: make test-one-asan PATTERN=pattern"; exit 1; }
	$(RUN) $(ASAN_ENV) ctest --test-dir $(ASAN_BUILD_DIR) --output-on-failure -R $(PATTERN)

clean-asan:
	$(RUN) rm -rf $(ASAN_BUILD_DIR)

# -- ThreadSanitizer ----------------------------------------------------------

conan-install-tsan: check-contracts
	$(call conan_install,$(TSAN_BUILD_DIR))

configure-tsan: conan-install-tsan
	$(RUN) cmake -S $(WORKSPACE) -B $(TSAN_BUILD_DIR) -G Ninja \
	  -DCMAKE_TOOLCHAIN_FILE=$(TSAN_BUILD_DIR)/$(TOOLCHAIN_PATH) \
	  -DCMAKE_BUILD_TYPE=Release \
	  -DBUILD_TESTING=ON \
	  -DCMAKE_CXX_FLAGS="-fsanitize=thread $(SANITIZER_FLAGS)" \
	  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"

build-tsan:
	$(RUN) cmake --build $(TSAN_BUILD_DIR)

rebuild-tsan: clean-tsan configure-tsan build-tsan

test-tsan:
	$(RUN) ctest --test-dir $(TSAN_BUILD_DIR) --output-on-failure

test-one-tsan:
	@test -n "$(PATTERN)" || { echo "Usage: make test-one-tsan PATTERN=pattern"; exit 1; }
	$(RUN) ctest --test-dir $(TSAN_BUILD_DIR) --output-on-failure -R $(PATTERN)

clean-tsan:
	$(RUN) rm -rf $(TSAN_BUILD_DIR)

# -- Valgrind memcheck --------------------------------------------------------
# No special compiler flags: Valgrind instruments the binary at run time. ctest -T memcheck wraps
# every test in MEMORYCHECK_COMMAND; --error-exitcode=1 makes any finding fail the target.
# Tests labelled no_memcheck run a third-party binary, not ours, and are skipped here.

conan-install-valgrind: check-contracts
	$(call conan_install,$(VALGRIND_BUILD_DIR))

configure-valgrind: conan-install-valgrind
	$(RUN) cmake -S $(WORKSPACE) -B $(VALGRIND_BUILD_DIR) -G Ninja \
	  -DCMAKE_TOOLCHAIN_FILE=$(VALGRIND_BUILD_DIR)/$(TOOLCHAIN_PATH) \
	  -DCMAKE_BUILD_TYPE=Release \
	  -DBUILD_TESTING=ON \
	  -DCMAKE_CXX_FLAGS="$(SANITIZER_FLAGS)" \
	  '-DMEMORYCHECK_COMMAND_OPTIONS=--leak-check=full --error-exitcode=1 --show-leak-kinds=all --track-origins=yes --num-callers=20'

build-valgrind:
	$(RUN) cmake --build $(VALGRIND_BUILD_DIR)

rebuild-valgrind: clean-valgrind configure-valgrind build-valgrind

test-valgrind:
	$(RUN) ctest --test-dir $(VALGRIND_BUILD_DIR) -T memcheck -LE no_memcheck --output-on-failure

test-one-valgrind:
	@test -n "$(PATTERN)" || { echo "Usage: make test-one-valgrind PATTERN=pattern"; exit 1; }
	$(RUN) ctest --test-dir $(VALGRIND_BUILD_DIR) -T memcheck -LE no_memcheck --output-on-failure -R $(PATTERN)

clean-valgrind:
	$(RUN) rm -rf $(VALGRIND_BUILD_DIR)

# -- clang-format / clang-tidy ------------------------------------------------
# Targets that rewrite sources run as the host user: root without CAP_DAC_OVERRIDE cannot write host files.

SOURCE_DIRS    := $(wildcard adapter_*) domain request_parser tools contracts_cpp
FORMAT_SOURCES := main.cpp $(if $(SOURCE_DIRS),$(shell find $(SOURCE_DIRS) -name '*.cpp' -o -name '*.h'))
TIDY_SOURCES   := main.cpp $(if $(SOURCE_DIRS),$(shell find $(SOURCE_DIRS) -path '*/test' -prune -o -name '*.cpp' -print))

format:
	$(RUN_AS_HOST) clang-format -i $(FORMAT_SOURCES)

format-check: include-check
	$(RUN) clang-format --dry-run --Werror $(FORMAT_SOURCES)

# clang-format sorts and groups includes but cannot turn <> into "", so project headers written
# with <> are caught here.
include-check:
	@if grep -nE '^[[:space:]]*#[[:space:]]*include[[:space:]]*<(adapter_[a-z_]+|domain|request_parser)/' $(FORMAT_SOURCES); then \
	  echo 'Project headers must be included with quotes: #include "adapter_.../...", "domain/..." or "request_parser/..."'; exit 1; fi

tidy:
	$(RUN) clang-tidy -p $(BUILD_DIR) $(TIDY_SOURCES)

tidy-fix:
	$(RUN_AS_HOST) clang-tidy -p $(BUILD_DIR) --fix --fix-errors $(TIDY_SOURCES)

tidy-check:
	$(RUN) clang-tidy -p $(BUILD_DIR) -warnings-as-errors='*' $(TIDY_SOURCES)
