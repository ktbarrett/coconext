.DEFAULT_GOAL := dev_tests

# Defaults to dev so environments are shifting back and forth locally,
# but set to "dev_tests" in CI to avoid installing unnecessary dependencies.
DEV_BUILD_DEP_GROUP ?= dev

SIM ?= nvc
TOPLEVEL_LANG ?= vhdl
CXX_STANDARD ?= 20
GCOV_EXECUTABLE ?= gcov
COCONEXT_DEVELOPER_MODE ?= ON

export SIM TOPLEVEL_LANG

PYTEST_COVERAGE_ARGS ?= --cov --cov-append --cov-report=

TESTS_BUILD_DIR ?= build/tests

.PHONY: dev_tests
dev_tests:
	$(MAKE) dev_build
	$(MAKE) coverage_reset
	$(MAKE) cpp_tests
	$(MAKE) python_tests
	$(MAKE) integration_tests
	$(MAKE) simulator_tests
	$(MAKE) generate_report

.PHONY: release_test
release_test:
	$(MAKE) release_install
	$(MAKE) cpp_tests COCONEXT_DEVELOPER_MODE=OFF
	$(MAKE) python_tests COCONEXT_DEVELOPER_MODE=OFF PYTEST_COVERAGE_ARGS=
	$(MAKE) integration_tests PYTEST_COVERAGE_ARGS=
	$(MAKE) simulator_tests PYTEST_COVERAGE_ARGS=

.PHONY: dev_build
dev_build:
	uv sync --no-default-groups --group=$(DEV_BUILD_DEP_GROUP) --no-install-project

	# Build the package with the requested standard, strict warnings, and coverage.
	CCACHE_DISABLE=1 \
	CMAKE_ARGS="$$CMAKE_ARGS -DCMAKE_CXX_STANDARD=$(CXX_STANDARD) -DCOCONEXT_DEVELOPER_MODE=$(COCONEXT_DEVELOPER_MODE)" \
	uv pip install --no-build-isolation --no-deps --force-reinstall -e .

	# Generate stubs.
	# import coconext to preload libcoconext.so and libgpi.so.
	# Run ruff on the generated stubs to fix formatting and linting issues,
	# and make `git diff` fail if the stubs are not up to date.
	python -c 'import coconext; from nanobind.stubgen import main; main(["-m", "_pycoconext", "-o", "python/_pycoconext.pyi"])'
	ruff check --fix --unsafe-fixes python/_pycoconext.pyi
	ruff format python/_pycoconext.pyi

	# Copy compile database to project root for clang-tidy and editor integration.
	cp build/compile_commands.json compile_commands.json

.PHONY: release_install
release_install:
	uv sync --no-default-groups --group=dev_tests --no-install-project
	uv pip install --no-build-isolation --no-deps --force-reinstall \
		coconext --find-links dist --no-index

.PHONY: cpp_tests
cpp_tests:
	cmake -S tests/cpp -B "$(TESTS_BUILD_DIR)/cpp" \
		-DCMAKE_PREFIX_PATH="$$(coconext-config --cmake-prefix)" \
		-DCMAKE_CXX_STANDARD=$(CXX_STANDARD) \
		-DCOCONEXT_DEVELOPER_MODE=$(COCONEXT_DEVELOPER_MODE)
	cmake --build "$(TESTS_BUILD_DIR)/cpp" --parallel
	ctest --output-on-failure --test-dir "$(TESTS_BUILD_DIR)/cpp"

.PHONY: python_tests
python_tests:
	cmake -S tests/python -B "$(TESTS_BUILD_DIR)/python" \
		-DCMAKE_PREFIX_PATH="$$(coconext-config --cmake-prefix)" \
		-DCMAKE_CXX_STANDARD=$(CXX_STANDARD) \
		-DCOCONEXT_DEVELOPER_MODE=$(COCONEXT_DEVELOPER_MODE) \
		-DPython_EXECUTABLE="$$(python3 -c 'import sys; print(sys.executable)')" \
		-Dnanobind_DIR="$$(python3 -m nanobind --cmake_dir)"
	cmake --build "$(TESTS_BUILD_DIR)/python" --parallel
	PYTHON_TESTS_MODULE_DIR="$(TESTS_BUILD_DIR)/python" \
	pytest $(PYTEST_COVERAGE_ARGS) tests/python/pytest

.PHONY: simulator_tests
simulator_tests:
	pytest $(PYTEST_COVERAGE_ARGS) tests/simulator

.PHONY: integration_tests
integration_tests: dev_build
	pytest $(PYTEST_COVERAGE_ARGS) tests/integration_tests tests/pytest/

.PHONY: coverage_reset
coverage_reset:
	rm -f .coverage .coverage.* .python-coverage.xml .cpp-coverage.xml
	@[ ! -d build ] || find build -type f -name '*.gcda' -delete

.PHONY: generate_report
generate_report:
	coverage xml -o .python-coverage.xml
	gcovr build/ --gcov-executable='$(GCOV_EXECUTABLE)' --cobertura -o .cpp-coverage.xml
	coverage report
	gcovr build/ --gcov-executable='$(GCOV_EXECUTABLE)' --print-summary

.PHONY: clean_coverage_report
clean_coverage_report:
	rm -rf *.coverage .*.xml build/

.PHONY: clean
clean: coverage_reset
	rm -rf build/

DOCS_SOURCE ?= docs/source
DOCS_OUTDIR ?= .docs_out
SPHINXOPTS ?=

.PHONY: docs
docs:
	uv sync --no-default-groups --group=docs
	uv run --no-sync sphinx-build '$(DOCS_SOURCE)' '$(DOCS_OUTDIR)' --color -b html $(SPHINXOPTS)
	@echo "Documentation built at $(DOCS_OUTDIR)/index.html"

.PHONY: docs_preview
docs_preview:
	uv sync --no-default-groups --group=docs_preview
	uv run --no-sync sphinx-autobuild \
		--ignore '*/source/master-notes.rst' \
		--ignore '*/doxygen/*' \
		--ignore '**/\#*\#' \
		--ignore '**/.\#*' \
		--ignore '**/.*.sw[px]' \
		--ignore '**/*~' \
		--ignore '*@*:*' \
		--watch python/cocotb \
		'$(DOCS_SOURCE)' '$(DOCS_OUTDIR)' $(SPHINXOPTS)

.PHONY: docs_linkcheck
docs_linkcheck:
	uv sync --no-default-groups --group=docs
	uv run --no-sync sphinx-build '$(DOCS_SOURCE)' '$(DOCS_OUTDIR)' --color -b linkcheck $(SPHINXOPTS)

.PHONY: docs_spelling
docs_spelling:
	uv sync --no-default-groups --group=docs
	uv run --no-sync sphinx-build '$(DOCS_SOURCE)' '$(DOCS_OUTDIR)' --color -b spelling $(SPHINXOPTS)

# This default assumes you are running on a system where run-clang-tidy is fairly new.
# We require clang-tidy 18 at a minimum to support misc-include-cleaner.
# you may need to set it to run-clang-tidy-18 or similar.
RUN_CLANG_TIDY_EXECUTABLE ?= run-clang-tidy

.PHONY: clang_tidy
clang_tidy: dev_build
	$(RUN_CLANG_TIDY_EXECUTABLE) -p . -warnings-as-errors='*' -quiet '(cpp/coconext/src|python/_pycoconext)/'
