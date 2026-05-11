.PHONY: dev_build
dev_build:
	uv sync --no-default-groups --group=tests --no-install-project

	# Build the package with debugging and coverage flags
	CCACHE_DISABLE=1 \
	CXXFLAGS="$$CXXFLAGS --coverage -g -Og" \
	CFLAGS="$$CFLAGS --coverage -g -Og" \
	LDFLAGS="$$LDFLAGS --coverage" \
	uv pip install --no-build-isolation --force-reinstall -e .

GCOV_EXECUTABLE ?= gcov

.PHONY: dev_tests
dev_tests: dev_build
	@wheel_tag="$$(python tools/release_paths.py wheel-tag)"; \
	find . -name ".coverage" -delete; \
	COCOTB_USER_COVERAGE=1 pytest --cov=coconext --cov-report=; \
	ctest --output-on-failure --test-dir "build/$$wheel_tag"; \
	find . -name ".coverage" | xargs coverage combine; \
	coverage xml -o .python-coverage.xml; \
	gcovr "build/$$wheel_tag/" --gcov-executable='$(GCOV_EXECUTABLE)' --cobertura -o .cpp-coverage.xml; \
	coverage report; \
	gcovr --gcov-executable='$(GCOV_EXECUTABLE)' --print-summary


.PHONY: clean
clean:
	# build/ hardcoded in pyproject.toml
	rm -rf build/


.PHONY: release_build_wheel
release_build_wheel:
	@eval "$$(python tools/release_paths.py shell)" && \
	SKBUILD_CMAKE_BUILD_TYPE=Release SKBUILD_BUILD_DIR="$$RELEASE_BUILD_DIR" uv build --wheel --out-dir "$$RELEASE_DIST_DIR"


.PHONY: release_test_wheel
release_test_wheel: release_build_wheel
	@eval "$$(python tools/release_paths.py shell)" && \
	if [ -d "$$RELEASE_VENV" ]; then rm -rf "$$RELEASE_VENV"; fi && \
	uv venv "$$RELEASE_VENV" && \
	. "$$RELEASE_VENV/bin/activate" && \
	uv sync --active --no-default-groups --group=tests --no-install-project && \
	SKBUILD_BUILD_DIR="$$RELEASE_BUILD_DIR" uv pip install --force-reinstall "$$RELEASE_WHEEL" && \
	pytest && \
	ctest --output-on-failure --test-dir "$$RELEASE_BUILD_DIR"


.PHONY: release_build_sdist
release_build_sdist:
	@eval "$$(python tools/release_paths.py shell)" && \
	SKBUILD_CMAKE_BUILD_TYPE=Release SKBUILD_BUILD_DIR="$$RELEASE_BUILD_DIR" uv build --sdist --out-dir "$$RELEASE_DIST_DIR"


.PHONY: release_test_sdist
release_test_sdist: release_build_sdist
	@eval "$$(python tools/release_paths.py shell)" && \
	if [ -d "$$RELEASE_VENV" ]; then rm -rf "$$RELEASE_VENV"; fi && \
	uv venv "$$RELEASE_VENV" && \
	. "$$RELEASE_VENV/bin/activate" && \
	uv sync --active --no-default-groups --group=tests --no-install-project && \
	SKBUILD_BUILD_DIR="$$RELEASE_BUILD_DIR" uv pip install "$$RELEASE_SDIST" && \
	pytest && \
	ctest --output-on-failure --test-dir "$$RELEASE_BUILD_DIR"


.PHONY: release_build
release_build: release_build_wheel release_build_sdist


.PHONY: release_test
release_test: release_test_wheel release_test_sdist


DOCS_OUTDIR ?= .docs_out

.PHONY: docs
docs:
	uv sync --no-default-groups --group=docs
	sphinx-build docs/ '$(DOCS_OUTDIR)/' --color -b html
	@echo "Documentation built at $(DOCS_OUTDIR)/index.html"
