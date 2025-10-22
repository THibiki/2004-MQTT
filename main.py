import network
import socket
import time
import os
import struct

# WiFi Configuration
SSID = 'your_ssid' ####### CHANGE THIS
PASSWORD = 'your_pw' ####### CHANGE THIS

# PC Configuration
PC_IP = 'your_ip' ####### CHANGE THIS
PC_PORT = 5005

# Connect Pico to Wifi
def connect_wifi():
    wlan = network.WLAN(network.STA_IF)
    wlan.active(True)
    wlan.connect(SSID, PASSWORD)
    
    print('Connecting to WiFi...')
    max_wait = 10
    while max_wait > 0:
        status = wlan.status()
        print("Status:", status)
        if status < 0 or status >= 3:
            break
        max_wait -= 1
        print('Waiting for connection...')
        time.sleep(1)
    
    if wlan.status() != 3:
        raise RuntimeError('Network connection failed')
    else:
        print('Connected!')
        status = wlan.ifconfig()
        print(f'Pico IP: {status[0]}')
        return wlan

# Save received file to Pico's flash storage (Pico has ~1MB flash for file storage)
def save_file(filename, content):
    try:
        with open(filename, 'wb') as f: # binary write mode
            f.write(content)
        print(f'File saved: {filename}')
        return True
    except Exception as e:
        print(f'Error saving file: {e}')
        return False

# Read and return file content from Pico's filesystem
def read_file(filename):
    try:
        with open(filename, 'rb') as f:
            content = f.read()
        return content
    except Exception as e:
        print(f'Error reading file: {e}')
        return None

# List all files in current directory
def list_files():
    try:
        files = os.listdir()
        return files
    except Exception as e:
        print(f'Error listing files: {e}')
        return []

# Read and display file content
def print_file_content(filename):
    print(f'\n{"="*56}')
    print(f'Reading file: {filename}')
    print(f'{"="*56}')
    
    content = read_file(filename)
    if content:
        print(f'File size: {len(content)} bytes')
        
        try:
            text_content = content.decode('utf-8') # Display as text
            print(text_content)
        except:
            print('[Cannot display as text]')
            print(f'First 100 bytes (hex): {content[:100].hex()}') # Hex dump
        
        print(f'{"="*56}')
        print(f'End of file: {filename}')
        print(f'{"="*56}\n')
        return True
    else:
        print(f'Could not read file: {filename}\n')
        return False

# Main communication handler
def communicate_with_pc(message):
    # Create UDP socket
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM) # udp socket
    sock.settimeout(35.0) # 35 seconds receive timeout (additional 5s buffer for network delay)
    print(f'-' * 56)
    
    try:
        # Send message to PC
        print(f'Sending to PC: {message}')
        sock.sendto(message.encode(), (PC_IP, PC_PORT))
        
        # Wait for response
        print('Waiting for response from PC...')
        data, addr = sock.recvfrom(65535)  # Increased buffer for larger files
        
        # Check if response is a file transfer
        if data.startswith(b'FILE:'):
            # Parse file transfer: FILE: + 4-byte filename length + filename + content
            try:
                # Extract filename length (4 bytes after "FILE:")
                if len(data) < 9: # packet is too short
                    print('Invalid file format: packet too short')
                    sock.sendto(b'FILE_FORMAT_ERROR', addr)
                    return None
                
                filename_len = struct.unpack('I', data[5:9])[0]
                
                if filename_len > 255 or filename_len == 0: # check for valid filename (length)
                    print(f'Invalid filename length: {filename_len}')
                    sock.sendto(b'FILE_FORMAT_ERROR', addr)
                    return None
                
                # Extract filename
                filename_start = 9
                filename_end = filename_start + filename_len
                
                if len(data) < filename_end: # check for incomplete filename
                    print('Invalid file format: filename incomplete')
                    sock.sendto(b'FILE_FORMAT_ERROR', addr)
                    return None
                
                filename = data[filename_start:filename_end].decode('utf-8')
                
                # Extract content (everything after filename)
                content = data[filename_end:]
                
                print(f'\nReceiving file: {filename}')
                print(f'File size: {len(content)} bytes')
                
                if save_file(filename, content):
                    # Send confirmation
                    sock.sendto(b'FILE_RECEIVED_OK', addr)
                    
                    # Automatically print the received file content
                    print_file_content(filename)
                    
                    return f'FILE:{filename}'
                else:
                    sock.sendto(b'FILE_SAVE_ERROR', addr)
                    return 'FILE_ERROR'
                    
            except struct.error as e:
                print(f'Error unpacking file header: {e}')
                sock.sendto(b'FILE_FORMAT_ERROR', addr)
                return None
            except Exception as e:
                print(f'Error processing file: {e}')
                sock.sendto(b'FILE_PROCESSING_ERROR', addr)
                return None
        else:
            # Regular text message
            try:
                response = data.decode('utf-8')
                print(f'Received from PC: {response}')
                return response
            except:
                print(f'Received binary data ({len(data)} bytes)')
                return '[BINARY_DATA]'
        
    except OSError as e:
        print(f'Timeout or error: {e}')
        return None
    finally:
        sock.close()

# Main execution
try:
    wlan = connect_wifi()
    time.sleep(2)
    
    print('\n============= Pico W Communication Started =============')
    print('Pico will send messages and wait for PC responses')
    print('PC can send text messages or files')
    print()
    
    # List existing files on Pico
    print('Files currently on Pico:')
    files = list_files()
    if files:
        for f in files:
            try:
                size = os.stat(f)[6]  # Get file size
                print(f'  - {f} ({size} bytes)')
            except:
                print(f'  - {f}')
    else:
        print('  (no files)')
    print()
    
    # Infinite communication loop
    message_count = 0
    while True:
        message_count += 1
        message = f"Hello from Pico! (Message #{message_count})"
        response = communicate_with_pc(message)
        
        if response:
            if response.startswith('FILE:'):
                print(f'File received, saved, and printed successfully!')
            else:
                print(f'Communication successful!')
            print(f'-' * 56)
        else:
            print('No response received within timeout')
            print(f'-' * 56)
        
        print('\nWaiting 5 seconds before next message...\n')
        time.sleep(5) # wait 5 seconds between messages
    
except KeyboardInterrupt:
    print('\nStopped by user')
except Exception as e:
    print(f'Error: {e}')
    import sys
    sys.print_exception(e)
    