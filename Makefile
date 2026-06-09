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

	# Build the package with debugging and coverage flags
	CCACHE_DISABLE=1 \
	CXXFLAGS="$$CXXFLAGS --coverage -g -Og" \
	CFLAGS="$$CFLAGS --coverage -g -Og" \
	LDFLAGS="$$LDFLAGS --coverage" \
	CMAKE_CXX_STANDARD=$(CXX_STANDARD) \
	uv pip install --no-build-isolation --no-deps --force-reinstall -e .

	cp "$$(python -c 'import _coconext, os; print(os.path.join(os.path.dirname(_coconext.__file__), "_coconext.pyi"))')" python/_coconext.pyi
	ruff check --fix python/_coconext.pyi
	ruff format python/_coconext.pyi
	cp build/compile_commands.json compile_commands.json

GCOV_EXECUTABLE ?= gcov
CPP_TESTS_BUILD_DIR ?= build/tests

.PHONY: dev_tests
dev_tests: dev_build
	pytest --cov --cov-report= tests/python/
	pytest tests/integration_tests/
	cmake -S tests/cpp -B "$(CPP_TESTS_BUILD_DIR)" \
	    -DCMAKE_PREFIX_PATH="$$(coconext-config --cmake-prefix)" \
	    -DCMAKE_CXX_STANDARD=$(CXX_STANDARD) \
	    -DCMAKE_EXE_LINKER_FLAGS=--coverage
	cmake --build "$(CPP_TESTS_BUILD_DIR)"
	ctest --output-on-failure --test-dir "$(CPP_TESTS_BUILD_DIR)"

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
