#!/usr/bin/env python3

"""Generate test environments and the suites relevant to each one.

The primary environment runs every suite. Other environments exercise only
the dimensions they vary: OS/compiler, C++ standard, or Python version.
"""

from __future__ import annotations

import argparse
import json
import sys

ALL_TESTS = "dev_build coverage_reset cpp_tests python_tests integration_tests simulator_tests generate_report"
OS_TESTS = (
    "dev_build coverage_reset cpp_tests python_tests simulator_tests generate_report"
)
CXX_STANDARD_TESTS = "dev_build coverage_reset cpp_tests python_tests generate_report"
PYTHON_VERSION_TESTS = (
    "dev_build coverage_reset python_tests simulator_tests generate_report"
)

ENVS = [
    {
        # Test Linux with clang
        "python-version": "3.13",
        "simulator-version": "1.20.1",
        "os": "ubuntu-24.04",
        "simulator": "nvc",
        "toplevel_lang": "vhdl",
        "cc": "clang",
        "cxx": "clang++",
        "gcov": "llvm-cov gcov",
        "cxx-standard": "20",
        "make-targets": ALL_TESTS,
    },
    {
        # Test macOS 26 (Apple Silicon)
        "python-version": "3.13",
        "simulator": "icarus",
        "toplevel_lang": "verilog",
        "os": "macos-26",
        "cxx-standard": "20",
        "make-targets": OS_TESTS,
    },
    {
        # Test macOS 15 (Intel)
        "python-version": "3.13",
        "simulator": "icarus",
        "toplevel_lang": "verilog",
        "os": "macos-15-intel",
        "cxx-standard": "20",
        "make-targets": OS_TESTS,
    },
    {
        # Test C++23
        "python-version": "3.13",
        "simulator-version": "1.20.1",
        "os": "ubuntu-24.04",
        "simulator": "nvc",
        "toplevel_lang": "vhdl",
        "cxx-standard": "23",
        "make-targets": CXX_STANDARD_TESTS,
    },
]

python_versions = ["3.9", "3.10", "3.11", "3.12", "3.13", "3.14", "3.14t"]
for ver in python_versions:
    ENVS += [
        {
            "python-version": ver,
            "simulator": "nvc",
            "os": "ubuntu-24.04",
            "simulator-version": "1.20.1",
            "toplevel_lang": "vhdl",
            "cxx-standard": "20",
            "make-targets": PYTHON_VERSION_TESTS,
            # Only check stubs on one version
            "check_stubs": "true" if ver != "3.9" else "false",
        }
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-format", choices=("gha", "json"), default="json")
    parser.add_argument(
        "--gha-output-file",
        type=argparse.FileType("a", encoding="utf-8"),
        help="The $GITHUB_OUTPUT file.",
    )

    args = parser.parse_args()

    selected_envs = ENVS

    for env in selected_envs:
        # Assemble the human-readable name of the job.
        name_parts = [f"Python {env['python-version']}"]

        if "simulator_tests" in env["make-targets"]:
            name_parts.append(env["simulator"])

        if int(env["cxx-standard"]) != 20:
            name_parts.append(f"C++{env['cxx-standard']}")

        if "cc" in env:
            name_parts.append(env["cc"])

        if not env["os"].startswith("ubuntu"):
            name_parts.append(env["os"])

        if env.get("may-fail-dev") is not None:
            name_parts.append("May fail")

        env["name"] = " | ".join(name_parts)

    if args.output_format == "gha":
        assert args.gha_output_file is not None

        print(f"envs={json.dumps(selected_envs)}", file=args.gha_output_file)

        print("Generated the following test configurations:")
        print(json.dumps(selected_envs, indent=2))

    elif args.output_format == "json":
        print(json.dumps(selected_envs, indent=2))

    else:
        assert False

    return 0


if __name__ == "__main__":
    sys.exit(main())
