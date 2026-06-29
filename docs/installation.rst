Installation
############

The following document provides a couple of different ways to install coconext into your HDL project.

If you are undecided on how to accomplish that, follow one of these sets of instructions:

.. tab-set::

   .. tab-item:: Installing for both C++ and Python APIs and cocotb integration

      1. Install ``uv``
      2. Install Python
      3. Create a ``pyproject.toml`` to track Python development dependencies
      4. Create and source a Python virtual environment
      5. Install ``coconext``

   .. tab-item:: Installing for C++ API only

      1. Install ``uv``
      2. Create a ``pyproject.toml`` to track Python development dependencies
      3. Create and source a Python virtual environment
      4. Install ``coconext``

      .. note::
         If you are wondering why we need to set up a Python virtual environment though you are only going to use the C++ API,
         that is because ``coconext`` is only distributed via `PyPI <https://pypi.org/>`_;
         meaning you need a Python package manager to install it and an environment to install it into.

   .. tab-item:: Installing without using ``uv``

      Recommended if you are on a system where ``uv`` is not available and you are unable to install it.

      Follow the instructions in :doc:`installation_without_uv`.

   .. tab-item:: Installing globally with ``uv``

      Not recommended, but documented here for completeness.

      1. Install ``uv``
      2. Install ``coconext`` :ref:`globally <installing-globally>`.

Installing ``uv``
=================

`uv <https://docs.astral.sh/uv/>`_ is a Python executable installer, virtual environment creator, and package manager all wrapped up into one tool.
It can even manage globally install "tools".
It is written in Rust and is quite fast, even for very large Python projects with lots of dependencies.
For those reasons and more, it is the recommended tool for the remaining steps, regardless of the path you take.

Installation instructions are `here <https://docs.astral.sh/uv/getting-started/installation/>`_.

.. _install-python:

Installing Python
=================

.. note::
   This step is only needed if you want to use the coconext Python API or cocotb integrations.

.. _install-python-with-uv:

Using ``uv``
------------

``uv`` is among many things, a Python executable installer.
You can install versions of Python using the following ``uv`` command.

.. code-block:: sh

   uv python install 3.14

You can also see what versions of Python are already available,
either through ``uv`` installs or ones that are included on your system by running the following command.

.. code-block:: sh

   uv python list

Using your system's package manager
-----------------------------------

If your system ships with a modern-enough version of Python and you would prefer using that distribution, you can.
To see if you have Python install and what the version is,
try the below commands in order and stop after the first succeeds.

.. code-block:: sh

   python --version
   python3 --version

Remember which command succeeded, that is the incantation of the ``python`` command you will be using.

The command will also print the version;
coconext and cocotb only support Python versions 3.9+.
If your system's version is too old, you may need to install a newer package,
or install a newer Python :ref:`using ``uv`` <install-python-with-uv>`.

To see what versions of Python are available on your system run the following.

.. tab-set::

   .. tab-item:: aptitude (Debian and Ubuntu)

      Only the system default version of Python is available.

   .. tab-item:: DNF/YUM (RedHat, Fedora, RockyLinux)

      dnf search python3

   .. tab-item:: homebrew (MacOS and Linux)

      brew search python

After you install a newer Python package, figure out what the ``python`` command is.
If it's not the standard version of Python distributed with your operating system, it is likely something of the form: ``python{major}.{minor}``, e.g. ``python3.12``.

Creating a ``pyproject.toml``
=============================

.. note::
   This step is optional, but highly recommended.

Creating a ``pyproject.toml`` allows you to track Python development dependencies for your project,
even if you do not plan on packaging your project as a Python package.
``uv`` makes this easy with the following command.

.. code-block:: sh

   uv init --app -p {python version}

The ``--app`` flag tells ``uv`` that this project is not intended to be built as a Python package.
The ``-p`` flag tells ``uv`` which Python version to use for the virtual environment it creates and the dependencies it installs.

Create and activate a Python virtual environment
================================================

Python virtual environments are where Python packages are installed into.
Technically there is only one Python environment, which is included alongside the executable.
So we create "virtual environments" to create environments per project which use the same Python executable.

Create a virtual environment
----------------------------

.. note::
   This step is only necessary if you did not create a ``pyproject.toml``.

Change directory to the root of your project if you haven't already.
This following command creates a virtual environment in the directory `.venv/`.

.. code-block:: sh

   uv venv -p {python version}

Activate your virtual environment
---------------------------------

This depends on the shell you are using,
but for all Unix-like shells this is equivalent.

.. code-block:: sh

   . .venv/bin/activate

This may add additional information to your terminal's prompt.
But you can check to see if it's working by running the following command *exactly*:
virtual environments normalize the ``python`` executable name.

.. code-block:: sh

   command -v python

If the printed path points inside the `.venv/` directory we just created, it is working as intended.

.. _use-direnv:

Automatic virtual environment activation with ``direnv``
--------------------------------------------------------

Since we are going to need to activate our virtual environment every time we work on our project,
we can make it easier on ourselves by auto-activating our virtual environment whenever we navigate into the project directory.

`direnv <https://direnv.net/>`_ is a tool which does exactly that:
it runs arbitrary actions when a directory or sub-directory is entered or exited.
We can use it to auto-activate our virtual environment.

First follow `direnv's installation instructions <https://direnv.net/docs/installation.html>`_.
Next, create ``.envrc`` in your project's root.

.. code-block:: sh

   echo "layout python" > .envrc

You'll see an error message the command after the file is saved stating the directory isn't currently in the allow list.
Add it using:

.. code-block:: sh

   direnv allow

Then navigate out of the project directory and back in to ensure the virtual environment is being deactivated and re-activated correctly.

Installing ``coconext``
=======================

System prerequisites
--------------------

Currently, there are no system library prerequisites.

Installing In Project-local Environment
----------------------------------------

To install ``coconext`` into your project's virtual environment
ensure the virtual environment is activated and run the most appropriate of the following.

.. tab-set::

   .. tab-item:: If you created a ``pyproject.toml``

      This adds ``coconext`` as a dependency on your project and also installs it into your virtual environment.

      .. code-block:: sh

         uv add coconext

      You can now optionally `lock this dependency <https://docs.astral.sh/uv/concepts/projects/sync/>`_ to a particular version,
      so that other developers and your CI will install the same version of ``coconext`` as you have installed.

   .. tab-item:: If you did not create a ``pyproject.toml``

      This just installs ``coconext`` into your virtual environment without any kind of dependency or version tracking.

      .. code-block:: sh

         uv pip install coconext

.. _installing-globally:

Installing In User-global Environment
-------------------------------------

You can install ``coconext`` into the user-global environment if desired to side-step having to create a virtual environment.
This prevents you from being able to share this dependency with other developers or your CI.

First ensure no virtual environments are currently activated.
Then run the following.

.. code-block:: sh

   uv pip install coconext
