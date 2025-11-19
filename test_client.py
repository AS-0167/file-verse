#!/usr/bin/env python3
"""
OFS Interactive Client
Interactive command-line interface for OFS file system
"""

import socket
import json
import uuid
import sys
import os

class OFSClient:
    def __init__(self, host='localhost', port=8080):
        self.host = host
        self.port = port
        self.session_id = None
        self.current_user = None
    
    def send_request(self, operation, parameters):
        """Send a request to the server and receive response"""
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(5)
            sock.connect((self.host, self.port))
            
            request = {
                "operation": operation,
                "session_id": self.session_id or "",
                "request_id": str(uuid.uuid4()),
                "parameters": parameters
            }
            
            message = json.dumps(request) + "\n"
            sock.sendall(message.encode('utf-8'))
            
            response_data = b""
            while True:
                chunk = sock.recv(8192)
                if not chunk:
                    break
                response_data += chunk
                if b'\n' in chunk:
                    break
            
            sock.close()
            
            try:
                response = json.loads(response_data.decode('utf-8', errors='ignore'))
                return response
            except json.JSONDecodeError:
                return None
            
        except Exception as e:
            print(f"✗ Connection error: {e}")
            return None
    
    def is_logged_in(self):
        return self.session_id is not None
    
    def login(self, username, password):
        response = self.send_request("user_login", {
            "username": username,
            "password": password
        })
        
        if response and response.get("status") == "success":
            self.session_id = response.get("data", {}).get("session_id")
            self.current_user = username
            return True, f"Logged in as {username}"
        else:
            error = response.get("error_message", "Unknown error") if response else "Connection failed"
            return False, error
    
    def logout(self):
        response = self.send_request("user_logout", {})
        if response and response.get("status") == "success":
            self.session_id = None
            self.current_user = None
            return True, "Logged out successfully"
        return False, "Logout failed"
    
    def create_file(self, path, content):
        response = self.send_request("file_create", {"path": path, "data": content})
        if response and response.get("status") == "success":
            return True, f"File '{path}' created"
        error = response.get("error_message", "Unknown error") if response else "Connection failed"
        return False, error
    
    def read_file(self, path):
        response = self.send_request("file_read", {"path": path})
        if response and response.get("status") == "success":
            return True, response.get("data", {})
        error = response.get("error_message", "Unknown error") if response else "Connection failed"
        return False, error
    
    def delete_file(self, path):
        response = self.send_request("file_delete", {"path": path})
        if response and response.get("status") == "success":
            return True, f"File '{path}' deleted"
        error = response.get("error_message", "Unknown error") if response else "Connection failed"
        return False, error
    
    def create_dir(self, path):
        response = self.send_request("dir_create", {"path": path})
        if response and response.get("status") == "success":
            return True, f"Directory '{path}' created"
        error = response.get("error_message", "Unknown error") if response else "Connection failed"
        return False, error
    
    def list_dir(self, path):
        response = self.send_request("dir_list", {"path": path})
        if response and response.get("status") == "success":
            return True, response.get("data", {}).get("files", [])
        error = response.get("error_message", "Unknown error") if response else "Connection failed"
        return False, error
    
    def get_stats(self):
        response = self.send_request("get_stats", {})
        if response and response.get("status") == "success":
            return True, response.get("data", {})
        error = response.get("error_message", "Unknown error") if response else "Connection failed"
        return False, error


