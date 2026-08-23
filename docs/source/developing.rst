*****************
Developing cocotb
*****************

Setting Up a Development Environment
====================================

:ref:`Install prerequisites to build the development version of cocotb <install-devel>` and standard development tools (editor, shell, git, etc.).

.. note:: Documentation generation requires Python 3.11+.

First, you should `fork and clone <https://guides.github.com/activities/forking/>`__ the cocotb repo to your machine.
This will allow you to make changes to the cocotb source code, create pull requests, and run regressions and build documentation locally.

You will need `doxygen <https://www.doxygen.nl/index.html>`__, for building documentation.
We recommend if you are using a Linux distribution to use your system package manager to install doxygen.
Likewise, doxygen can be installed using the homebrew package manager on Mac OS.
Windows contributors should download a binary distribution installer from the main website.

Next install `uv <https://docs.astral.sh/uv/getting-started/installation/>`__,
which is a tool for managing Python virtual environments and dependencies.

After ``uv`` is installed, run the following command in the project root to build a virtual environment for development.

.. code:: bash

   uv venv

This will create a virtual environment and print instructions on how to activate it.
After you activate the virtual environment, you can install the development dependencies with the following command:

.. code:: bash

   uv sync --dev

.. note::
   We recommend using `direnv <https://direnv.net/>`__ to automatically activate the virtual environment when you navigate into the project directory.

To enable pre-commit checks, run the following command at the root of the cloned project to install the git hooks.

.. code:: bash

   prek install

When committing, prek's git commit hooks will run, checking your changes for formatting, code smells, etc.
You will see the lists of checks printed and whether they passed, were skipped, or failed.
If any of the checks fail, it is recommended to fix them before opening a pull request,
otherwise the pull request checks will fail as well.

Now you are ready to contribute!

Running Tests Locally
=====================

First, `set up your development environment <#setting-up-a-development-environment>`__.

Development tests are managed by the top-level Makefile.
The default suite builds the package and runs the C++, Python, compatibility,
and simulator-backed coconext tests.

To run the tests locally, issue the following command.

.. code:: bash

   make dev_tests

The retained cocotb regression is available separately through
``make cocotb_tests`` and requires the relevant simulators to be installed.

The simulator and the toplevel language can be changed by setting the environment variables :make:var:`SIM` and :make:var:`TOPLEVEL_LANG`.

Selecting a Language and Simulator for Regression
=================================================

cocotb can be used with multiple simulators and languages.
Select them with Make variables.

.. code:: bash

   # Run the default development suite with Icarus Verilog.
   make dev_tests SIM=icarus TOPLEVEL_LANG=verilog

   # Run only the simulator-backed coconext tests with NVC and VHDL.
   make simulator_tests SIM=nvc TOPLEVEL_LANG=vhdl

   # Run the retained cocotb regression with a selected interface.
   make cocotb_tests SIM=questa TOPLEVEL_LANG=vhdl VHDL_GPI_INTERFACE=vhpi

Running Individual Tests Locally
================================

Each test under ``/tests/test_cases/*/`` and ``/examples/*/tests/`` can be run individually.
This is particularly useful if you want to run a particular test that fails the regression.

First, navigate to the directory containing the test you wish to run.
Then you may issue an :ref:`make <building>` command.
For example, if you want to test with Icarus using Verilog sources:

.. code:: bash

   make SIM=icarus TOPLEVEL_LANG=verilog

Building Documentation Locally
==============================

First, `set up your development environment <#setting-up-a-development-environment>`__.

Documentation is built locally using ``make``.
The last message in the output will contain a URL to the documentation you just built.
Simply copy and paste the link into your browser to view it.
The documentation will be built in the same location on your hard drive on every run, so you only have to refresh the page to see new changes.

To build the documentation locally on Linux or Mac, issue the following command:

.. code:: bash

   make docs

Building the documentation is not currently supported on Windows.
