#!/usr/bin/env python3
"""
OFS File System Client - Complete Implementation
Full-featured terminal UI with ALL server commands
"""

from textual.app import App, ComposeResult
from textual.containers import Container, Horizontal, Vertical, ScrollableContainer
from textual.widgets import (
    Header, Footer, Button, Static, Input, 
    Label, DataTable, TabbedContent, TabPane,
    TextArea
)
from textual.screen import Screen, ModalScreen
from textual.binding import Binding
import socket
import time

PASSWORD = ""
class OFSConnection:
    """Manages connection to OFS server (robust send/recv)."""

    def __init__(self, host='127.0.0.1', port=8080):
        self.host = host
        self.port = port
        self.sock = None
        self.session_id = None
        self.logged_in = False
        self.username = None

    def connect(self, timeout=5.0) -> bool:
        """Connect to server and read welcome message (non-blocking read logic)."""
        try:
            if self.sock:
                try:
                    self.sock.close()
                except:
                    pass
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(timeout)
            self.sock.connect((self.host, self.port))
            # read any welcome message (short wait)
            welcome = self.receive_response(timeout_after_first=0.25)
            if welcome:
                print("Server welcome:", welcome.strip().replace("\n", "\\n")[:200])
            # make socket blocking again with a reasonable default
            self.sock.settimeout(None)
            return True
        except Exception as e:
            print(f"[OFSConnection.connect] Connection error: {e}")
            self.sock = None
            return False

    def close(self):
        try:
            if self.sock:
                self.sock.close()
        finally:
            self.sock = None
            self.logged_in = False
            self.username = None

    def ensure_connected(self) -> bool:
        if self.sock is None:
            return self.connect()
        return True

    def receive_response(self, timeout_after_first: float = 0.2, max_bytes: int = 64 * 1024) -> str:
        """
        Read data from socket until a short idle timeout after first byte.
        Returns accumulated decoded string (may contain multiple JSON objects).
        """
        if not self.sock:
            return ""
        # Save current blocking & timeout state, then set a small timeout
        prev_timeout = self.sock.gettimeout()
        try:
            self.sock.settimeout(timeout_after_first)
            chunks = []
            total = 0
            while True:
                try:
                    data = self.sock.recv(4096)
                    if not data:
                        break
                    chunks.append(data)
                    total += len(data)
                    if total >= max_bytes:
                        break
                    # keep looping until a recv times out (means server is idle)
                    # set a short timeout each iteration to detect inactivity
                    self.sock.settimeout(timeout_after_first)
                except socket.timeout:
                    # no more data arrived within timeout_after_first
                    break
                except Exception as e:
                    print(f"[receive_response] socket recv error: {e}")
                    break
            if not chunks:
                return ""
            try:
                return b"".join(chunks).decode(errors='replace')
            except Exception:
                return "".join([c.decode(errors='replace') for c in chunks])
        finally:
            # restore previous timeout
            try:
                self.sock.settimeout(prev_timeout)
            except:
                pass

    def receive_multiple_responses(self) -> str:
        """
        Convenience wrapper — same as receive_response but slightly longer timeout.
        Useful for DIR_LIST/LIST_USERS responses which can be multi-line/multi-json.
        """
        return self.receive_response(timeout_after_first=0.4)

    def send_command(self, command: str) -> str:
        """
        Send a single-line command and return the server response (or aggregated responses).
        For multi-response commands, return whatever receive_multiple_responses collects.
        """
        if not self.ensure_connected():
            return "Error: not connected"

        try:
            # ensure command ends with newline
            out = command if command.endswith("\n") else command + "\n"
            self.sock.sendall(out.encode())

            # Which commands commonly return multiple JSON blobs?
            multi_cmds = ("DIR_LIST", "LIST_USERS")
            if any(command.startswith(c) for c in multi_cmds):
                return self.receive_multiple_responses()
            else:
                return self.receive_response()
        except Exception as e:
            # try to recover a bit
            err = f"Error: {e}"
            print(f"[send_command] {err} (command: {command})")
            try:
                self.close()
            except:
                pass
            return err

    def send_with_content(self, command: str, content: str) -> str:
        """
        Send command that requires content (CREATE, EDIT).
        Flow:
         - send command line
         - wait for server prompt (receive)
         - send content
         - send newline + <<<EOF>>> + newline marker
         - wait for final response
        """
        if not self.ensure_connected():
            return "Error: not connected"

        try:
            # Step 1: send command line
            out = command if command.endswith("\n") else command + "\n"
            self.sock.sendall(out.encode())

            # Step 2: wait for server prompt (shorter timeout)
            prompt = self.receive_response(timeout_after_first=0.5)
            # (server should respond with a prompt message in JSON)
            # Step 3: send content and EOF marker exactly as server expects
            # Ensure content ends with newline so EOF marker is on its own line
            content_bytes = content.encode()
            if not content_bytes.endswith(b"\n"):
                content_bytes += b"\n"
            eof_marker = b"<<<EOF>>>"
            # send content then marker on its own line
            self.sock.sendall(content_bytes + eof_marker + b"\n")
            # Step 4: receive final result
            result = self.receive_response(timeout_after_first=0.6)
            # return both prompt and result (helpful for debugging) or just result
            # but to keep backward compatibility, return result (server JSON)
            if result:
                return result
            # If no result, but prompt existed, return the prompt for diagnosis
            return prompt if prompt else "Error: no response"
        except Exception as e:
            err = f"Error in send_with_content: {e}"
            print(f"[send_with_content] {err} (command: {command})")
            try:
                self.close()
            except:
                pass
            return err

    # Helpers: convenient wrappers
    def login(self, username: str, password: str) -> bool:
        response = self.send_command(f"LOGIN {username} {password}")
        if "SUCCESS_LOGIN" in response:
            self.logged_in = True
            self.username = username
            return True
        return False

    def logout(self) -> bool:
        response = self.send_command("LOGOUT")
        self.logged_in = False
        self.username = None
        # server returns "SUCCESS_LOGOUT" or similar; accept any SUCCESS
        return "SUCCESS" in response

