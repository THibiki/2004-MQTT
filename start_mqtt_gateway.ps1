#!/usr/bin/env pwsh

Write-Host "=== Starting MQTT-SN Gateway ===" -ForegroundColor Cyan
Write-Host ""

# Check if gateway exists
$tempDir = $env:TEMP
$gatewayDir = Join-Path $tempDir "paho.mqtt-sn.embedded-c-master\MQTTSNGateway"

if (-not (Test-Path $gatewayDir)) {
    Write-Host "❌ MQTT-SN Gateway not found!" -ForegroundColor Red
    Write-Host "Please run setup_mqtt_gateway.ps1 first" -ForegroundColor Yellow
    exit 1
}

$gatewayExe = Join-Path $gatewayDir "MQTT-SNGateway.exe"
if (-not (Test-Path $gatewayExe)) {
    $gatewayExe = Join-Path $gatewayDir "MQTT-SNGateway"
    if (-not (Test-Path $gatewayExe)) {
        Write-Host "❌ MQTT-SNGateway binary not found!" -ForegroundColor Red
        Write-Host "Please run setup_mqtt_gateway.ps1 first to build it" -ForegroundColor Yellow
        Write-Host ""
        Write-Host "Alternatively, use the Python gateway:" -ForegroundColor Cyan
        Write-Host "  python python_mqtt_gateway.py" -ForegroundColor White
        exit 1
    }
}

$configPath = Join-Path $gatewayDir "gateway.conf"
if (-not (Test-Path $configPath)) {
    Write-Host "❌ gateway.conf not found!" -ForegroundColor Red
    Write-Host "Please run setup_mqtt_gateway.ps1 first" -ForegroundColor Yellow
    exit 1
}

Write-Host "✅ Starting MQTT broker..." -ForegroundColor Green
$mosquittoService = Get-Service -Name "Mosquitto Broker" -ErrorAction SilentlyContinue

if ($mosquittoService) {
    try {
        Start-Service -Name "Mosquitto Broker" -ErrorAction Stop
        Write-Host "✓ Mosquitto service started" -ForegroundColor Green
    } catch {
        if ($_.Exception.Message -like "*running*") {
            Write-Host "✓ Mosquitto service already running" -ForegroundColor Green
        } else {
            Write-Host "⚠ Could not start Mosquitto: $($_.Exception.Message)" -ForegroundColor Yellow
        }
    }
} else {
    Write-Host "⚠ Mosquitto service not found - you may need to start mosquitto manually" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "✅ Starting MQTT-SN Gateway on UDP port 5000..." -ForegroundColor Green
Write-Host "   Forwarding to MQTT broker on localhost:1883" -ForegroundColor Cyan
Write-Host ""
Write-Host "Press Ctrl+C to stop" -ForegroundColor Yellow
Write-Host "================================" -ForegroundColor Cyan
Write-Host ""

Set-Location $gatewayDir
& $gatewayExe gateway.conf

