
.PHONY: dev_build
dev_build:
	uv sync --dev --no-install-project

	# Build the package with debugging and coverage flags
	CCACHE_DISABLE=1 \
	CXXFLAGS="$$CXXFLAGS --coverage -g -Og" \
	CFLAGS="$$CFLAGS --coverage -g -Og" \
	LDFLAGS="$$LDFLAGS --coverage" \
	uv pip install --no-build-isolation --force-reinstall -e .

GCOV_EXECUTABLE ?= $(subst gcc,gcov,$(or $(CC),gcc))

.PHONY: tests
tests: dev_build
	# Clean up old coverage data
	find . -name ".coverage" -delete

	# Run Python tests
	COCOTB_USER_COVERAGE=1 pytest --cov=coconext --cov-report=

	# Run C++ tests
	WHEEL_TAG=$$(python -c "from scikit_build_core.builder.wheel_tag import WheelTag; print(WheelTag.compute_best([], ''))"); \
    ctest --output-on-failure --test-dir build/$$WHEEL_TAG

	# Combine coverage data and generate reports
	find . -name ".coverage" | xargs coverage combine
	coverage xml -o .python-coverage.xml
	WHEEL_TAG=$$(python -c "from scikit_build_core.builder.wheel_tag import WheelTag; print(WheelTag.compute_best([], ''))"); \
	gcovr build/$$WHEEL_TAG/ --gcov-executable=$(GCOV_EXECUTABLE) --cobertura -o .cpp-coverage.xml
	coverage report
	gcovr --gcov-executable=$(GCOV_EXECUTABLE) --print-summary


DOCS_OUTDIR ?= .docs_out

.PHONY: docs
docs:
	uv sync --no-default-groups --group=docs
	sphinx-build docs/ $(DOCS_OUTDIR)/ --color -b html
	@echo "Documentation built at $(DOCS_OUTDIR)/index.html"
