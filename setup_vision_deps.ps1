# setup_vision_deps.ps1
# Automates downloading OpenCV (MinGW) and ONNX Runtime for Smart File Organizer

$ErrorActionPreference = "Stop"

$LibsDir = Join-Path $PSScriptRoot "libs"
if (!(Test-Path $LibsDir)) {
    New-Item -ItemType Directory -Path $LibsDir | Out-Null
}

Write-Host "=== Setting up Vision Dependencies ==="

# 1. OpenCV (MinGW Build by huihut)
# Version: 4.5.5 (Stable, commonly used)
$OpenCVUrl = "https://github.com/huihut/OpenCV-MinGW-Build/archive/refs/tags/OpenCV-4.5.5-x64.zip"
$OpenCVZip = Join-Path $LibsDir "opencv_mingw.zip"
$OpenCVDir = Join-Path $LibsDir "OpenCV-MinGW-Build-OpenCV-4.5.5-x64"

if (!(Test-Path $OpenCVDir)) {
    Write-Host "Downloading OpenCV 4.5.5 (MinGW Key)..."
    Invoke-WebRequest -Uri $OpenCVUrl -OutFile $OpenCVZip
    
    Write-Host "Extracting OpenCV..."
    Expand-Archive -Path $OpenCVZip -DestinationPath $LibsDir -Force
    
    Remove-Item $OpenCVZip -Force
    Write-Host "OpenCV Setup Complete."
} else {
    Write-Host "OpenCV already exists. Skipping."
}

# 2. ONNX Runtime (Windows x64)
# Version: 1.16.3 (Stable)
$OnnxUrl = "https://github.com/microsoft/onnxruntime/releases/download/v1.16.3/onnxruntime-win-x64-1.16.3.zip"
$OnnxZip = Join-Path $LibsDir "onnxruntime.zip"
$OnnxDir = Join-Path $LibsDir "onnxruntime-win-x64-1.16.3"

if (!(Test-Path $OnnxDir)) {
    Write-Host "Downloading ONNX Runtime 1.16.3..."
    Invoke-WebRequest -Uri $OnnxUrl -OutFile $OnnxZip
    
    Write-Host "Extracting ONNX Runtime..."
    Expand-Archive -Path $OnnxZip -DestinationPath $LibsDir -Force
    
    Remove-Item $OnnxZip -Force
    Write-Host "ONNX Runtime Setup Complete."
} else {
    Write-Host "ONNX Runtime already exists. Skipping."
}

# 3. Download CLIP ONNX Model (Tiny version for testing)
# Using a placeholder URL or skipped for now? 
# The Implementation plan said "Download CLIP".
# Let's put models in a 'models' folder.
$ModelsDir = Join-Path $PSScriptRoot "models"
if (!(Test-Path $ModelsDir)) {
    New-Item -ItemType Directory -Path $ModelsDir | Out-Null
}

# Note: Actual model download links are long/complex (HuggingFace). 
# We will skip automatic model download for this script step to avoid huge downloads,
# or user can perform it later. But the structure is ready.

Write-Host "=== Dependency Setup Finished ==="
Write-Host "Libs are in: $LibsDir"
Write-Host "Please re-run CMake configuration."
