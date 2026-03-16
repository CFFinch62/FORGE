"""
Terminal Widget for FORGE IDE
Provides an output terminal for running FORGE programs
"""

import subprocess
import os
from pathlib import Path
from PyQt6.QtWidgets import QWidget, QVBoxLayout, QPlainTextEdit, QHBoxLayout, QPushButton
from PyQt6.QtCore import Qt, pyqtSignal, QProcess, QThread
from PyQt6.QtGui import QFont, QColor, QTextCursor, QKeyEvent, QKeySequence

from forge_ide.app.themes import Theme
from forge_ide.app.settings import SettingsManager


class TerminalOutput(QPlainTextEdit):
    """Custom terminal output widget that handles user input"""

    input_submitted = pyqtSignal(str)  # Signal emitted when user presses Enter

    def __init__(self, parent=None):
        super().__init__(parent)
        self.input_start_pos = 0  # Position where current input line starts
        self.input_enabled = False  # Whether input is currently allowed

    def keyPressEvent(self, event: QKeyEvent):
        """Handle key presses for input"""
        if not self.input_enabled:
            # If input is disabled, only allow navigation and copying
            if event.key() in (Qt.Key.Key_Up, Qt.Key.Key_Down, Qt.Key.Key_Left,
                             Qt.Key.Key_Right, Qt.Key.Key_Home, Qt.Key.Key_End,
                             Qt.Key.Key_PageUp, Qt.Key.Key_PageDown):
                super().keyPressEvent(event)
            elif event.matches(QKeySequence.StandardKey.Copy):
                super().keyPressEvent(event)
            return

        # Input is enabled - handle text input
        cursor = self.textCursor()

        # Don't allow editing before the input start position
        if cursor.position() < self.input_start_pos:
            if event.key() not in (Qt.Key.Key_Up, Qt.Key.Key_Down, Qt.Key.Key_Left,
                                   Qt.Key.Key_Right, Qt.Key.Key_Home, Qt.Key.Key_End):
                cursor.setPosition(self.input_start_pos)
                self.setTextCursor(cursor)

        # Handle Enter key - submit input
        if event.key() in (Qt.Key.Key_Return, Qt.Key.Key_Enter):
            # Get the input text (from input_start_pos to end)
            cursor.setPosition(self.input_start_pos)
            cursor.movePosition(QTextCursor.MoveOperation.End, QTextCursor.MoveMode.KeepAnchor)
            input_text = cursor.selectedText()

            # Move cursor to end and add newline
            cursor.clearSelection()
            cursor.movePosition(QTextCursor.MoveOperation.End)
            self.setTextCursor(cursor)
            self.appendPlainText("")  # Add newline

            # Emit the input
            self.input_submitted.emit(input_text)

            # Update input start position for next input
            cursor.movePosition(QTextCursor.MoveOperation.End)
            self.input_start_pos = cursor.position()
        else:
            # Handle normal text input
            super().keyPressEvent(event)

    def enable_input(self):
        """Enable input mode and mark where input starts"""
        self.input_enabled = True
        cursor = self.textCursor()
        cursor.movePosition(QTextCursor.MoveOperation.End)
        self.setTextCursor(cursor)
        self.input_start_pos = cursor.position()
        self.setReadOnly(False)

    def disable_input(self):
        """Disable input mode"""
        self.input_enabled = False
        self.setReadOnly(True)


