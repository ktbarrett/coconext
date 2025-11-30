from __future__ import annotations

import re
from typing import TYPE_CHECKING, Any

if TYPE_CHECKING:
    from sphinx.application import Sphinx

__version__ = "0.1.0"


special_method_re = re.compile(r"^__.*__$")


def ignore_undocumented_special_methods(
    app: Sphinx, what: str, name: str, obj: object, skip: bool, options: Any
) -> bool | None:
    # Setting special-methods to True in autodoc config will sometimes output
    # special methods without docstrings. These should be ignored.
    if (
        what == "class"
        and special_method_re.match(name)
        and not getattr(obj, "__doc__", None)
    ):
        return True
    return None


def setup(app: Sphinx) -> dict[str, object]:
    app.setup_extension("sphinx.ext.autodoc")
    app.connect("autodoc-skip-member", ignore_undocumented_special_methods)
    return {"version": __version__, "parallel_read_safe": True}
