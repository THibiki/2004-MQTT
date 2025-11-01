# How to Install pioasm

`pioasm` is the assembler for Raspberry Pi Pico's Programmable I/O (PIO) programs. It's typically built automatically by the Pico SDK, but you can also install it standalone.

## Current Status

Your project has a `pioasm` binary at:
```
build/pioasm/pioasm.exe
```

**Note:** This binary may have been built for Linux/WSL (not native Windows). If you get an error like "The specified executable is not a valid application for this OS platform", you need to build a Windows version.

## Installation Options

### Option 1: Use the Built Version (No Installation Needed)

The `pioasm.exe` is already built in your project. To use it:

```powershell
# Use it directly
.\build\pioasm\pioasm.exe input.pio output.h

# Or add to PATH for current session
$env:PATH += ";$PWD\build\pioasm"
```

### Option 2: Build Standalone from Pico SDK (Windows Native)

1. **Find your Pico SDK path:**
   - Check if `PICO_SDK_PATH` environment variable is set
   - Common locations: `C:\pico\pico-sdk`, `C:\Users\<user>\pico\pico-sdk`, or check where `pico_sdk_import.cmake` came from

2. **Build pioasm for Windows:**
   ```powershell
   # Navigate to pioasm directory
   cd <PICO_SDK_PATH>\tools\pioasm
   
   # Create build directory
   mkdir build-windows -Force
   cd build-windows
   
   # Configure with CMake (use MinGW or MSVC)
   cmake .. -G "MinGW Makefiles"  # If using MinGW
   # OR
   cmake .. -G "Visual Studio 17 2022"  # If using Visual Studio
   
   # Build
   cmake --build .
   ```

3. **Copy to a system location** (optional):
   ```powershell
   # Copy to a tools directory
   New-Item -ItemType Directory -Force -Path C:\tools\pico-tools
   Copy-Item pioasm.exe C:\tools\pico-tools\pioasm.exe
   
   # Add to PATH permanently (User scope)
   $currentPath = [Environment]::GetEnvironmentVariable("Path", "User")
   if ($currentPath -notlike "*C:\tools\pico-tools*") {
       [Environment]::SetEnvironmentVariable("Path", "$currentPath;C:\tools\pico-tools", "User")
   }
   ```

### Option 3: Install via Package Manager

**For Windows with MSYS2/MinGW:**
```powershell
# If you have MSYS2 installed
pacman -S mingw-w64-x86_64-pioasm
```

**For Linux/macOS:**
```bash
# On Linux (if available via package manager)
# Note: pioasm is typically not in standard repos, SDK build is recommended
```

### Option 4: Download Pre-built Binary

Pre-built binaries are sometimes available, but building from source is recommended for compatibility.

## Usage

Once installed/available, use pioasm like this:

```powershell
# Assemble a .pio file to a C header
pioasm.exe program.pio program.pio.h

# Or to a Python file
pioasm.exe -o python program.pio program.py
```

## Integration with CMake

If your project uses PIO programs, the Pico SDK will automatically:
1. Build pioasm during the CMake configuration phase
2. Use it to assemble any `.pio` files in your project
3. Generate header files automatically

No manual installation needed when using the SDK's build system!

## Verify Installation

Check if pioasm is available:
```powershell
pioasm.exe --version
```

Or check if it exists in your build:
```powershell
Test-Path .\build\pioasm\pioasm.exe
```

