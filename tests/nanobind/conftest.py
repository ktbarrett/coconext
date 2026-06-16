from __future__ import annotations

import os
import sys

nb_so_directory = os.environ.get("NB_SO_DIR")

if nb_so_directory not in sys.path:
    sys.path.insert(0, nb_so_directory)