class TerminalWidget(QWidget):
    """Terminal widget for program output"""
    
    execution_finished = pyqtSignal(int)  # Exit code
    
    def __init__(self, parent=None, theme: Theme = None, settings: SettingsManager = None):
        super().__init__(parent)
        self.theme = theme
        self.settings = settings
        self.process = None
        self._setup_ui()
    
    def _setup_ui(self):
        """Set up terminal UI"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        
        # Control bar
        control_layout = QHBoxLayout()
        control_layout.setContentsMargins(8, 4, 8, 4)
        
        self.clear_btn = QPushButton("Clear")
        self.clear_btn.setFixedHeight(28)
        self.clear_btn.clicked.connect(self.clear)
        control_layout.addWidget(self.clear_btn)
        
        self.stop_btn = QPushButton("Stop")
        self.stop_btn.setFixedHeight(28)
        self.stop_btn.clicked.connect(self.stop_execution)
        self.stop_btn.setEnabled(False)
        control_layout.addWidget(self.stop_btn)
        
        control_layout.addStretch()
        layout.addLayout(control_layout)
        
        # Output area
        self.output = TerminalOutput()
        self.output.setReadOnly(True)
        self.output.setLineWrapMode(QPlainTextEdit.LineWrapMode.WidgetWidth)
        self.output.input_submitted.connect(self._on_input_submitted)
        
        # Set monospace font
        if self.settings:
            font = QFont(self.settings.settings.terminal.font_family, 
                        self.settings.settings.terminal.font_size)
        else:
            font = QFont("JetBrains Mono", 11)
        font.setStyleHint(QFont.StyleHint.Monospace)
        self.output.setFont(font)
        
        layout.addWidget(self.output)
    
    def apply_settings(self):
        """Apply terminal settings (font)"""
        if self.settings:
            font = QFont(self.settings.settings.terminal.font_family,
                        self.settings.settings.terminal.font_size)
            font.setStyleHint(QFont.StyleHint.Monospace)
            self.output.setFont(font)
            # Reapply theme to ensure font is included in stylesheet
            if self.theme:
                self.apply_theme(self.theme)

    def apply_theme(self, theme: Theme):
        """Apply theme to terminal"""
        self.theme = theme

        # Get font settings
        if self.settings:
            font_family = self.settings.settings.terminal.font_family
            font_size = self.settings.settings.terminal.font_size
        else:
            font_family = "JetBrains Mono"
            font_size = 11

        self.output.setStyleSheet(f"""
            QPlainTextEdit {{
                background-color: {theme.terminal_background};
                color: {theme.terminal_foreground};
                border: none;
                padding: 8px;
                font-family: "{font_family}", monospace;
                font-size: {font_size}pt;
            }}
        """)

        self.setStyleSheet(f"""
            QWidget {{
                background-color: {theme.panel_background};
            }}
            QPushButton {{
                background-color: {theme.button_background};
                color: {theme.button_foreground};
                border: none;
                border-radius: 4px;
                padding: 4px 12px;
            }}
            QPushButton:hover {{
                background-color: {theme.button_hover};
            }}
        """)

        # Update viewport to ensure changes are visible
        self.output.viewport().update()
        self.output.update()
    
    def clear(self):
        """Clear terminal output"""
        self.output.clear()
    
    def write(self, text: str, color: str = None):
        """Write text to terminal"""
        
        # Check for clear screen sequence: \033[2J\033[H or just \033[2J
        # The runtime emits "\033[2J\033[H"
        
        # Simple detection for now - if text contains the sequence
        # We might need a proper ANSI parser later for more complex codes
        
        if "\033[2J" in text or "\x1b[2J" in text:
            self.clear()
            # Remove the sequence from text to avoid printing it?
            # Or just print the rest? 
            # If we clear, we probably want to start fresh.
            # Let's remove the clear sequence parts and print the rest if any.
            text = text.replace("\033[2J", "").replace("\x1b[2J", "")
            text = text.replace("\033[H", "").replace("\x1b[H", "")
            
            # If nothing left, valid clear
            if not text:
                return

        cursor = self.output.textCursor()
        cursor.movePosition(QTextCursor.MoveOperation.End)

        if color and self.theme:
            cursor.insertHtml(f'<span style="color: {color}">{text}</span>')
        else:
            cursor.insertText(text)

        self.output.setTextCursor(cursor)
        self.output.ensureCursorVisible()

        # Update input start position so user can't edit program output
        if self.output.input_enabled:
            cursor.movePosition(QTextCursor.MoveOperation.End)
            self.output.input_start_pos = cursor.position()
    
    def write_line(self, text: str, color: str = None):
        """Write a line to terminal"""
        self.write(text + "\n", color)
    
    def run_forge_file(self, file_path: str, forge_executable: str = None):
        """Run a FORGE file using the FORGE interpreter"""
        if self.process is not None:
            self.write_line("[!] A program is already running!", self.theme.warning if self.theme else None)
            return

        # Find FORGE executable using multiple strategies
        if forge_executable is None:
            forge_executable = self._find_forge_interpreter()

        if not forge_executable:
            self.clear()
            self.write_line("[ERROR] FORGE interpreter not found!", self.theme.error if self.theme else None)
            self.write_line("")
            self.write_line("Please ensure the 'forge' executable is:")
            self.write_line("  1. In your PATH, or")
            self.write_line("  2. In the same directory as the IDE")
            return

        self.clear()
        self.write_line(f"[>] Running: {file_path}", self.theme.info if self.theme else None)
        self.write_line(f"[>] Interpreter: {forge_executable}", self.theme.info if self.theme else None)

        # Build command arguments
        args = ["run", file_path]

        self.write_line("-" * 50)

        self.process = QProcess(self)
        self.process.readyReadStandardOutput.connect(self._on_stdout)
        self.process.readyReadStandardError.connect(self._on_stderr)
        self.process.finished.connect(self._on_finished)

        self.process.start(forge_executable, args)
        self.stop_btn.setEnabled(True)

        # Enable input so user can provide input when program requests it
        self.output.enable_input()
    
    def run_external_file(self, file_path: str):
        """Run a FORGE file in an external terminal"""
        forge_executable = self._find_forge_interpreter()
        if not forge_executable:
             self.write_line("[ERROR] FORGE interpreter not found!", self.theme.error if self.theme else None)
             return

        # Get configured command or auto-detect
        command_template = ""
        if self.settings:
            command_template = self.settings.settings.terminal.external_terminal_command
        
        if not command_template:
            # Auto-detect logic (duplicate of settings dialog for fallback)
            import sys
            import shutil
            
            if sys.platform == "win32":
                command_template = "start cmd /k"
            elif sys.platform == "darwin":
                command_template = "open -a Terminal"
            else:
                 terminals = [
                    ("gnome-terminal", "gnome-terminal --"),
                    ("konsole", "konsole -e"),
                    ("xfce4-terminal", "xfce4-terminal -x"),
                    ("x-terminal-emulator", "x-terminal-emulator -e"),
                    ("xterm", "xterm -e"),
                    ("urxvt", "urxvt -e"),
                ]
                 for term, cmd in terminals:
                    if shutil.which(term):
                        command_template = cmd
                        break
            
            # Save detected command if found
            if command_template and self.settings:
                self.settings.settings.terminal.external_terminal_command = command_template
                self.settings.save()
        
        if not command_template:
             self.write_line("[ERROR] No supported external terminal found. Please configure one in Preferences.", self.theme.error if self.theme else None)
             return

        # Prepare the command to run
        # We need to keep the window open after execution
        import sys
        import shlex
        
        full_command = []
        
        # Parse the template
        # If {command} placeholder exists, use it. Otherwise append command to end.
        if "{command}" in command_template:
            # complex template
            pass # TODO: Implement complex template parsing if needed
        
        # Simpler approach: split template into parts
        cmd_parts = shlex.split(command_template)
        full_command.extend(cmd_parts)
        
        # Construct the minimal command to run the script and pause
        # e.g. forge run /path/to/file.fg
        
        if sys.platform == "win32":
            # Windows: cmd /k keeps window open
            forge_executable_abs = str(Path(forge_executable).resolve())
            file_path_abs = str(Path(file_path).resolve())
            file_dir = str(Path(file_path).parent.resolve())

            import tempfile
            with tempfile.NamedTemporaryFile(mode='w', suffix='.bat', delete=False) as f:
                f.write('@echo off\n')
                f.write(f'cd /d "{file_dir}"\n')
                f.write(f'"{forge_executable_abs}" run "{file_path_abs}"\n')
                f.write('echo.\n')
                f.write('pause\n')
                temp_batch = f.name

            if cmd_parts[0] == "start":
                win_cmd = f'start "" "{temp_batch}"'
            else:
                win_cmd = f'{command_template} "{temp_batch}"'

            subprocess.Popen(win_cmd, shell=True)

        elif sys.platform == "darwin":
            import tempfile
            import stat

            with tempfile.NamedTemporaryFile(mode='w', suffix='.command', delete=False) as f:
                f.write('#!/bin/bash\n')
                f.write(f'"{forge_executable}" run "{file_path}"\n')
                f.write('read -p "Press Enter to close..."\n')
                temp_script = f.name

            os.chmod(temp_script, os.stat(temp_script).st_mode | stat.S_IEXEC)
            subprocess.Popen(["open", "-a", "Terminal", temp_script])

        else:
            # Linux
            wrapper_cmd = f'"{forge_executable}" run "{file_path}"; echo; read -p "Press Enter to close..."'
            run_args = ["bash", "-c", wrapper_cmd]
            final_cmd = list(cmd_parts)
            final_cmd.extend(run_args)

            try:
                subprocess.Popen(final_cmd)
                self.write_line(f"[>] Launched external terminal: {' '.join(final_cmd)}")
            except Exception as e:
                self.write_line(f"[ERROR] Failed to launch terminal: {e}", self.theme.error if self.theme else None)

    def _find_forge_interpreter(self) -> str:
        """Find the FORGE interpreter using multiple strategies"""
        import shutil
        import sys

        # Strategy 1: Check settings for configured path
        if self.settings and self.settings.settings.forge_interpreter_path:
            path = Path(self.settings.settings.forge_interpreter_path)
            if path.exists() and path.is_file():
                return str(path)

        # Strategy 2: Check PATH
        forge_in_path = shutil.which("forge")
        if forge_in_path:
            return forge_in_path

        # Strategy 3: Check bundled location (PyInstaller _MEIPASS or exe directory)
        forge_name = "forge.exe" if sys.platform == "win32" else "forge"

        if getattr(sys, 'frozen', False):
            # PyInstaller 6.x: _MEIPASS points to _internal/ where binaries are bundled
            if hasattr(sys, '_MEIPASS'):
                meipass_forge = Path(sys._MEIPASS) / forge_name
                if meipass_forge.exists():
                    return str(meipass_forge)
            exe_dir = Path(sys.executable).parent
        else:
            # Running from source — forge_ide/app/terminal.py → go up to FORGE/
            exe_dir = Path(__file__).parent.parent.parent

        forge_exe = exe_dir / forge_name
        if forge_exe.exists():
            return str(forge_exe)

        # Strategy 4: Check parent directory
        if sys.platform == "win32":
            parent_forge = exe_dir.parent / "forge.exe"
            if parent_forge.exists():
                return str(parent_forge)

        parent_forge = exe_dir.parent / "forge"
        if parent_forge.exists():
            return str(parent_forge)

        return None
    
    def stop_execution(self):
        """Stop the running program"""
        if self.process:
            self.process.kill()
            self.output.disable_input()
            self.write_line("\n[STOPPED] Program terminated.", self.theme.warning if self.theme else None)
    
    def _on_stdout(self):
        """Handle stdout from process"""
        data = self.process.readAllStandardOutput().data().decode('utf-8', errors='replace')
        self.write(data)
    
    def _on_stderr(self):
        """Handle stderr from process"""
        data = self.process.readAllStandardError().data().decode('utf-8', errors='replace')
        self.write(data, self.theme.error if self.theme else None)
    
    def _on_finished(self, exit_code, exit_status):
        """Handle process completion"""
        # Disable input if it was enabled
        self.output.disable_input()

        self.write_line("-" * 50)
        if exit_code == 0:
            self.write_line(f"[OK] Program finished successfully.", self.theme.success if self.theme else None)
        else:
            self.write_line(f"[ERROR] Program exited with code {exit_code}", self.theme.error if self.theme else None)

        self.process = None
        self.stop_btn.setEnabled(False)
        self.execution_finished.emit(exit_code)

    def _on_input_submitted(self, text: str):
        """Handle input submitted by user"""
        if self.process:
            # Send input to process stdin
            data = (text + "\n").encode('utf-8')
            self.process.write(data)