class ConfirmDialog(ModalScreen):
    """Confirmation dialog"""
    
    def __init__(self, message, callback):
        super().__init__()
        self.message = message
        self.callback = callback
    
    def compose(self) -> ComposeResult:
        yield Container(
            Static(self.message, id="confirm_message"),
            Horizontal(
                Button("Yes", variant="success", id="yes_btn"),
                Button("No", variant="error", id="no_btn"),
                classes="button_row"
            ),
            id="confirm_dialog"
        )
    
    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "yes_btn":
            self.callback(True)
        self.app.pop_screen()


class InputDialog(ModalScreen):
    """Input dialog for user input"""
    
    BINDINGS = [
        ("escape", "cancel", "Cancel"),
    ]
    
    def __init__(self, title, fields, callback):
        super().__init__()
        self.title = title
        self.fields = fields  # List of (label, placeholder, is_password, is_multiline)
        self.callback = callback
    
    def compose(self) -> ComposeResult:
        container_items = [Static(self.title, classes="dialog_title")]
        
        for i, field_info in enumerate(self.fields):
            # Handle both old format (3 items) and new format (4 items)
            if len(field_info) == 3:
                label, placeholder, is_password = field_info
                is_multiline = False
            else:
                label, placeholder, is_password, is_multiline = field_info
            
            container_items.append(Label(label))
            
            if is_multiline:
                # Use TextArea for multi-line content
                container_items.append(TextArea(placeholder, id=f"input_{i}"))
            else:
                # Use Input for single-line
                container_items.append(Input(
                    placeholder=placeholder,
                    password=is_password,
                    id=f"input_{i}"
                ))
        
        container_items.append(
            Horizontal(
                Button("OK", variant="success", id="ok_btn"),
                Button("Cancel", variant="error", id="cancel_btn"),
                classes="button_row"
            )
        )
        
        yield Container(*container_items, id="input_dialog")
    
    def on_mount(self) -> None:
        """Focus first input when dialog opens"""
        if len(self.fields) > 0:
            try:
                self.query_one("#input_0").focus()
            except:
                pass
    
    def action_cancel(self) -> None:
        """Cancel action"""
        self.app.pop_screen()
    
    def on_input_submitted(self, event: Input.Submitted) -> None:
        """Handle Enter key in input fields"""
        # When Enter is pressed, submit the form
        self.submit_form()
    
    def submit_form(self) -> None:
        """Collect values and submit"""
        values = []
        for i, field_info in enumerate(self.fields):
            try:
                # Check if it's multiline (TextArea) or single-line (Input)
                if len(field_info) == 4 and field_info[3]:  # is_multiline
                    widget = self.query_one(f"#input_{i}", TextArea)
                    values.append(widget.text)
                else:
                    widget = self.query_one(f"#input_{i}", Input)
                    values.append(widget.value)
            except Exception as e:
                print(f"Error getting input_{i}: {e}")
                values.append("")
        
        print(f"Input Dialog - Values collected: {values}")
        self.app.pop_screen()
        self.callback(values)
    
    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "ok_btn":
            self.submit_form()
            return
        
        if event.button.id == "cancel_btn":
            self.app.pop_screen()


