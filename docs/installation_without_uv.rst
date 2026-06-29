Installing without using ``uv``
###############################

You may be in a locked down environment that doesn't have a newer tool like ``uv`` already available,
or perhaps you are already familiar with ``pip`` and ``venv`` and just want to use those over a new tool.
Either way, installing ``coconext`` is still possible, with some tweaked directions.

To replace ``uv`` we need to install some other Python tools: ``pip`` and ``venv``.
Both of these tools require Python, so follow :ref:`install-python` first.

Install ``pip``
===============

`pip <https://packaging.python.org/en/latest/tutorials/installing-packages/#use-pip-for-installing>`_ is the official Python package manager.
``pip`` may already come with your Python distribution;
so try the below commands in order and stop after the first succeeds.

.. code-block:: sh

   pip --version  # This may be Python 2, check version output
   pip3 --version
   # Use the appropriate python command for your system
   python -m pip --version
   python3 -m pip --version

.. note::
   These instructions require ``pip`` version 25.1 or newer for PEP 735 dependency group support (``pip install --group``).
   If your ``pip`` is older, upgrade with ``pip install --upgrade pip``.

Remember whichever of these particular incantation worked;
that will be how you invoke ``pip`` for the remainder of the installation instructions.

If all of the above fail, ``pip`` is not installed.
Your package manager likely provides ``pip`` as a separate package.

.. tab-set::

   .. tab-item:: aptitude (Debian / Ubuntu)

      .. code-block:: sh

         sudo apt install python3-pip

   .. tab-item:: DNF/YUM (Red Hat, Fedora, and RockyLinux)

      .. code-block:: sh

         sudo dnf install python3-pip

   .. tab-item:: homebrew (MacOS and Linux)

      Installing Python via homebrew should also install ``pip``.

Afterwards, try the set of ``pip`` invocations mentioned above to find out which one works on your system.

Install ``venv``
================

`venv <https://docs.python.org/3/library/venv.html>`_ is a virtual environment creation tool built into the Python standard library.
To see if it's available run ``python -m venv -h``.

Some systems, notable Debian-based distributions, ship the ``venv`` library as a separate package.

.. tab-set::

   .. tab-item:: Debian / Ubuntu

      .. code-block:: sh

         sudo apt install python3-venv

Creating a ``pyproject.toml``
=============================

This must be done manually, but it's a fairly simple text file.
This can be blank for now, as the only thing we need to add is the coconext dependency which we will cover in ``Installing coconext`` below.


Create and activate a Python virtual environment
================================================

Use ``venv`` to create a virtual environment in your project's root directory.
The idiomatic name for the virtual environment directory is ``.venv/``.

.. code-block:: sh

   python -m venv .venv/

Then you can activate the virtual environment with the following command.

.. code-block:: sh

   . .venv/bin/activate

You can also set up your project so that the virtual environment is automatically activated when you enter the project directory.
See :ref:`use-direnv` for more details.

Installing ``coconext``
=======================

System Prerequisites
--------------------

Currently, there are no system library prerequisites.

Installing Locally
------------------

.. tab-set::

   .. tab-item:: If you created a ``pyproject.toml``

      Add the following to your ``pyproject.toml`` file.

      .. code-block:: toml

         [dependency-groups]
         dev = [
            "coconext",
         ]

      This creates a new dependency group called ``dev`` which contains your ``coconext`` dependency.
      After this, you can install the dependencies in this group with the following command.

      .. code-block:: sh

         pip install --group dev

   .. tab-item:: If you did not create a ``pyproject.toml``

      .. code-block:: sh

         pip install coconext

      This installs ``coconext`` into your virtual environment without any kind of dependency or version tracking.

Installing In User-global Environment
-------------------------------------

To install ``coconext`` globally with ``pip``, run the following command.

.. code-block:: sh

   pip install --user coconext

.. note::
   Modifying the system-global environment of your operating system's provided Python is not recommended,
   and is disallowed on many operating systems.
   The operating system uses the system package manager instead of ``pip`` to manage this environment,
   and system utilities written in Python use this environment.
   Modifying this environment can break system utilities and make your system unusable.
   For those reasons, the ``--user`` flag is required so that packages are installed user-global.
