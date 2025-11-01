#!/usr/bin/env pwsh
#Requires -RunAsAdministrator

Write-Host "=== MQTT-SN Gateway Setup Script (Windows) ===" -ForegroundColor Cyan
Write-Host ""

# Check if running as administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $isAdmin) {
    Write-Host "❌ This script requires Administrator privileges!" -ForegroundColor Red
    Write-Host "Please run PowerShell as Administrator and try again." -ForegroundColor Yellow
    exit 1
}

Write-Host "Current system info:" -ForegroundColor Green
Get-NetIPAddress -AddressFamily IPv4 | Where-Object { $_.IPAddress -like "192.168.*" -or $_.IPAddress -like "172.*" -or $_.IPAddress -like "10.*" } | Select-Object -ExpandProperty IPAddress
Write-Host ""

# Step 1: Install MQTT Broker (Mosquitto)
Write-Host "Step 1: Installing MQTT broker (Mosquitto)..." -ForegroundColor Yellow
$mosquittoInstalled = $false

# Check if mosquitto is already installed
if (Get-Command mosquitto -ErrorAction SilentlyContinue) {
    Write-Host "✓ Mosquitto is already installed" -ForegroundColor Green
    $mosquittoInstalled = $true
} else {
    Write-Host "Mosquitto not found. Attempting to install..." -ForegroundColor Yellow
    
    # Try winget first (Windows 10/11)
    if (Get-Command winget -ErrorAction SilentlyContinue) {
        Write-Host "Installing via winget..." -ForegroundColor Cyan
        try {
            winget install Eclipse.Mosquitto --accept-source-agreements --accept-package-agreements
            $mosquittoInstalled = $true
            Write-Host "✓ Mosquitto installed via winget" -ForegroundColor Green
        } catch {
            Write-Host "⚠ winget installation failed, trying chocolatey..." -ForegroundColor Yellow
        }
    }
    
    # Try Chocolatey
    if (-not $mosquittoInstalled -and (Get-Command choco -ErrorAction SilentlyContinue)) {
        Write-Host "Installing via Chocolatey..." -ForegroundColor Cyan
        try {
            choco install mosquitto -y
            $mosquittoInstalled = $true
            Write-Host "✓ Mosquitto installed via Chocolatey" -ForegroundColor Green
        } catch {
            Write-Host "⚠ Chocolatey installation failed" -ForegroundColor Yellow
        }
    }
    
    # Manual installation instructions
    if (-not $mosquittoInstalled) {
        Write-Host "❌ Automatic installation failed!" -ForegroundColor Red
        Write-Host "Please install Mosquitto manually:" -ForegroundColor Yellow
        Write-Host "  1. Download from: https://mosquitto.org/download/" -ForegroundColor Cyan
        Write-Host "  2. Or install Chocolatey first: https://chocolatey.org/install" -ForegroundColor Cyan
        Write-Host "  3. Then run: choco install mosquitto" -ForegroundColor Cyan
        Write-Host ""
        $continue = Read-Host "Continue anyway? (y/n)"
        if ($continue -ne "y") {
            exit 1
        }
    }
}

# Refresh PATH to include mosquitto if it was just installed
if ($mosquittoInstalled) {
    $env:Path = [System.Environment]::GetEnvironmentVariable("Path", "Machine") + ";" + [System.Environment]::GetEnvironmentVariable("Path", "User")
}

Write-Host ""

# Step 2: Download MQTT-SN Gateway
Write-Host "Step 2: Downloading MQTT-SN Gateway..." -ForegroundColor Yellow
$tempDir = $env:TEMP
$zipPath = Join-Path $tempDir "paho-mqtt-sn-master.zip"
$extractDir = Join-Path $tempDir "paho.mqtt-sn.embedded-c-master"
$gatewayDir = Join-Path $extractDir "MQTTSNGateway"

if (Test-Path $gatewayDir) {
    Write-Host "✓ Gateway source already exists at $gatewayDir" -ForegroundColor Green
} else {
    Write-Host "Downloading from GitHub..." -ForegroundColor Cyan
    try {
        Invoke-WebRequest -Uri "https://github.com/eclipse/paho.mqtt-sn.embedded-c/archive/master.zip" -OutFile $zipPath -UseBasicParsing
        Write-Host "✓ Download complete" -ForegroundColor Green
        
        Write-Host "Extracting..." -ForegroundColor Cyan
        Expand-Archive -Path $zipPath -DestinationPath $tempDir -Force
        Write-Host "✓ Extraction complete" -ForegroundColor Green
    } catch {
        Write-Host "❌ Failed to download or extract gateway source!" -ForegroundColor Red
        Write-Host $_.Exception.Message -ForegroundColor Red
        exit 1
    }
}

