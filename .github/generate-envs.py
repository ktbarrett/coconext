#!/usr/bin/env python3

"""Generate a list of test environments.

Each environment must contain the following fields:

Optional fields:

What tests belong in what groups:
"""

from __future__ import annotations

import argparse
import json
import sys

ENVS = [
    {
        "python-version": "3.13",
        "nvc-version": "1.20.1",
        "os": "ubuntu-24.04",
        "simulator": "nvc",
        "toplevel_lang": "vhdl",
        "cc": "clang",
        "cxx": "clang++",
        "gcov": "llvm-cov gcov",
        "cxx-standard": "20",
    },
    {
        "python-version": "3.13",
        "simulator": "icarus",
        "toplevel_lang": "verilog",
        "os": "macos-26",
        "cxx-standard": "20",
    },
    {
        "python-version": "3.13",
        "simulator": "icarus",
        "toplevel_lang": "verilog",
        "os": "macos-15-intel",
        "cxx-standard": "20",
    },
    {
        "python-version": "3.13",
        "nvc-version": "1.20.1",
        "os": "ubuntu-24.04",
        "simulator": "nvc",
        "toplevel_lang": "vhdl",
        "cxx-standard": "23",
    },
]

python_versions = ["3.9", "3.10", "3.11", "3.12", "3.13", "3.14"]
for ver in python_versions:
    ENVS += [
        {
            "python-version": ver,
            "simulator": "nvc",
            "os": "ubuntu-24.04",
            "nvc-version": "1.20.1",
            "toplevel_lang": "vhdl",
            "cxx-standard": "20",
        }
    ]


def append_str_val(listref, my_list, key) -> None:
    if key not in my_list:
        return
    listref.append(str(my_list[key]))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--group")
    parser.add_argument("--output-format", choices=("gha", "json"), default="json")
    parser.add_argument(
        "--gha-output-file",
        type=argparse.FileType("a", encoding="utf-8"),
        help="The $GITHUB_OUTPUT file.",
    )

    args = parser.parse_args()

    if args.group is not None and args.group != "":
        selected_envs = [t for t in ENVS if "group" in t and t["group"] == args.group]
    else:
        # Return all tasks if no group is selected.
        selected_envs = ENVS

    for env in selected_envs:
        # Assemble the human-readable name of the job.
        name_parts = []
        append_str_val(name_parts, env, "simulator")
        append_str_val(name_parts, env, "os")
        append_str_val(name_parts, env, "python-version")
        if env.get("may-fail-dev") is not None:
            name_parts.append("May fail")

        env["name"] = "|".join(name_parts)

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
