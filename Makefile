# explicitly specify cocotb directory location while testing locally
# CI clones cocotb in this directory hence it defaults to point to ./cocotb/
COCOTB_DIR_PATH ?= $(PWD)/cocotb

# Defaults to dev so environments are shifting back and forth locally,
# but set to "dev_tests" in CI to avoid installing unnecessary dependencies.
DEV_BUILD_DEP_GROUP ?= dev

CXX_STANDARD ?= 20

.PHONY: dev_build
dev_build:
	uv sync --no-default-groups --group=$(DEV_BUILD_DEP_GROUP) --no-install-project

	# Build the package with debugging and coverage flags.
	CCACHE_DISABLE=1 \
	CXXFLAGS="$$CXXFLAGS --coverage -g -Og" \
	CFLAGS="$$CFLAGS --coverage -g -Og" \
	LDFLAGS="$$LDFLAGS --coverage" \
	CMAKE_CXX_STANDARD=$(CXX_STANDARD) \
	uv pip install --no-build-isolation --no-deps --force-reinstall -e .

	# Generate stubs.
	# import coconext to preload libcoconext.so and libgpi.so.
	# Run ruff on the generated stubs to fix formatting and linting issues,
	# and make `git diff` fail if the stubs are not up to date.
	python -c 'import coconext; from nanobind.stubgen import main; main(["-m", "_pycoconext", "-o", "python/_pycoconext.pyi"])'
	ruff check --fix python/_pycoconext.pyi
	ruff format python/_pycoconext.pyi

	# Copy compile database to project root for clang-tidy and editor integration.
	cp build/compile_commands.json compile_commands.json

GCOV_EXECUTABLE ?= gcov
CPP_TESTS_BUILD_DIR ?= build/tests

.PHONY: dev_tests
dev_tests: dev_build
	pytest --cov --cov-report= tests/python/
	cmake -S tests/cpp -B "$(CPP_TESTS_BUILD_DIR)" \
	    -DCMAKE_PREFIX_PATH="$$(coconext-config --cmake-prefix)" \
	    -DCMAKE_CXX_STANDARD=$(CXX_STANDARD) \
	    -DCMAKE_EXE_LINKER_FLAGS=--coverage
	cmake --build "$(CPP_TESTS_BUILD_DIR)"
	ctest --output-on-failure --test-dir "$(CPP_TESTS_BUILD_DIR)"

NB_TESTS_BUILD_DIR ?= build/nanobind_tests

.PHONY: nanobind_tests
nanobind_tests:
	cmake -S tests/nanobind -B "$(NB_TESTS_BUILD_DIR)" \
		-DCMAKE_CXX_STANDARD=$(CXX_STANDARD) \
		-Dnanobind_DIR=$$(python3 -m nanobind --cmake_dir)
	cmake --build "$(NB_TESTS_BUILD_DIR)"
	NB_SO_DIR="$(NB_TESTS_BUILD_DIR)" \
	pytest tests/nanobind/pytest

release_test:
	uv sync --no-default-groups --no-install-project
	uv pip install coconext --find-links dist --no-index
	pytest tests/python/
	cmake -S tests/cpp -B "$(CPP_TESTS_BUILD_DIR)" \
		-DCMAKE_PREFIX_PATH="$$(coconext-config --cmake-prefix)" \
		-DCMAKE_CXX_STANDARD=$(CXX_STANDARD)
	cmake --build "$(CPP_TESTS_BUILD_DIR)"
	ctest --output-on-failure --test-dir "$(CPP_TESTS_BUILD_DIR)"

.PHONY: integration_tests
integration_tests: dev_build
	COCOTB_DIR_PATH="$(COCOTB_DIR_PATH)" \
	pytest --cov --cov-append --cov-report= tests/integration_tests/

.PHONY: generate_report
generate_report:
	coverage xml -o .python-coverage.xml
	gcovr build/ --gcov-executable='$(GCOV_EXECUTABLE)' --cobertura -o .cpp-coverage.xml
	coverage report
	gcovr build/ --gcov-executable='$(GCOV_EXECUTABLE)' --print-summary

.PHONY: clean
clean:
	rm -rf build/

DOCS_OUTDIR ?= .docs_out

.PHONY: docs
docs:
	uv sync --dev --no-install-project
	sphinx-build docs/ '$(DOCS_OUTDIR)/' --color -b html
	@echo "Documentation built at $(DOCS_OUTDIR)/index.html"

# This default assumes you are running on a system where run-clang-tidy is fairly new.
# We require clang-tidy 18 at a minimum to support misc-include-cleaner.
# you may need to set it to run-clang-tidy-18 or similar.
RUN_CLANG_TIDY_EXECUTABLE ?= run-clang-tidy

.PHONY: clang_tidy
clang_tidy: dev_build
	$(RUN_CLANG_TIDY_EXECUTABLE) -p . -warnings-as-errors='*' -quiet 'coconext/(cpp|nanobind)/src/'