class LoginScreen(Screen):
    """Login screen"""
    
    BINDINGS = [
        ("escape", "app.quit", "Exit"),
    ]
    
    def compose(self) -> ComposeResult:
        yield Container(
            Static("╔═══════════════════════════════════╗", classes="logo"),
            Static("║   OFS File System Client v2.0    ║", classes="logo"),
            Static("╚═══════════════════════════════════╝", classes="logo"),
            Static(""),
            Label("Username:", classes="label"),
            Input(placeholder="Enter username", id="username"),
            Static(""),
            Label("Password:", classes="label"),
            Input(placeholder="Enter password", password=True, id="password"),
            Static(""),
            Horizontal(
                Button("Login", variant="primary", id="login_btn"),
                Button("Exit", variant="error", id="exit_btn"),
                classes="button_row"
            ),
            Static("", id="message"),
            id="login_container"
        )
    
    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "login_btn":
            username = self.query_one("#username", Input).value
            password = self.query_one("#password", Input).value
            PASSWORD = password
            msg = self.query_one("#message", Static)
            msg.update("[yellow]Logging in...[/yellow]")
            
            connection = self.app.connection
            if connection.login(username, password):
                msg.update("[green]✓ Login successful![/green]")
                self.app.switch_to_main_screen()
            else:
                msg.update("[red]✗ Login failed! Check credentials.[/red]")
        
        elif event.button.id == "exit_btn":
            self.app.exit()


