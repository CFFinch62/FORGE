# -*- mode: python ; coding: utf-8 -*-
# PyInstaller spec file for FORGE IDE
#
# Run from the FORGE/ directory:
#   pyinstaller forge_ide.spec
#
# Output: dist/ForgeIDE/
#   ForgeIDE            ← main executable
#   _internal/          ← PyInstaller support files
#     forge             ← bundled FORGE compiler
#     images/           ← application icons
#     themes/syntax/    ← bundled syntax themes (copied to ~/.config/forge_ide on first run)
#     forge_ide/        ← Python package

block_cipher = None

a = Analysis(
    ['forge_ide/main.py'],
    pathex=['.'],
    binaries=[
        # Bundle the FORGE compiler alongside the IDE.
        # In PyInstaller 6.x onedir mode this lands in _internal/ (sys._MEIPASS).
        ('forge', '.'),
    ],
    datas=[
        # Application icons — resolved by get_resource_path("images/...")
        ('forge_ide/images', 'images'),
        # Bundled syntax themes — copied to ~/.config/forge_ide/themes/syntax/ on first run.
        # Resolved by ThemeManager._get_bundled_themes_dir() via sys._MEIPASS.
        ('forge_ide/themes/syntax', 'themes/syntax'),
        # Quick reference and documentation — resolved by get_resource_path("user_docs/...")
        # Source is FORGE/user_docs/ (relative to the FORGE/ cwd where pyinstaller is run).
        ('user_docs', 'user_docs'),
    ],
    hiddenimports=[
        # Ensure all forge_ide sub-modules are included (they may not be auto-detected
        # because main.py imports some of them lazily inside functions).
        'forge_ide',
        'forge_ide.app',
        'forge_ide.app.main_window',
        'forge_ide.app.editor',
        'forge_ide.app.syntax',
        'forge_ide.app.terminal',
        'forge_ide.app.settings',
        'forge_ide.app.settings_dialog',
        'forge_ide.app.themes',
        'forge_ide.app.utils',
        'forge_ide.app.file_browser',
        'forge_ide.app.find_replace',
        'forge_ide.app.help_viewer',
        # PyQt6
        'PyQt6',
        'PyQt6.QtWidgets',
        'PyQt6.QtCore',
        'PyQt6.QtGui',
        'PyQt6.QtPrintSupport',
        # stdlib used at runtime
        'configparser',
        'json',
        'shutil',
        'subprocess',
    ],
    hookspath=[],
    hooksconfig={},
    runtime_hooks=[],
    excludes=[
        # Exclude heavy scientific packages that are not needed
        'tkinter',
        'matplotlib',
        'numpy',
        'scipy',
        'pandas',
        'PIL',
    ],
    win_no_prefer_redirects=False,
    win_private_assemblies=False,
    cipher=block_cipher,
    noarchive=False,
)

pyz = PYZ(a.pure, a.zipped_data, cipher=block_cipher)

exe = EXE(
    pyz,
    a.scripts,
    [],
    exclude_binaries=True,   # onedir mode — separate COLLECT step below
    name='ForgeIDE',
    debug=False,
    bootloader_ignore_signals=False,
    strip=False,
    upx=True,
    console=False,           # GUI application — no console window
    disable_windowed_traceback=False,
    argv_emulation=False,
    target_arch=None,
    codesign_identity=None,
    entitlements_file=None,
    # Icon used for the executable (Windows .ico; ignored on Linux)
    icon='forge_ide/images/forge_icon.ico',
)

coll = COLLECT(
    exe,
    a.binaries,
    a.zipfiles,
    a.datas,
    strip=False,
    upx=True,
    upx_exclude=[],
    name='ForgeIDE',
)