class InteractiveCLI:
    def __init__(self):
        self.client = OFSClient()
        self.running = True
        self.commands = {
            'login': self.cmd_login,
            'logout': self.cmd_logout,
            'ls': self.cmd_list,
            'mkdir': self.cmd_mkdir,
            'create': self.cmd_create,
            'cat': self.cmd_cat,
            'rm': self.cmd_rm,
            'stats': self.cmd_stats,
            'help': self.cmd_help,
            'exit': self.cmd_exit,
            'quit': self.cmd_exit,
        }
    
    def print_banner(self):
        print("\n" + "="*60)
        print("        OFS Interactive Client")
        print("        Student: BSCS24115")
        print("="*60)
        print("Type 'help' for available commands")
        print("="*60 + "\n")
    
    def get_prompt(self):
        if self.client.is_logged_in():
            return f"ofs:{self.client.current_user}> "
        return "ofs> "
    
    def cmd_login(self, args):
        """Login to OFS - Usage: login <username> <password>"""
        if len(args) < 2:
            print("Usage: login <username> <password>")
            return
        
        username, password = args[0], args[1]
        success, message = self.client.login(username, password)
        
        if success:
            print(f"✓ {message}")
        else:
            print(f"✗ Login failed: {message}")
    
    def cmd_logout(self, args):
        """Logout from OFS"""
        if not self.client.is_logged_in():
            print("Not logged in")
            return
        
        success, message = self.client.logout()
        print(f"✓ {message}" if success else f"✗ {message}")
    
    def cmd_list(self, args):
        """List directory contents - Usage: ls [path]"""
        if not self.client.is_logged_in():
            print("✗ Not logged in. Use 'login' first.")
            return
        
        path = args[0] if args else "/"
        success, result = self.client.list_dir(path)
        
        if success:
            files = result
            if not files:
                print(f"(empty directory)")
                return
            
            print(f"\nContents of '{path}':")
            print("-" * 60)
            print(f"{'Type':<6} {'Name':<30} {'Size':<12} {'Owner':<10}")
            print("-" * 60)
            
            for f in files:
                type_str = "DIR" if f.get("type") == 1 else "FILE"
                name = f.get("name", "?")
                size = f.get("size", 0)
                owner = f.get("owner", "?")[:10]
                print(f"{type_str:<6} {name:<30} {size:<12} {owner:<10}")
            
            print("-" * 60)
            print(f"Total: {len(files)} items\n")
        else:
            print(f"✗ Error: {result}")
    
    def cmd_mkdir(self, args):
        """Create directory - Usage: mkdir <path>"""
        if not self.client.is_logged_in():
            print("✗ Not logged in. Use 'login' first.")
            return
        
        if not args:
            print("Usage: mkdir <path>")
            return
        
        path = args[0]
        success, message = self.client.create_dir(path)
        print(f"✓ {message}" if success else f"✗ Error: {message}")
    
    def cmd_create(self, args):
        """Create file - Usage: create <path> <content...>"""
        if not self.client.is_logged_in():
            print("✗ Not logged in. Use 'login' first.")
            return
        
        if len(args) < 2:
            print("Usage: create <path> <content...>")
            return
        
        path = args[0]
        content = ' '.join(args[1:])
        
        success, message = self.client.create_file(path, content)
        print(f"✓ {message}" if success else f"✗ Error: {message}")
    
    def cmd_cat(self, args):
        """Read file contents - Usage: cat <path>"""
        if not self.client.is_logged_in():
            print("✗ Not logged in. Use 'login' first.")
            return
        
        if not args:
            print("Usage: cat <path>")
            return
        
        path = args[0]
        success, result = self.client.read_file(path)
        
        if success:
            content = result.get("content", "")
            size = result.get("size", 0)
            print(f"\n--- {path} ({size} bytes) ---")
            print(content)
            print("--- End of file ---\n")
        else:
            print(f"✗ Error: {result}")
    
    def cmd_rm(self, args):
        """Delete file - Usage: rm <path>"""
        if not self.client.is_logged_in():
            print("✗ Not logged in. Use 'login' first.")
            return
        
        if not args:
            print("Usage: rm <path>")
            return
        
        path = args[0]
        success, message = self.client.delete_file(path)
        print(f"✓ {message}" if success else f"✗ Error: {message}")
    
    def cmd_stats(self, args):
        """Show file system statistics"""
        if not self.client.is_logged_in():
            print("✗ Not logged in. Use 'login' first.")
            return
        
        success, result = self.client.get_stats()
        
        if success:
            print("\n" + "="*60)
            print("File System Statistics")
            print("="*60)
            print(f"Total Size:        {result.get('total_size', 0):>15} bytes")
            print(f"Used Space:        {result.get('used_space', 0):>15} bytes")
            print(f"Free Space:        {result.get('free_space', 0):>15} bytes")
            print(f"Total Files:       {result.get('total_files', 0):>15}")
            print(f"Total Directories: {result.get('total_directories', 0):>15}")
            
            used_pct = (result.get('used_space', 0) / result.get('total_size', 1)) * 100
            print(f"Usage:             {used_pct:>14.2f}%")
            print("="*60 + "\n")
        else:
            print(f"✗ Error: {result}")
    
    def cmd_help(self, args):
        """Show available commands"""
        print("\n" + "="*60)
        print("Available Commands")
        print("="*60)
        
        commands = [
            ("login <user> <pass>", "Login to the file system"),
            ("logout", "Logout from the file system"),
            ("ls [path]", "List directory contents (default: /)"),
            ("mkdir <path>", "Create a new directory"),
            ("create <path> <text>", "Create a file with content"),
            ("cat <path>", "Display file contents"),
            ("rm <path>", "Delete a file"),
            ("stats", "Show file system statistics"),
            ("help", "Show this help message"),
            ("exit/quit", "Exit the program"),
        ]
        
        for cmd, desc in commands:
            print(f"  {cmd:<25} - {desc}")
        
        print("="*60)
        print("\nExamples:")
        print("  login admin admin123")
        print("  mkdir /documents")
        print("  create /test.txt Hello World")
        print("  cat /test.txt")
        print("  ls /")
        print("="*60 + "\n")
    
    def cmd_exit(self, args):
        """Exit the program"""
        if self.client.is_logged_in():
            print("Logging out...")
            self.client.logout()
        
        print("Goodbye!")
        self.running = False
    
    def run(self):
        self.print_banner()
        
        while self.running:
            try:
                prompt = self.get_prompt()
                user_input = input(prompt).strip()
                
                if not user_input:
                    continue
                
                parts = user_input.split()
                command = parts[0].lower()
                args = parts[1:]
                
                if command in self.commands:
                    self.commands[command](args)
                else:
                    print(f"Unknown command: {command}. Type 'help' for available commands.")
                
            except KeyboardInterrupt:
                print("\n\nUse 'exit' or 'quit' to leave")
            except EOFError:
                print("\nGoodbye!")
                break
            except Exception as e:
                print(f"Error: {e}")


def main():
    cli = InteractiveCLI()
    cli.run()


if __name__ == "__main__":
    main()