Write-Host ""

# Step 3: Build the gateway
Write-Host "Step 3: Building the gateway..." -ForegroundColor Yellow
$gatewayExe = Join-Path $gatewayDir "MQTT-SNGateway.exe"

if (Test-Path $gatewayExe) {
    Write-Host "✓ Gateway executable already exists" -ForegroundColor Green
} else {
    Write-Host "Checking for build tools..." -ForegroundColor Cyan
    
    # Check for MinGW
    $mingwFound = $false
    $gccPath = $null
    
    # Check common MinGW locations
    $mingwPaths = @(
        "C:\msys64\mingw64\bin\gcc.exe",
        "C:\MinGW\bin\gcc.exe",
        "C:\mingw64\bin\gcc.exe"
    )
    
    foreach ($path in $mingwPaths) {
        if (Test-Path $path) {
            $gccPath = $path
            $mingwFound = $true
            Write-Host "✓ Found MinGW at: $path" -ForegroundColor Green
            break
        }
    }
    
    # Check if gcc is in PATH
    if (-not $mingwFound) {
        $gccCmd = Get-Command gcc -ErrorAction SilentlyContinue
        if ($gccCmd) {
            $gccPath = $gccCmd.Path
            $mingwFound = $true
            Write-Host "✓ Found gcc in PATH: $gccPath" -ForegroundColor Green
        }
    }
    
    if (-not $mingwFound) {
        Write-Host "❌ No C compiler found!" -ForegroundColor Red
        Write-Host ""
        Write-Host "To build the gateway, you need a C compiler:" -ForegroundColor Yellow
        Write-Host "  Option 1: Install MinGW-w64" -ForegroundColor Cyan
        Write-Host "    - Download from: https://www.mingw-w64.org/downloads/" -ForegroundColor Cyan
        Write-Host "    - Or use MSYS2: https://www.msys2.org/" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "  Option 2: Use Visual Studio Build Tools" -ForegroundColor Cyan
        Write-Host "    - Download from: https://visualstudio.microsoft.com/downloads/" -ForegroundColor Cyan
        Write-Host ""
        Write-Host "  Option 3: Use the Python gateway instead (recommended for Windows)" -ForegroundColor Cyan
        Write-Host "    - Run: python python_mqtt_gateway.py" -ForegroundColor Cyan
        Write-Host ""
        $continue = Read-Host "Continue anyway? (y/n)"
        if ($continue -ne "y") {
            exit 1
        }
    } else {
        Push-Location $gatewayDir
        try {
            Write-Host "Building gateway..." -ForegroundColor Cyan
            
            # Try building with makefile first
            if (Test-Path "src\Makefile") {
                Push-Location "src"
                & make
                Pop-Location
            } elseif (Test-Path "Makefile") {
                & make
            } else {
                # Manual build - try Windows-specific paths
                Write-Host "Building manually..." -ForegroundColor Cyan
                
                if (Test-Path "src\windows") {
                    # Windows-specific source files
                    $sourceFiles = Get-ChildItem -Path "src\windows\*.c" -Recurse
                    $includeDirs = @("-I./src", "-I./src/MQTTSNGateway")
                    & $gccPath -o "MQTT-SNGateway.exe" $sourceFiles.FullName -lpthread $includeDirs
                } elseif (Test-Path "src\linux") {
                    # Try Linux files (may not work on Windows)
                    $sourceFiles = Get-ChildItem -Path "src\linux\*.c", "src\MQTTSNGateway\*.c" -Recurse
                    $includeDirs = @("-I./src", "-I./src/MQTTSNGateway")
                    & $gccPath -o "MQTT-SNGateway.exe" $sourceFiles.FullName -lpthread $includeDirs 2>&1 | Out-String
                } else {
                    # Generic build attempt
                    $sourceFiles = Get-ChildItem -Path "src\*.c" -Recurse
                    if ($sourceFiles.Count -gt 0) {
                        $includeDirs = @("-I./src")
                        & $gccPath -o "MQTT-SNGateway.exe" $sourceFiles.FullName -lpthread $includeDirs
                    } else {
                        throw "No source files found"
                    }
                }
            }
            
            # Check if Windows executable was created
            if (Test-Path "MQTT-SNGateway.exe") {
                Write-Host "✓ Gateway built successfully!" -ForegroundColor Green
                $gatewayExe = (Resolve-Path "MQTT-SNGateway.exe").Path
            } elseif (Test-Path "MQTT-SNGateway") {
                # Unix executable - rename it
                Rename-Item "MQTT-SNGateway" "MQTT-SNGateway.exe"
                Write-Host "✓ Gateway built successfully!" -ForegroundColor Green
                $gatewayExe = (Resolve-Path "MQTT-SNGateway.exe").Path
            } else {
                throw "Build failed - executable not created"
            }
        } catch {
            Write-Host "❌ Build failed!" -ForegroundColor Red
            Write-Host $_.Exception.Message -ForegroundColor Red
            Write-Host ""
            Write-Host "Note: Building on Windows can be complex. Consider using:" -ForegroundColor Yellow
            Write-Host "  python python_mqtt_gateway.py" -ForegroundColor Cyan
            Pop-Location
            exit 1
        }
        Pop-Location
    }
}

