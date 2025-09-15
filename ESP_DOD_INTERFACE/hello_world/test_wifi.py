#!/usr/bin/env python3
"""
Test script for ESP32-S7 Bootloader WiFi functionality
"""

import requests
import json
import time
import sys

def test_wifi_connection(ip="192.168.4.1", port=80):
    """Test basic WiFi connectivity"""
    print(f"Testing connection to {ip}:{port}")
    
    try:
        response = requests.get(f"http://{ip}:{port}/api/status", timeout=5)
        if response.status_code == 200:
            print("✓ WiFi connection successful")
            return True
        else:
            print(f"✗ HTTP error: {response.status_code}")
            return False
    except requests.exceptions.RequestException as e:
        print(f"✗ Connection failed: {e}")
        return False

def test_status_api(ip="192.168.4.1", port=80):
    """Test status API endpoint"""
    print("\nTesting status API...")
    
    try:
        response = requests.get(f"http://{ip}:{port}/api/status", timeout=5)
        if response.status_code == 200:
            data = response.json()
            print("✓ Status API working")
            print(f"  WiFi connected: {data.get('wifi_connected', 'N/A')}")
            print(f"  Free heap: {data.get('free_heap', 'N/A')} bytes")
            print(f"  UART TX: GPIO {data.get('uart_tx', 'N/A')}")
            print(f"  UART RX: GPIO {data.get('uart_rx', 'N/A')}")
            print(f"  UART Baud: {data.get('uart_baud', 'N/A')}")
            return True
        else:
            print(f"✗ Status API failed: {response.status_code}")
            return False
    except Exception as e:
        print(f"✗ Status API error: {e}")
        return False

def test_command_api(ip="192.168.4.1", port=80):
    """Test command API endpoint"""
    print("\nTesting command API...")
    
    test_commands = ["status", "establish_handshake", "get_bl_version"]
    
    for cmd in test_commands:
        try:
            data = {"command": cmd}
            response = requests.post(
                f"http://{ip}:{port}/api/command",
                json=data,
                headers={"Content-Type": "application/json"},
                timeout=10
            )
            if response.status_code == 200:
                result = response.json()
                print(f"✓ Command '{cmd}': {result.get('result', 'No result')[:50]}...")
            else:
                print(f"✗ Command '{cmd}' failed: {response.status_code}")
        except Exception as e:
            print(f"✗ Command '{cmd}' error: {e}")

def test_files_api(ip="192.168.4.1", port=80):
    """Test files API endpoint"""
    print("\nTesting files API...")
    
    try:
        response = requests.get(f"http://{ip}:{port}/api/files", timeout=5)
        if response.status_code == 200:
            data = response.json()
            files = data.get('files', [])
            print(f"✓ Files API working - Found {len(files)} files")
            for file_info in files:
                print(f"  • {file_info['name']} ({file_info['size']} bytes)")
            return True
        else:
            print(f"✗ Files API failed: {response.status_code}")
            return False
    except Exception as e:
        print(f"✗ Files API error: {e}")
        return False

def test_file_upload(ip="192.168.4.1", port=80):
    """Test file upload functionality"""
    print("\nTesting file upload...")
    
    # Create a test file
    test_content = "Hello from WiFi test!\nThis is a test file uploaded via WiFi.\n"
    test_filename = "wifi_test.txt"
    
    try:
        files = {'file': (test_filename, test_content, 'text/plain')}
        response = requests.post(f"http://{ip}:{port}/upload", files=files, timeout=10)
        
        if response.status_code == 200:
            print(f"✓ File upload successful: {response.text}")
            
            # Verify file was uploaded by listing files
            list_response = requests.get(f"http://{ip}:{port}/api/files", timeout=5)
            if list_response.status_code == 200:
                files_data = list_response.json()
                uploaded_files = [f['name'] for f in files_data.get('files', [])]
                if test_filename in uploaded_files:
                    print(f"✓ File '{test_filename}' confirmed in SPIFFS")
                else:
                    print(f"✗ File '{test_filename}' not found in SPIFFS")
            
            return True
        else:
            print(f"✗ File upload failed: {response.status_code} - {response.text}")
            return False
    except Exception as e:
        print(f"✗ File upload error: {e}")
        return False

def test_web_interface(ip="192.168.4.1", port=80):
    """Test web interface accessibility"""
    print("\nTesting web interface...")
    
    try:
        response = requests.get(f"http://{ip}:{port}/", timeout=5)
        if response.status_code == 200:
            if "ESP32-S7 Bootloader Control" in response.text:
                print("✓ Web interface accessible and contains expected content")
                return True
            else:
                print("✗ Web interface accessible but content unexpected")
                return False
        else:
            print(f"✗ Web interface failed: {response.status_code}")
            return False
    except Exception as e:
        print(f"✗ Web interface error: {e}")
        return False

def main():
    print("ESP32-S7 Bootloader WiFi Test Suite")
    print("=" * 40)
    
    # Parse command line arguments
    ip = "192.168.4.1"
    port = 80
    
    if len(sys.argv) > 1:
        ip = sys.argv[1]
    if len(sys.argv) > 2:
        port = int(sys.argv[2])
    
    print(f"Testing ESP32 at {ip}:{port}")
    print("Make sure you're connected to the 'ESP32-S7-Bootloader' WiFi network")
    print()
    
    # Run tests
    tests = [
        test_wifi_connection,
        test_status_api,
        test_command_api,
        test_files_api,
        test_file_upload,
        test_web_interface
    ]
    
    passed = 0
    total = len(tests)
    
    for test in tests:
        try:
            if test(ip, port):
                passed += 1
        except Exception as e:
            print(f"✗ Test failed with exception: {e}")
    
    print("\n" + "=" * 40)
    print(f"Test Results: {passed}/{total} tests passed")
    
    if passed == total:
        print("🎉 All tests passed! WiFi functionality is working correctly.")
        return 0
    else:
        print("❌ Some tests failed. Check the ESP32 logs and network connection.")
        return 1

if __name__ == "__main__":
    sys.exit(main())