class FileBrowserPanel(Static):
    """File browser with all file operations"""
    
    def __init__(self, connection):
        super().__init__()
        self.connection = connection
        self.current_path = "/"
        self.selected_file = None
        self.selected_type = None   # "FILE" or "DIR"

    
    def compose(self) -> ComposeResult:
        yield Label(f"📁 Current: {self.current_path}", id="current_path")
        yield DataTable(id="file_table", cursor_type="row")
        
        # File operations row 1
        yield Horizontal(
            Button("📝 New File", id="create_file", variant="success"),
            Button("📂 New Dir", id="create_dir", variant="primary"),
            Button("✏️ Edit", id="edit_file"),
            Button("👁️ Read", id="read_file"),
            classes="button_row"
        )
        
        # File operations row 2
        yield Horizontal(
            Button("🔄 Rename", id="rename_file"),
            Button("✂️ Truncate", id="truncate_file"),
            Button("🗑️ Delete File", id="delete_file", variant="error"),
            Button("🗑️ Delete Dir", id="delete_dir", variant="error"),
            classes="button_row"
        )
        
        # File operations row 3
        yield Horizontal(
            Button("🔍 File Exists", id="file_exists"),
            Button("🔍 Dir Exists", id="dir_exists"),
            Button("📊 Metadata", id="get_metadata"),
            Button("🔄 Refresh", id="refresh"),
            classes="button_row"
        )
        
        # Permissions row
        yield Horizontal(
            Button("🔐 Set Permissions", id="set_permissions"),
            Button("👤 Set Owner", id="set_owner"),
            Button("⬇️ Enter Directory", id="enter_dir", variant="primary"),
            Button("⬆️ Go to Parent", id="go_up", variant="warning"),

            classes="button_row"
        )
    '''
    def find_start_path_for_user(self):
        """
        Find the deepest directory the current user owns or can access.
        """
        username = self.connection.username
        # Try common candidates first
        candidates = [f"/home/{username}", "/"]
        
        for path in candidates:
            meta = self.connection.send_command(f"GET_METADATA {path}")
            if not meta:
                continue
            # crude check for ownership (adjust depending on your server's JSON)
            if f'"owner":"{username}"' in meta:
                return path
        
        # If user owns /home/n1/n2, try to drill down recursively
        path = f"/home/{username}"
        parts = path.strip("/").split("/")
        current = "/"
        for p in parts:
            next_path = f"{current}/{p}".replace("//", "/")
            meta = self.connection.send_command(f"GET_METADATA {next_path}")
            if f'"owner":"{username}"' in meta:
                current = next_path
            else:
                break
        return current
    '''
    def find_all_owned_paths(self):
        """
        Temporarily login as admin to traverse '/' and find paths owned by the current user.
        """
        username = self.connection.username
        owned_paths = []

        # Save the current connection state
        old_user = self.connection.username
        old_pass = PASSWORD # in case your connection stores it

        # Login as admin temporarily
        self.connection.login("admin", "admin123")  # adjust admin password if needed

        def admin_scan(path):
            response = self.connection.send_command(f"DIR_LIST {path}")
            if not response or "ERROR" in response:
                return

            for json_obj in response.split('}'):
                if not json_obj.strip():
                    continue
                json_obj = json_obj.strip() + '}'
                try:
                    start = json_obj.find('"owner":"') + len('"owner":"')
                    end = json_obj.find('"', start)
                    owner = json_obj[start:end]

                    start2 = json_obj.find('"param2"')
                    start2 = json_obj.find('"', start2 + 8) + 1
                    end2 = json_obj.find('"', start2)
                    val = json_obj[start2:end2]

                    if owner == username:
                        owned_paths.append(path)

                    if val and ':' in val:
                        typ, name = val.split(':', 1)
                        if typ == 'D':
                            next_path = f"{path}/{name}".replace("//", "/")
                            admin_scan(next_path)
                except:
                    continue

        admin_scan("/")  # Start scanning from root

        # Restore the original user session
        self.connection.login(old_user, old_pass)

        if owned_paths:
            return owned_paths[-1]
        return "/"


    def on_mount(self) -> None:
        table = self.query_one("#file_table", DataTable)
        table.add_columns("Name", "Type", "Size", "Modified")

        # Set starting path dynamically based on user ownership
        self.current_path = self.find_all_owned_paths()
        
        self.refresh_files()

    '''
    def on_mount(self) -> None:
        table = self.query_one("#file_table", DataTable)
        table.add_columns("Name", "Type", "Size", "Modified")

        # Set starting path dynamically based on user ownership
        self.current_path = self.find_start_path_for_user()
        
        self.refresh_files()
    '''
    '''
    def on_mount(self) -> None:
        table = self.query_one("#file_table", DataTable)
        table.add_columns("Name", "Type", "Size", "Modified")
        self.refresh_files()
    '''
    def on_data_table_row_selected(self, event: DataTable.RowSelected) -> None:
        table = self.query_one("#file_table", DataTable)
        row = table.get_row(event.row_key)
        filename = str(row[0])
        filetype = str(row[1])

        if filename == "..":
            # Navigate to parent directory
            if self.current_path != "/":
                parts = self.current_path.rstrip('/').split('/')
                if len(parts) > 1:
                    self.current_path = '/'.join(parts[:-1])
                    if not self.current_path:
                        self.current_path = "/"
                else:
                    self.current_path = "/"
                self.refresh_files()
        elif filename != "(empty)":
            self.selected_file = filename
            self.selected_type = filetype  # track the type
            self.app.notify(f"Selected: {filename} ({filetype}). Double-click or press Enter to open")

    def on_data_table_row_highlighted(self, event: DataTable.RowHighlighted) -> None:
        table = self.query_one("#file_table", DataTable)
        try:
            row = table.get_row(event.row_key)
            filename = str(row[0])
            filetype = str(row[1])
            if filename != "(empty)":
                self.selected_file = filename
                self.selected_type = filetype
        except:
            pass

    def on_key(self, event) -> None:
        if event.key == "enter" and self.selected_file:
            full_path = f"{self.current_path}/{self.selected_file}".replace("//", "/")
            # if we know the type from the table, use it
            if self.selected_type == "D":
                self.current_path = full_path
                self.refresh_files()
                return

            # otherwise attempt to dir-list and fall back to read
            response = self.connection.send_command(f"DIR_LIST {full_path}")
            if '"entry"' in response and ('"param2"' in response and 'D:' in response):
                # server treats it as directory
                self.current_path = full_path
                self.refresh_files()
            else:
                # read file
                result = self.connection.send_command(f"READ {full_path}")
                class ReadDialog(ModalScreen):
                    def __init__(self, path, content):
                        super().__init__()
                        self.path = path
                        self.content = content
                    def compose(self):
                        yield Container(
                            Label(f"Content of {self.path}"),
                            ScrollableContainer(Static(self.content, id="file_content"), id="content_scroll"),
                            Button("Close", id="close_btn"),
                            id="read_dialog"
                        )
                    def on_button_pressed(self, event):
                        self.app.pop_screen()
                self.app.push_screen(ReadDialog(full_path, result))

    def parse_file_list(self, response):
        """Parse names and types from DIR_LIST response"""
        entries = []

        if not response:
            return entries

        json_objects = response.split('}')

        for json_str in json_objects:
            if not json_str.strip():
                continue

            json_str = json_str.strip() + '}'

            try:
                start = json_str.find('"param2"')
                start = json_str.find('"', start + 8) + 1
                end = json_str.find('"', start)

                val = json_str[start:end]

                if val and ':' in val:
                    typ, name = val.split(':', 1)

                    if typ == 'F':
                        type_label = "FILE"
                    elif typ == 'D':
                        type_label = "DIR"
                    else:
                        type_label = "UNKNOWN"
                    if type_label != "UNKNOWN":
                        entries.append((name, type_label))

            except Exception as e:
                print(f"Parse error: {e}")

        return entries

    def refresh_files(self):
        """Refresh file list (DEBUG: show raw response and populate table with type)"""
        response = self.connection.send_command(f"DIR_LIST {self.current_path}")

        # Show raw response in label
        path_label = self.query_one("#current_path", Label)
        path_label.update(f"📁 Current: {self.current_path}\n\nRAW RESPONSE:\n{response}")

        entries = []  # list of (name, type)

        if response:
            json_objects = response.split('}')

            for json_str in json_objects:
                if not json_str.strip():
                    continue

                json_str = json_str.strip() + '}'

                try:
                    start = json_str.find('"param2"')
                    start = json_str.find('"', start + 8) + 1
                    end = json_str.find('"', start)

                    val = json_str[start:end]     # e.g. F:f1.txt or D:Songs

                    if val and ':' in val:
                        typ, name = val.split(':', 1)

                        if typ == 'F':
                            type_label = "FILE"
                        elif typ == 'D':
                            type_label = "DIR"
                        else:
                            type_label = "UNKNOWN"
                        if type_label != "UNKNOWN":
                            entries.append((name, type_label))

                except Exception as e:
                    print(f"Parse error: {e}")

        # Populate table
        table = self.query_one("#file_table", DataTable)
        table.clear()

        if not entries:
            table.add_row("(empty)", "-", "-", "-")
        else:
            for name, type_label in entries:
                table.add_row(name, type_label, "-", "-")


    '''
    def refresh_files(self):
        """Refresh file list (DEBUG: show raw response and populate table)"""
        response = self.connection.send_command(f"DIR_LIST {self.current_path}")

        # Show raw response in the label
        path_label = self.query_one("#current_path", Label)
        path_label.update(f"📁 Current: {self.current_path}\n\nRAW RESPONSE:\n{response}")

        # Parse entries (just take raw param2 values, no type distinction yet)
        entries = []
        if response:
            json_objects = response.split('}')
        for json_str in json_objects:
            if not json_str.strip():
                continue
            json_str = json_str.strip() + '}'
            try:
                start = json_str.find('"param2"')
                start = json_str.find('"', start + 8) + 1
                end = json_str.find('"', start)
                val = json_str[start:end]  # e.g., "F:f1.txt" or "D:mydir"
                if val and ':' in val:
                    typ, name = val.split(':', 1)  # split only on first colon
                    entries.append(name)  # use only the actual name

            except Exception as e:
                print(f"Parse error: {e}")

        # Populate table
        table = self.query_one("#file_table", DataTable)
        table.clear()
        if not entries:
            table.add_row("(empty)", "-", "-", "-")
        else:
            for name in entries:
                table.add_row(name, "-", "-", "-")

    def parse_file_list(self, response):
        """Return list of names without distinguishing type (DEBUG)"""
        entries = []
        if not response:
            return entries

        # Split by closing brace '}' — each is one JSON object
        json_objects = response.split('}')
        for json_str in json_objects:
            if not json_str.strip():
                continue
            json_str = json_str.strip() + '}'
            try:
                # find "param2"
                start = json_str.find('"param2"')
                start = json_str.find('"', start + 8) + 1
                end = json_str.find('"', start)
                val = json_str[start:end]
                if val:
                    entries.append(val)  # just take raw string
            except Exception as e:
                print(f"Parse error: {e}")
        return entries
    '''


    def on_button_pressed(self, event: Button.Pressed) -> None:
        btn_id = event.button.id
        
        if btn_id == "refresh":
            self.refresh_files()
        
        elif btn_id == "create_file":
            def callback(values):
                print(f"Create File Callback - Received values: {values}")
                path, content = values
                print(f"Path: '{path}', Content length: {len(content)} chars")
                
                if not path:
                    self.app.notify("❌ Please enter a filename!", severity="error")
                    return
                
                # Build full path
                if not path.startswith("/"):
                    full_path = f"{self.current_path}/{path}".replace("//", "/")
                else:
                    full_path = path
                
                print(f"Full path: {full_path}")
                print(f"Content preview: {content[:100]}")
                print(f"Sending CREATE command...")
                
                try:
                    result = self.connection.send_with_content(f"CREATE {full_path}", content)
                    print(f"Result: {result}")
                    
                    if "SUCCESS" in result:
                        self.app.notify(f"✓ File created: {full_path}", severity="information")
                        self.refresh_files()
                    else:
                        self.app.notify(f"✗ Failed: {result}", severity="error")
                except Exception as e:
                    print(f"Exception: {e}")
                    self.app.notify(f"✗ Error: {str(e)}", severity="error")
            
            # Note: 4th parameter True means multiline TextArea
            self.app.push_screen(InputDialog(
                "Create New File",
                [("Filename:", "e.g., myfile.txt", False, False),
                 ("Content (multi-line allowed):", "Type your content here...", False, True)],
                callback
            ))
        
        elif btn_id == "create_dir":
            def callback(values):
                dirname = values[0]
                if dirname:
                    full_path = f"{self.current_path}/{dirname}".replace("//", "/")
                    result = self.connection.send_command(f"DIR_CREATE {full_path}")
                    self.app.notify("✓ Directory created!" if "SUCCESS" in result else "✗ Failed to create directory")
                    self.refresh_files()
            
            self.app.push_screen(InputDialog(
                "Create New Directory",
                [("Directory name:", "e.g., myfolder", False)],
                callback
            ))
        
        elif btn_id == "edit_file":
            if not self.selected_file:
                self.app.notify("Please select a file first", severity="warning")
                return
            
            def callback(values):
                index, content = values
                if content:
                    full_path = f"{self.current_path}/{self.selected_file}".replace("//", "/")
                    result = self.connection.send_with_content(f"EDIT {full_path} {index}", content)
                    self.app.notify("✓ File edited!" if "SUCCESS" in result else "✗ Failed to edit file")
                    self.refresh_files()
            
            self.app.push_screen(InputDialog(
                f"Edit File: {self.selected_file}",
                [("Index (0 for append):", "0", False, False),
                 ("New content (multi-line):", "Content here", False, True)],
                callback
            ))
        
        elif btn_id == "read_file":
            if not self.selected_file:
                self.app.notify("Please select a file first", severity="warning")
                return
            
            full_path = f"{self.current_path}/{self.selected_file}".replace("//", "/")
            result = self.connection.send_command(f"READ {full_path}")
            
            # Show result in a modal
            class ReadDialog(ModalScreen):
                def compose(self):
                    yield Container(
                        Label(f"Content of {full_path}"),
                        ScrollableContainer(
                            Static(result, id="file_content"),
                            id="content_scroll"
                        ),
                        Button("Close", id="close_btn"),
                        id="read_dialog"
                    )
                
                def on_button_pressed(self, event):
                    self.app.pop_screen()
            
            self.app.push_screen(ReadDialog())
        
        elif btn_id == "rename_file":
            if not self.selected_file:
                self.app.notify("Please select a file first", severity="warning")
                return
            
            def callback(values):
                new_name = values[0]
                if new_name:
                    old_path = f"{self.current_path}/{self.selected_file}".replace("//", "/")
                    new_path = f"{self.current_path}/{new_name}".replace("//", "/")
                    result = self.connection.send_command(f"RENAME_FILE {old_path} {new_path}")
                    self.app.notify("✓ File renamed!" if "SUCCESS" in result else "✗ Failed to rename")
                    self.refresh_files()
            
            self.app.push_screen(InputDialog(
                f"Rename: {self.selected_file}",
                [("New name:", self.selected_file, False)],
                callback
            ))
        
        elif btn_id == "truncate_file":
            if not self.selected_file:
                self.app.notify("Please select a file first", severity="warning")
                return
            
            def confirm_callback(confirmed):
                if confirmed:
                    full_path = f"{self.current_path}/{self.selected_file}".replace("//", "/")
                    result = self.connection.send_command(f"TRUNCATE {full_path}")
                    self.app.notify("✓ File truncated!" if "SUCCESS" in result else "✗ Failed to truncate")
                    self.refresh_files()
            
            self.app.push_screen(ConfirmDialog(
                f"Truncate {self.selected_file}? (File will be emptied)",
                confirm_callback
            ))
        
        elif btn_id in ("delete_file", "delete_dir"):
            if not self.selected_file:
                self.app.notify("Please select a target first", severity="warning")
                return

            target_path = f"{self.current_path}/{self.selected_file}".replace("//", "/")
            # determine required op
            if btn_id == "delete_file":
                if self.selected_type != "FILE":
                    self.app.notify("Selected entry is not a file", severity="error")
                    return
                prompt = f"Delete file {self.selected_file}?"
                cmd = f"DELETE_FILE {target_path}"
            else:
                if self.selected_type != "DIR":
                    self.app.notify("Selected entry is not a directory", severity="error")
                    return
                prompt = f"Delete directory {self.selected_file}? (must be empty)"
                cmd = f"DELETE_DIR {target_path}"

            def confirm_callback(confirmed):
                if confirmed:
                    result = self.connection.send_command(cmd)
                    if "SUCCESS" in result:
                        self.app.notify("✓ Deleted", severity="information")
                    else:
                        self.app.notify(f"✗ Failed: {result}", severity="error")
                    self.refresh_files()

            self.app.push_screen(ConfirmDialog(prompt, confirm_callback))

        elif btn_id == "file_exists":
            def callback(values):
                path = values[0]
                if path:
                    result = self.connection.send_command(f"FILE_EXISTS {path}")
                    exists = "SUCCESS" in result
                    self.app.notify(f"{'✓' if exists else '✗'} File {path} {'exists' if exists else 'does not exist'}")
            
            self.app.push_screen(InputDialog(
                "Check File Exists",
                [("File path:", "/path/to/file", False)],
                callback
            ))
        
        elif btn_id == "dir_exists":
            def callback(values):
                path = values[0]
                if path:
                    result = self.connection.send_command(f"DIR_EXISTS {path}")
                    exists = "SUCCESS" in result
                    self.app.notify(f"{'✓' if exists else '✗'} Directory {path} {'exists' if exists else 'does not exist'}")
            
            self.app.push_screen(InputDialog(
                "Check Directory Exists",
                [("Directory path:", "/path/to/dir", False)],
                callback
            ))
        
        elif btn_id == "get_metadata":
            if not self.selected_file:
                self.app.notify("Please select a file first", severity="warning")
                return
            
            full_path = f"{self.current_path}/{self.selected_file}".replace("//", "/")
            result = self.connection.send_command(f"GET_METADATA {full_path}")
            self.app.notify(f"Metadata: {result}")
        elif btn_id == "set_permissions":
            if not self.selected_file:
                self.app.notify("Please select a file or directory first", severity="warning")
                return

            def callback(values):
                perms = values[0]
                if not perms:
                    self.app.notify("Permissions required", severity="error")
                    return
                full_path = f"{self.current_path}/{self.selected_file}".replace("//", "/")
                result = self.connection.send_command(f"SET_PERMISSIONS {full_path} {perms}")
                self.app.notify("✓ Permissions set!" if "SUCCESS" in result else f"✗ Failed: {result}")

            self.app.push_screen(InputDialog(
                f"Set Permissions: {self.selected_file}",
                [("Permissions (numeric, e.g., 0755):", "0755", False)],
                callback
            ))

        elif btn_id == "set_owner":
            if not self.selected_file:
                self.app.notify("Please select a file or directory first", severity="warning")
                return

            def callback(values):
                owner = values[0]
                if not owner:
                    self.app.notify("Owner username required", severity="error")
                    return
                full_path = f"{self.current_path}/{self.selected_file}".replace("//", "/")
                result = self.connection.send_command(f"SET_OWNER {full_path} {owner}")
                self.app.notify("✓ Owner set!" if "SUCCESS" in result else f"✗ Failed: {result}")

            self.app.push_screen(InputDialog(
                f"Set Owner: {self.selected_file}",
                [("New owner username:", "", False)],
                callback
            ))
        elif btn_id == "enter_dir":
            if not self.selected_file:
                self.app.notify("Please select a target first", severity="warning")
                return

            if self.selected_type != "DIR":
                self.app.notify("❌ Selected item is NOT a directory", severity="warning")
                return
            dirname = self.selected_file

            # Build new path
            if self.current_path == "/":
                self.current_path = f"/{dirname}"
            else:
                self.current_path = f"{self.current_path}/{dirname}"

            self.refresh_files()


        elif btn_id == "go_up":
            if self.current_path == "/":
                return  # already at root

            parts = self.current_path.strip("/").split("/")
            parts.pop()

            if not parts:
                self.current_path = "/"
            else:
                self.current_path = "/" + "/".join(parts)

            self.refresh_files()






