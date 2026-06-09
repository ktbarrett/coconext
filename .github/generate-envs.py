#!/usr/bin/env python3

"""Generate a list of test environments.

Each environment must contain the following fields:
TODO

Optional fields:
TODO

What tests belong in what groups:
TODO
"""

from __future__ import annotations

import argparse
import json
import sys

ENVS = [
    {
        "python-version": "3.13",
        "simulator-version": "1.20.1",
        "os": "ubuntu-24.04",
        "simulator": "nvc",
        "toplevel_lang": "vhdl",
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
        "simulator-version": "1.20.1",
        "os": "ubuntu-24.04",
        "simulator": "nvc",
        "toplevel_lang": "vhdl",
        "cxx-standard": "23",
    },
]

python_versions = ["3.9", "3.10", "3.11", "3.12", "3.14"]
for ver in python_versions:
    ENVS += [
        {
            "python-version": ver,
            "simulator": "nvc",
            "os": "ubuntu-24.04",
            "simulator-version": "1.20.1",
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
        name_parts = []
        append_str_val(name_parts, env, "simulator")
        append_str_val(name_parts, env, "os")
        append_str_val(name_parts, env, "python-version")

        if int(env["cxx-standard"]) != 20:
            cpp_ver = env["cxx-standard"]
            name_parts.append(f"C++{cpp_ver}")

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
