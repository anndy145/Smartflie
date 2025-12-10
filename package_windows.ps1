# package_windows.ps1
# Script to collect distribution files consistently
# Usage: .\package_windows.ps1

$ErrorActionPreference = "Stop"

# --- Configuration ---
$ProjectRoot = Get-Location
$BuildDir = Join-Path $ProjectRoot "build" 
$ExeName = "SmartFileOrganizer.exe" 
$OutputDir = Join-Path $ProjectRoot "release_staging"
# If Qt is not in PATH, specify bin dir here, e.g. "C:\Qt\6.6.0\msvc2019_64\bin"
$QtBinDir = "C:\Qt\6.10.1\mingw_64\bin" 

# Validate Source
$ExePath = Join-Path $BuildDir $ExeName
if (-not (Test-Path $ExePath)) {
    Write-Host "Error: Cannot find $ExePath" -ForegroundColor Red
    Write-Host "Please build the project in Release mode first."
    exit 1
}

# 1. Clean Output Directory
Write-Host "Cleaning output directory: $OutputDir..."
if (Test-Path $OutputDir) { Remove-Item $OutputDir -Recurse -Force }
New-Item -ItemType Directory -Path $OutputDir | Out-Null

# 2. Copy Executable
Write-Host "Copying executable..."
Copy-Item $ExePath $OutputDir

# 3. Run windeployqt
Write-Host "Running windeployqt to capture Qt dependencies..."
$Windeployqt = "windeployqt.exe"
if ($QtBinDir -ne "" -and (Test-Path $QtBinDir)) {
    $Windeployqt = Join-Path $QtBinDir "windeployqt.exe"
}

try {
    # Check if windeployqt exists in path or specified dir
    if ($QtBinDir -eq "" -and (Get-Command $Windeployqt -ErrorAction SilentlyContinue) -eq $null) {
        throw "windeployqt not found in PATH"
    }
    
    & $Windeployqt --dir $OutputDir "$OutputDir\$ExeName" --no-translations --no-compiler-runtime --no-opengl-sw
    # Remove optional heavy DLLs if they exist
    $UnusedDlls = @("D3Dcompiler_47.dll", "opengl32sw.dll")
    foreach ($dll in $UnusedDlls) {
        $path = Join-Path $OutputDir $dll
        if (Test-Path $path) { Remove-Item $path -Force }
    }
} catch {
    Write-Host "Error: Failed to run windeployqt. Ensure it is in PATH or set `$QtBinDir` in script." -ForegroundColor Red
    Write-Host "Details: $_"
    exit 1
}

# 4. Copy Models
$ModelSrc = Join-Path $ProjectRoot "models"
if (Test-Path $ModelSrc) {
    Write-Host "Copying models directory..."
    Copy-Item $ModelSrc $OutputDir -Recurse
} else {
    Write-Host "Warning: 'models' directory not found. Please create it manually." -ForegroundColor Yellow
}

# 5. Copy Llama Dependency
$LlamaDll = Join-Path $BuildDir "llama.dll"
if (Test-Path $LlamaDll) {
    Copy-Item $LlamaDll $OutputDir
    Write-Host "Copying llama.dll..."
}



# 6. Create Portable ZIP
$ZipPath = Join-Path $ProjectRoot "SmartFile_Portable_v1.0.zip"
Write-Host "Creating Portable ZIP: $ZipPath..."
if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }
Compress-Archive -Path "$OutputDir\*" -DestinationPath $ZipPath -Force

Write-Host "Packaging Complete!" -ForegroundColor Green
Write-Host "  - Portable ZIP: $ZipPath"

# Cleanup Staging (Optional)
Write-Host "Cleaning up staging directory to save space..."
if (Test-Path $OutputDir) { Remove-Item $OutputDir -Recurse -Force }
Write-Host "Done."
