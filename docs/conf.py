# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html
from __future__ import annotations

import pathlib
import sys

from coconext import __version__

# Add current directory to path to pick up local extensions
sys.path.insert(0, pathlib.Path.cwd().as_posix())

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = "coconext"
copyright = "2025, Kaleb Barrett"
author = "Kaleb Barrett"
version = ".".join(__version__.split(".", 3)[:2])
release = __version__

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
    "sphinx.ext.autodoc",
    "sphinx.ext.napoleon",
    "sphinx.ext.intersphinx",
    "sphinx_autodoc_typehints",
    "sphinxcontrib.prettyspecialmethods",
    "sphinx_rtd_theme",
    "ignore_undoc_special_methods",
]

templates_path = ["_templates"]

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = "sphinx_rtd_theme"
html_static_path = ["_static"]

# Options for RTD Theme
################################################################################

github_url = "https://github.com/ktbarrett/coconext/"

# Options for autodoc
################################################################################

autoclass_content = "class"
autodoc_member_order = "bysource"
autodoc_default_options = {
    "members": True,
    "special-members": True,
    "exclude-members": "__weakref__",
    "show-inheritance": True,
}
autodoc_typehints = "signature"
autodoc_class_signature = "separated"

# Options for intersphinx
################################################################################

intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
    "cocotb": ("https://docs.cocotb.org/en/development/", None),
}
