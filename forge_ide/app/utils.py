"""
FORGE IDE Application Utilities
"""

import sys
import os
from pathlib import Path


def get_resource_path(relative_path: str) -> str:
    """
    Get absolute path to resource, works for dev and for PyInstaller.
    
    Handles:
    - PyInstaller onefile mode (_MEIPASS)
    - PyInstaller onedir mode (relative to executable)
    - Development mode (relative to source root)
    - Symlinks to the executable (resolves real path)
    """
    if getattr(sys, 'frozen', False):
        # PyInstaller creates a temp folder and stores path in _MEIPASS,
        if hasattr(sys, '_MEIPASS'):
             base_path = sys._MEIPASS
        else:
             # For onedir mode, resources are relative to the executable
             # Resolve symlinks to find the real executable location
             base_path = os.path.dirname(os.path.realpath(sys.executable))
    else:
        # Dev mode: relative to project root
        # This file is in forge_ide/app/utils.py, so go up 3 levels
        base_path = str(Path(__file__).parent.parent.parent)

    return str(Path(base_path) / relative_path)