class UserPanel(Static):
    """User management panel"""
    
    def __init__(self, connection):
        super().__init__()
        self.connection = connection
    
    def compose(self) -> ComposeResult:
        yield Label("👥 User Management", classes="panel_title")
        yield DataTable(id="user_table", cursor_type="row")
        yield Horizontal(
            Button("➕ Create User", id="create_user", variant="success"),
            Button("🗑️ Delete User", id="delete_user", variant="error"),
            Button("ℹ️ Session Info", id="session_info"),
            Button("🔄 Refresh", id="refresh_users"),
            classes="button_row"
        )
        yield Static("", id="user_message")
    
    def on_mount(self):
        table = self.query_one("#user_table", DataTable)
        table.add_columns("Username", "Role", "Status")
        self.refresh_users()
    
    def refresh_users(self):
        response = self.connection.send_command("LIST_USERS")
        table = self.query_one("#user_table", DataTable)
        table.clear()
        
        json_objects = response.split('}')
        for json_str in json_objects:
            if not json_str.strip():
                continue
            json_str = json_str.strip() + '}'
            
            if '"param2"' in json_str and '"user"' in json_str:
                try:
                    start = json_str.find('"param2"')
                    start = json_str.find('"', start + 10) + 1
                    end = json_str.find('"', start)
                    username = json_str[start:end]
                    if username:
                        table.add_row(username, "USER", "Active")
                except:
                    pass
    
    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "refresh_users":
            self.refresh_users()
        
        elif event.button.id == "create_user":
            def callback(values):
                username, password, role = values
                print(f"Creating user: username={username}, role={role}")
                
                if not username or not password:
                    self.app.notify("❌ Username and password required!", severity="error")
                    return
                
                # Validate role
                if role not in ["0", "1"]:
                    self.app.notify("❌ Role must be 0 (Admin) or 1 (User)", severity="error")
                    return
                
                result = self.connection.send_command(f"CREATE_USER {username} {password} {role}")
                print(f"Create user result: {result}")
                
                if "SUCCESS" in result:
                    self.app.notify(f"✓ User '{username}' created successfully!", severity="information")
                    self.refresh_users()
                else:
                    self.app.notify(f"✗ Failed to create user: {result}", severity="error")
            
            self.app.push_screen(InputDialog(
                "Create New User",
                [("Username:", "", False, False),
                 ("Password:", "", True, False),
                 ("Role (0=Admin, 1=User):", "1", False, False)],
                callback
            ))
        
        elif event.button.id == "delete_user":
            def callback(values):
                username = values[0]
                if username:
                    result = self.connection.send_command(f"DELETE_USER {username}")
                    self.app.notify("✓ User deleted!" if "SUCCESS" in result else "✗ Failed to delete user")
                    self.refresh_users()
            
            self.app.push_screen(InputDialog(
                "Delete User",
                [("Username:", "", False)],
                callback
            ))
        
        elif event.button.id == "session_info":
            result = self.connection.send_command("GET_SESSION_INFO")
            msg = self.query_one("#user_message", Static)
            msg.update(f"[cyan]Session Info:[/cyan]\n{result}")


