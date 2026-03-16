#!/usr/bin/env python3
"""
FORGE IDE - A modern IDE for the FORGE programming language
Based on the PLAIN IDE architecture
"""

import sys
import os

from PyQt6.QtWidgets import QApplication
from PyQt6.QtCore import Qt
from PyQt6.QtGui import QFont, QIcon

from forge_ide.app.main_window import ForgeIDEMainWindow
from forge_ide.app.settings import SettingsManager


def main():
    """Main entry point for the FORGE IDE"""
    # Enable high DPI scaling
    app = QApplication(sys.argv)
    app.setApplicationName("FORGE IDE")
    app.setApplicationVersion("1.0.0")
    app.setOrganizationName("FORGE Language")

    # Load settings
    settings = SettingsManager()

    # Apply theme
    from forge_ide.app.themes import ThemeManager
    theme_manager = ThemeManager(settings)
    app.setStyleSheet(theme_manager.get_current_stylesheet())

    # Create and show main window
    window = ForgeIDEMainWindow(settings, theme_manager)
    window.show()

    sys.exit(app.exec())


if __name__ == "__main__":
    main()

