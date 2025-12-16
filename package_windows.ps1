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
if ($QtBinDir -ne "") {
    $Windeployqt = Join-Path $QtBinDir "windeployqt.exe"
}

try {
    # Check if windeployqt exists in path or specified dir
    if ($QtBinDir -eq "" -and (Get-Command $Windeployqt -ErrorAction SilentlyContinue) -eq $null) {
        throw "windeployqt not found in PATH"
    }
    
    & $Windeployqt --dir $OutputDir "$OutputDir\$ExeName" --no-translations --no-compiler-runtime
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
# 6. Create Portable SFX (Self-Extracting Executable)
$SfxName = "SmartFile_Portable_v1.0.exe"
$SfxPath = Join-Path $ProjectRoot $SfxName
$SevenZip = "C:\Program Files\7-Zip\7z.exe"
$SfxModule = "C:\Program Files\7-Zip\7z.sfx"

if (Test-Path $SevenZip) {
    Write-Host "Creating Portable SFX: $SfxPath..."
    
    # 6a. Create 7z Archive
    $Archive7z = Join-Path $ProjectRoot "app.7z"
    if (Test-Path $Archive7z) { Remove-Item $Archive7z -Force }
    & $SevenZip a -t7z -mx9 "$Archive7z" "$OutputDir\*" | Out-Null
    
    # 6b. Create Config
    $ConfigPath = Join-Path $ProjectRoot "sfx_config.txt"
    $ConfigContent = ";!@Install@!UTF-8!`nTitle=""Smart File Organizer""`nExecuteFile=""SmartFileOrganizer.exe""`nGUIMode=""1""`n;!@InstallEnd@!"
    Set-Content -Path $ConfigPath -Value $ConfigContent -Encoding UTF8
    
    # 6c. Combine (Binary Copy)
    # copy /b 7z.sfx + config.txt + app.7z target.exe
    if (Test-Path $SfxModule) {
        cmd /c "copy /b ""$SfxModule"" + ""$ConfigPath"" + ""$Archive7z"" ""$SfxPath"""
        Write-Host "SFX Created successfully: $SfxPath"
    } else {
        Write-Host "SFX Module not found ($SfxModule), falling back to ZIP." -ForegroundColor Yellow
        $ZipPath = Join-Path $ProjectRoot "SmartFile_Portable_v1.0.zip"
        Compress-Archive -Path "$OutputDir\*" -DestinationPath $ZipPath -Force
        Write-Host "Created ZIP instead: $ZipPath"
    }

    # Cleanup temp
    if (Test-Path $Archive7z) { Remove-Item $Archive7z -Force }
    if (Test-Path $ConfigPath) { Remove-Item $ConfigPath -Force }

} else {
    Write-Host "7-Zip not found, falling back to standard ZIP."
    $ZipPath = Join-Path $ProjectRoot "SmartFile_Portable_v1.0.zip"
    Compress-Archive -Path "$OutputDir\*" -DestinationPath $ZipPath -Force
    Write-Host "Portable ZIP: $ZipPath"
}

Write-Host "Packaging Complete!" -ForegroundColor Green
Write-Host "  - Staging: $OutputDir"
if (Test-Path $SfxPath) { Write-Host "  - Portable Executable: $SfxPath" }
Write-Host "Next Step: Compile installer_windows.iss (Optional)"

# 7. Create Installer (Inno Setup)
Write-Host "Checking for Inno Setup..."
$ISCC = "ISCC.exe"
# Common paths for Inno Setup
$PossiblePaths = @(
    "C:\Program Files (x86)\Inno Setup 6\ISCC.exe",
    "C:\Program Files\Inno Setup 6\ISCC.exe"
)

if ((Get-Command $ISCC -ErrorAction SilentlyContinue) -eq $null) {
    foreach ($path in $PossiblePaths) {
        if (Test-Path $path) { 
            $ISCC = $path
            break 
        }
    }
}

if ((Get-Command $ISCC -ErrorAction SilentlyContinue) -ne $null -or (Test-Path $ISCC)) {
    Write-Host "Compiling Installer with Inno Setup..."
    & $ISCC "installer_windows.iss"
    Write-Host "Installer created successfully."
} else {
    Write-Host "Inno Setup (ISCC.exe) not found. Skipping installer generation." -ForegroundColor Yellow
    Write-Host "Please install Inno Setup 6+ to generate the .exe installer."
}

