#!/usr/bin/env python3
"""
WiFi Client for ESP32-S7 Bootloader
This script provides a command-line interface to interact with the ESP32 via WiFi
"""

import requests
import json
import sys
import os
import argparse

class ESP32WiFiClient:
    def __init__(self, ip_address="192.168.4.1", port=80):
        self.base_url = f"http://{ip_address}:{port}"
        self.session = requests.Session()
    
    def get_status(self):
        """Get system status from ESP32"""
        try:
            response = self.session.get(f"{self.base_url}/api/status")
            if response.status_code == 200:
                return response.json()
            else:
                return {"error": f"HTTP {response.status_code}"}
        except Exception as e:
            return {"error": str(e)}
    
    def execute_command(self, command):
        """Execute a command on the ESP32"""
        try:
            data = {"command": command}
            response = self.session.post(
                f"{self.base_url}/api/command",
                json=data,
                headers={"Content-Type": "application/json"}
            )
            if response.status_code == 200:
                return response.json()
            else:
                return {"error": f"HTTP {response.status_code}"}
        except Exception as e:
            return {"error": str(e)}
    
    def list_files(self):
        """List files in SPIFFS"""
        try:
            response = self.session.get(f"{self.base_url}/api/files")
            if response.status_code == 200:
                return response.json()
            else:
                return {"error": f"HTTP {response.status_code}"}
        except Exception as e:
            return {"error": str(e)}
    
    def upload_file(self, file_path, remote_name=None):
        """Upload a file to SPIFFS"""
        try:
            if not os.path.exists(file_path):
                return {"error": f"File {file_path} not found"}
            
            if remote_name is None:
                remote_name = os.path.basename(file_path)
            
            with open(file_path, 'rb') as f:
                files = {'file': (remote_name, f, 'text/plain')}
                response = self.session.post(f"{self.base_url}/upload", files=files)
            
            if response.status_code == 200:
                return {"success": response.text}
            else:
                return {"error": f"HTTP {response.status_code}: {response.text}"}
        except Exception as e:
            return {"error": str(e)}
    
    def view_file(self, filename):
        """View file contents"""
        try:
            response = self.session.get(f"{self.base_url}/api/file/{filename}")
            if response.status_code == 200:
                return {"content": response.text}
            else:
                return {"error": f"HTTP {response.status_code}"}
        except Exception as e:
            return {"error": str(e)}
    
    def delete_file(self, filename):
        """Delete a file from SPIFFS"""
        try:
            response = self.session.delete(f"{self.base_url}/api/delete/{filename}")
            if response.status_code == 200:
                return {"success": response.text}
            else:
                return {"error": f"HTTP {response.status_code}"}
        except Exception as e:
            return {"error": str(e)}

def main():
    parser = argparse.ArgumentParser(description="ESP32-S7 Bootloader WiFi Client")
    parser.add_argument("--ip", default="192.168.4.1", help="ESP32 IP address")
    parser.add_argument("--port", type=int, default=80, help="ESP32 port")
    
    subparsers = parser.add_subparsers(dest="command", help="Available commands")
    
    # Status command
    subparsers.add_parser("status", help="Get system status")
    
    # Command execution
    cmd_parser = subparsers.add_parser("cmd", help="Execute a command")
    cmd_parser.add_argument("command", help="Command to execute")
    
    # File operations
    subparsers.add_parser("list", help="List files in SPIFFS")
    
    upload_parser = subparsers.add_parser("upload", help="Upload a file")
    upload_parser.add_argument("file", help="Local file path")
    upload_parser.add_argument("--name", help="Remote filename (default: same as local)")
    
    view_parser = subparsers.add_parser("view", help="View file contents")
    view_parser.add_argument("filename", help="Filename to view")
    
    delete_parser = subparsers.add_parser("delete", help="Delete a file")
    delete_parser.add_argument("filename", help="Filename to delete")
    
    args = parser.parse_args()
    
    if not args.command:
        parser.print_help()
        return
    
    client = ESP32WiFiClient(args.ip, args.port)
    
    if args.command == "status":
        result = client.get_status()
        print(json.dumps(result, indent=2))
    
    elif args.command == "cmd":
        result = client.execute_command(args.command)
        print(json.dumps(result, indent=2))
    
    elif args.command == "list":
        result = client.list_files()
        if "files" in result:
            print("Files in SPIFFS:")
            for file_info in result["files"]:
                print(f"  • {file_info['name']} ({file_info['size']} bytes)")
        else:
            print(json.dumps(result, indent=2))
    
    elif args.command == "upload":
        result = client.upload_file(args.file, args.name)
        print(json.dumps(result, indent=2))
    
    elif args.command == "view":
        result = client.view_file(args.filename)
        if "content" in result:
            print(result["content"])
        else:
            print(json.dumps(result, indent=2))
    
    elif args.command == "delete":
        result = client.delete_file(args.filename)
        print(json.dumps(result, indent=2))

if __name__ == "__main__":
    main()



