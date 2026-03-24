
TESTS_OUTPUT_HTML := 0

.PHONY: tests
tests:
	uv sync --dev --no-install-project
	uv pip install --no-build-isolation --force-reinstall .

	# Clean up old coverage data
	find . -name ".coverage" -delete

	# Run Python tests
	COCOTB_USER_COVERAGE=1 pytest --cov=coconext --cov-report= tests/

	# Run C++ tests
	WHEEL_TAG=$$(python -c "from scikit_build_core.builder.wheel_tag import WheelTag; print(WheelTag.compute_best([], ''))"); \
    ctest --output-on-failure --test-dir build/$$WHEEL_TAG

	# Combine coverage data and generate reports
	find . -name ".coverage" | xargs coverage combine
	coverage xml
	coverage report
ifeq ($(TESTS_OUTPUT_HTML),1)
	coverage html -d .htmlcov
endif


DOCS_OUTDIR := .docs_out

.PHONY: docs
docs:
	uv sync --no-default-groups --group=docs
	sphinx-build docs/ $(DOCS_OUTDIR)/ --color -b html
	@echo "Documentation built at $(DOCS_OUTDIR)/index.html"
