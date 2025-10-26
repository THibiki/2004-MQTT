# Build and Flash MQTT-SN Client for Pico W
# Run this script from PowerShell: .\build_and_flash.ps1

Write-Host "MQTT-SN Client - Build and Flash Script" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

# Change to project directory
$projectDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $projectDir

Write-Host "Project directory: $projectDir" -ForegroundColor Green

# Step 1: Check if build directory exists
$buildDir = Join-Path $projectDir "build"
if (Test-Path $buildDir) {
    Write-Host "Cleaning previous build..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir
}

Write-Host "Creating build directory..." -ForegroundColor Yellow
New-Item -ItemType Directory -Path $buildDir -Force | Out-Null

# Step 2: Configure CMake
Write-Host ""
Write-Host "Configuring CMake..." -ForegroundColor Yellow
Set-Location $buildDir
cmake .. -G Ninja

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    exit 1
}

# Step 3: Build
Write-Host ""
Write-Host "Building project..." -ForegroundColor Yellow
ninja

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit 1
}

# Step 4: Locate .uf2 file
$uf2File = Join-Path $buildDir "MQTT\mqtt_sn_client\mqtt_sn_client.uf2"

if (Test-Path $uf2File) {
    Write-Host ""
    Write-Host "Build successful!" -ForegroundColor Green
    Write-Host "UF2 file location: $uf2File" -ForegroundColor Green
    Write-Host ""
    
    # Check if Pico W is connected (in BOOTSEL mode)
    Write-Host "Checking for Pico W..." -ForegroundColor Yellow
    
    # Check for RPI-RP2 drive
    $rpiDrive = Get-WmiObject -Class Win32_LogicalDisk | Where-Object { $_.VolumeName -eq "RPI-RP2" }
    
    if ($rpiDrive) {
        $drive = $rpiDrive.DeviceID
        Write-Host "Pico W found at: $drive" -ForegroundColor Green
        Write-Host ""
        
        Write-Host "Copying UF2 file to Pico W..." -ForegroundColor Yellow
        Copy-Item $uf2File "$drive\RPI_UPDATE.UF2" -Force
        Write-Host "Flash successful! Pico W will reboot automatically." -ForegroundColor Green
    } else {
        Write-Host "Pico W not found in BOOTSEL mode." -ForegroundColor Yellow
        Write-Host "Please:" -ForegroundColor Yellow
        Write-Host "1. Hold the BOOTSEL button" -ForegroundColor Yellow
        Write-Host "2. Connect the USB cable" -ForegroundColor Yellow
        Write-Host "3. Release the BOOTSEL button" -ForegroundColor Yellow
        Write-Host "4. Run this script again" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "OR manually copy:" -ForegroundColor Yellow
        Write-Host "  Source: $uf2File" -ForegroundColor Yellow
        Write-Host "  Destination: Pico W USB drive" -ForegroundColor Yellow
    }
} else {
    Write-Host "UF2 file not found at: $uf2File" -ForegroundColor Red
    Write-Host "Build may have failed. Check build output above." -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Next steps:" -ForegroundColor Cyan
Write-Host "1. Configure WiFi credentials in MQTT/mqtt_sn_client/main.c" -ForegroundColor Yellow
Write-Host "2. Set up MQTT-SN Gateway on your network" -ForegroundColor Yellow
Write-Host "3. Monitor serial output at 115200 baud" -ForegroundColor Yellow
Write-Host ""
Write-Host "See TESTING_PROCEDURE.md for detailed instructions" -ForegroundColor Cyan

