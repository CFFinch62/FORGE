<img src="../images/forge_banner.svg" alt="FORGE IDE" width="100%"/>

# FORGE IDE

A modern, feature-rich IDE for the FORGE programming language.

## Features

- **Syntax Highlighting** - Color-coded FORGE syntax with support for keywords, types, operators, and comments
- **Code Editor** - Modern editing with line numbers, auto-indent, and code folding
- **Nested Scope Coloring** - BlueJ-style colored backgrounds showing block nesting at a glance
- **File Browser** - Navigate and manage your FORGE project files
- **Bookmarks** - Mark and navigate important code locations
- **Find/Replace** - Powerful search and replace across files
- **Themes** - 50+ syntax color themes included
- **Session Persistence** - Remembers your open files and settings between sessions
- **Integrated Terminal** - Run FORGE programs directly inside the IDE

## Installation

### Prerequisites

- Python 3.8 or higher
- pip (Python package manager)
- The `forge` compiler binary on your system PATH (or configured in Preferences)

### Install Dependencies

```bash
# Navigate to the IDE directory
cd forge_ide

# Install required packages
pip install -r requirements.txt
```

### Run the IDE

```bash
# From the forge_ide directory
python main.py

# Or from the FORGE project directory
python -m forge_ide.main
```

### First-Time Setup

1. **Configure Compiler Path:**
   - Settings → Preferences → Interpreter
   - Set the path to the `forge` compiler executable
   - Default locations checked automatically:
     - Linux/macOS: `forge` on PATH, or `./forge`
     - Windows: `forge.exe` on PATH, or `.\forge.exe`

2. **Choose Theme:**
   - Settings → Preferences → Appearance
   - Select from 50+ available syntax themes

## Usage

### Creating a New File

1. File → New (`Ctrl+N`)
2. Write your FORGE code
3. File → Save (`Ctrl+S`)
4. Save with the `.fg` extension

### Running Code

1. Run → Run Program (`F5`)
2. Or click the ▶ Run button in the toolbar
3. Output appears in the integrated terminal panel

### Keyboard Shortcuts

| Action | Windows/Linux | macOS |
|--------|---------------|-------|
| New File | `Ctrl+N` | `Cmd+N` |
| Open File | `Ctrl+O` | `Cmd+O` |
| Save | `Ctrl+S` | `Cmd+S` |
| Save As | `Ctrl+Shift+S` | `Cmd+Shift+S` |
| Run | `F5` | `F5` |
| Find | `Ctrl+F` | `Cmd+F` |
| Replace | `Ctrl+H` | `Cmd+H` |
| Comment/Uncomment | `Ctrl+/` | `Cmd+/` |

### Nested Scope Coloring

The editor paints nested, colored backgrounds behind each block of
code — similar to BlueJ — so you can see where a `proc`, `if`, `while`,
`for`, etc. body starts and ends just by looking at the background,
instead of only inferring it from indentation. Each level of nesting
gets its own color, drawn behind the syntax-highlighted text, and
recomputes automatically a moment after you stop typing.

**To toggle it on or off:**

- **View** menu → **Show Nested Scope Coloring**, or
- **Settings → Preferences** (`Ctrl+,`) → **Editor** tab → **"Show nested scope boxes"**

Both controls stay in sync with each other.

**To customize the colors:**

1. Open **Settings → Preferences** (`Ctrl+,`) → **Editor** tab
2. Under **Nested Scope Coloring**, click a depth's color swatch to open a color picker and choose a custom color for that nesting level
3. Click **Reset to Theme Defaults** at any time to go back to the colors defined by your current UI theme

If you never customize the colors, they automatically follow whichever UI theme you have selected (**View → Theme → UI Theme**), so switching themes keeps the scope colors looking coherent with the rest of the IDE.

## Troubleshooting

### "No module named PyQt6"

Install the required dependencies:
```bash
pip install -r requirements.txt
```

### "Cannot find FORGE compiler"

1. Check the compiler path in Settings → Preferences
2. Ensure the `forge` binary is on your PATH or specify the full path
3. Verify the compiler works: `forge --version`

### IDE Won't Start

```bash
# Check Python version (needs 3.8+)
python --version

# Try running with verbose output
python main.py
```

### High DPI Display Issues

The IDE automatically detects high DPI displays. If you experience scaling issues:

1. Settings → Preferences → Appearance
2. Adjust the font size
3. Restart the IDE

## Development

### Project Structure

```
forge_ide/
├── app/                    # Main application code
│   ├── main_window.py      # Main window
│   ├── editor.py           # Code editor widget
│   ├── syntax.py           # FORGE syntax highlighter
│   ├── terminal.py         # Integrated terminal / run panel
│   ├── themes.py           # Theme loading and management
│   ├── settings.py         # Settings persistence
│   ├── settings_dialog.py  # Preferences dialog
│   ├── file_browser.py     # File browser panel
│   ├── find_replace.py     # Find/Replace dialog
│   └── help_viewer.py      # Quick reference viewer
├── images/                 # Application icons
├── themes/syntax/          # Bundled syntax color themes
├── main.py                 # Entry point
└── requirements.txt        # Python dependencies
```

### Building a Standalone Executable

A `forge_ide.spec` file is provided in the `FORGE/` project root for bundling the IDE
together with the `forge` compiler into a self-contained distribution using PyInstaller.

```bash
# From the FORGE/ project directory
pip install pyinstaller
pyinstaller forge_ide.spec
```

The output is placed in `dist/ForgeIDE/`:

```
dist/ForgeIDE/
├── ForgeIDE              ← main executable
└── _internal/
    ├── forge             ← bundled FORGE compiler
    ├── images/           ← application icons
    └── themes/syntax/    ← bundled syntax themes
```

On first launch, syntax themes are automatically copied to `~/.config/forge_ide/themes/syntax/`
so the user can customize them without affecting the installation.

## Support

For issues, feature requests, or questions:
- GitHub Issues: https://github.com/CFFinch62/forge-language/issues
- Email: info@fragillidaesoftware.com

## License

Proprietary - See LICENSE file in the parent directory.

Copyright (c) 2026 Fragillidae Software
