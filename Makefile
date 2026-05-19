# Defaults to dev so environments are shifting back and forth locally,
# but set to "tests" in CI to avoid installing unnecessary dependencies.
DEV_BUILD_DEP_GROUP ?= dev

.PHONY: dev_build
dev_build:
	uv sync --no-default-groups --group=$(DEV_BUILD_DEP_GROUP) --no-install-project

	# Build the package with debugging and coverage flags
	CCACHE_DISABLE=1 \
	CXXFLAGS="$$CXXFLAGS --coverage -g -Og" \
	CFLAGS="$$CFLAGS --coverage -g -Og" \
	LDFLAGS="$$LDFLAGS --coverage" \
	uv pip install --no-build-isolation --no-deps --force-reinstall -e .

	cp "$$(python -c 'import _coconext, os; print(os.path.join(os.path.dirname(_coconext.__file__), "_coconext.pyi"))')" python/_coconext.pyi
	ruff check --fix python/_coconext.pyi
	ruff format python/_coconext.pyi
	cp build/compile_commands.json compile_commands.json

GCOV_EXECUTABLE ?= gcov
CPP_TESTS_BUILD_DIR ?= build/tests

.PHONY: dev_tests
dev_tests: dev_build
	find . -name ".coverage" -delete
	COCOTB_USER_COVERAGE=1 pytest --cov=coconext --cov-report=
	cmake -S tests/cpp -B "$(CPP_TESTS_BUILD_DIR)" \
	    -DCMAKE_PREFIX_PATH="$$(python -c 'import coconext_tools; print(coconext_tools.cmake_prefix_path())')" \
	    -DCMAKE_EXE_LINKER_FLAGS=--coverage
	cmake --build "$(CPP_TESTS_BUILD_DIR)"
	ctest --output-on-failure --test-dir "$(CPP_TESTS_BUILD_DIR)"
	find . -name ".coverage" | xargs coverage combine
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