Write-Host ""

# Step 4: Create configuration file
Write-Host "Step 4: Creating configuration file..." -ForegroundColor Yellow
$configPath = Join-Path $gatewayDir "gateway.conf"

$configContent = @"
GatewayID=1
GatewayName=WindowsGW
KeepAlive=900

# UDP Port for MQTT-SN
Port=5000

# MQTT Broker connection
BrokerName=localhost
BrokerPortNo=1883
BrokerSecurePortNo=8883

# Logging
SharedMemory=NO
"@

Set-Content -Path $configPath -Value $configContent -Encoding UTF8
Write-Host "✓ Configuration file created at: $configPath" -ForegroundColor Green

Write-Host ""

# Step 5: Start MQTT broker
Write-Host "Step 5: Starting MQTT broker..." -ForegroundColor Yellow

# Check if mosquitto service exists
$mosquittoService = Get-Service -Name "Mosquitto Broker" -ErrorAction SilentlyContinue

if ($mosquittoService) {
    Write-Host "Starting Mosquitto service..." -ForegroundColor Cyan
    try {
        Start-Service -Name "Mosquitto Broker" -ErrorAction Stop
        Write-Host "✓ Mosquitto service started" -ForegroundColor Green
    } catch {
        Write-Host "⚠ Could not start service (may already be running): $($_.Exception.Message)" -ForegroundColor Yellow
    }
} else {
    Write-Host "⚠ Mosquitto service not found" -ForegroundColor Yellow
    Write-Host "  You may need to start mosquitto manually or run it as a foreground process" -ForegroundColor Cyan
}

Write-Host ""

# Step 6: Test MQTT broker
Write-Host "Step 6: Testing MQTT broker..." -ForegroundColor Yellow
if (Get-Command mosquitto_pub -ErrorAction SilentlyContinue) {
    try {
        Write-Host "Testing broker connection..." -ForegroundColor Cyan
        Start-Process -FilePath "mosquitto_pub" -ArgumentList @("-h", "localhost", "-t", "test/topic", "-m", "Hello MQTT") -NoNewWindow -Wait -ErrorAction SilentlyContinue
        
        Write-Host "✓ Broker test complete (check for errors above)" -ForegroundColor Green
        Write-Host "  You can subscribe with: mosquitto_sub -h localhost -t test/topic" -ForegroundColor Cyan
    } catch {
        Write-Host "⚠ Could not test broker (may not be running)" -ForegroundColor Yellow
    }
} else {
    Write-Host "⚠ mosquitto_pub not found - broker test skipped" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=== Setup Complete! ===" -ForegroundColor Green
Write-Host ""
Write-Host "Gateway location: $gatewayDir" -ForegroundColor Cyan
Write-Host ""
Write-Host "To start the MQTT-SN Gateway, run:" -ForegroundColor Yellow
Write-Host "  cd `"$gatewayDir`"" -ForegroundColor White
if (Test-Path $gatewayExe) {
    Write-Host "  .\MQTT-SNGateway.exe gateway.conf" -ForegroundColor White
} else {
    Write-Host "  .\MQTT-SNGateway gateway.conf" -ForegroundColor White
}
Write-Host ""
Write-Host "Note: If building failed, use the Python gateway instead:" -ForegroundColor Yellow
Write-Host "  python python_mqtt_gateway.py" -ForegroundColor Cyan
Write-Host ""
Write-Host "The gateway will listen on UDP port 5000 and forward to MQTT broker on port 1883" -ForegroundColor Green