class StatsPanel(Static):
    """Statistics dashboard"""
    
    def __init__(self, connection):
        super().__init__()
        self.connection = connection
    
    def compose(self) -> ComposeResult:
        yield Label("📊 File System Statistics", classes="panel_title")
        yield ScrollableContainer(
            Static(id="stats_content"),
            id="stats_scroll"
        )
        yield Button("🔄 Refresh Stats", id="refresh_stats")
    
    def on_mount(self):
        self.refresh_stats()
    
    def refresh_stats(self):
        response = self.connection.send_command("GET_STATS")
        stats_widget = self.query_one("#stats_content", Static)
        stats_widget.update(f"""[yellow]OFS File System Statistics[/yellow]

[cyan]Server Response:[/cyan]
{response}

[dim]Press 🔄 Refresh Stats to update[/dim]
        """)
    
    def on_button_pressed(self, event: Button.Pressed) -> None:
        if event.button.id == "refresh_stats":
            self.refresh_stats()


class MainScreen(Screen):
    """Main application screen"""
    
    BINDINGS = [
        ("f1", "switch_tab('files')", "Files"),
        ("f2", "switch_tab('users')", "Users"),
        ("f3", "switch_tab('stats')", "Stats"),
        ("ctrl+l", "logout", "Logout"),
        ("ctrl+q", "quit_app", "Quit"),
    ]
    
    def __init__(self, connection):
        super().__init__()
        self.connection = connection
    
    def compose(self) -> ComposeResult:
        yield Header()
        
        with TabbedContent(initial="files"):
            with TabPane("📁 Files (F1)", id="files"):
                yield FileBrowserPanel(self.connection)
            
            with TabPane("👥 Users (F2)", id="users"):
                yield UserPanel(self.connection)
            
            with TabPane("📊 Stats (F3)", id="stats"):
                yield StatsPanel(self.connection)
        
        yield Footer()
    
    def action_switch_tab(self, tab: str) -> None:
        self.query_one(TabbedContent).active = tab
    
    def action_logout(self) -> None:
        """Logout and return to login screen"""
        self.connection.logout()
        self.app.pop_screen()
        self.app.push_screen(LoginScreen())
    
    def action_quit_app(self) -> None:
        """Quit the application and send EXIT to server"""
        self.connection.send_command("EXIT")
        self.app.exit()


class OFSApp(App):
    """Main OFS Client Application"""
    COLORS = {
    "surface": "#FFC5D3",
    "panel": "rgba(255, 197, 211, 0.5)", # Translucent pink
    # You may need to define primary, accent, text here if needed
    }
    CSS = """

    Screen {
        background: $surface;
    }
    
    #login_container {
        width: 60;
        height: auto;
        background: #FFC5D3;
        border: heavy #B666D2;
        padding: 2 4;
        margin: 1 2;
        align: center middle;
    }
    
    .logo {
        text-align: center;
        color: #B666D2;
    }
    
    .label {
        color: $text;
        margin: 1 0;
    }
    
    Input {
        margin: 0 0 1 0;
    }
    
    TextArea {
        height: 10;
        margin: 0 0 1 0;
        border: solid $primary;
    }
    
    .button_row {
        height: auto;
        margin: 1 0;
    }
    
    Button {
        margin: 0 1;
    }
    
    #message, #user_message {
        text-align: center;
        margin: 1 0;
    }
    
    DataTable {
        height: 20;
        margin: 1 0;
    }
    
    .panel_title {
        text-style: bold;
        color: $accent;
        margin: 1 0;
    }
    
    .dialog_title {
        text-style: bold;
        text-align: center;
        margin: 1 0;
    }
    
    #confirm_dialog, #input_dialog {
        width: 80;
        height: auto;
        background: #FFC5D3;
        border: heavy $primary;
        padding: 2 4;
        align: center middle;
    }
    
    #confirm_message {
        text-align: center;
        margin: 2 0;
    }
    
    #stats_scroll, #content_scroll {
        height: 30;
        border: solid $primary;
        margin: 1 0;
    }
    
    #stats_content, #file_content {
        padding: 1 2;
    }
    
    #read_dialog {
        width: 80;
        height: 40;
        background: #FFC5D3;
        border: heavy $primary;
        padding: 2 4;
        align: center middle;
    }
    """
    
    TITLE = "OFS File System Client"
    SUB_TITLE = "Complete Feature Set"
    
    def __init__(self):
        super().__init__()
        self.connection = OFSConnection()
    
    def on_mount(self) -> None:
        print("App starting...")
        if not self.connection.connect():
            self.exit(message="Failed to connect to OFS server!")
            return
        print("Connected!")
        self.push_screen(LoginScreen())
    
    def switch_to_main_screen(self):
        self.pop_screen()
        self.push_screen(MainScreen(self.connection))


def main():
    app = OFSApp()
    app.run()


if __name__ == "__main__":
    main()