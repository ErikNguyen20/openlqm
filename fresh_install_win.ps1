# fresh_install_win.ps1
# Fresh configure/build/install/package script for OpenLQM on Windows 11.
# Run from Developer PowerShell for VS 2022.
# ---------------------------------------------------------
# cd C:\Users\USER\projects\openlqm
# Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
# .\fresh-install_win.ps1 -RepoRoot "C:\Users\USER\projects\openlqm" -PackageFileName "openlqm-1.0.1-win64" -SkipClean
# ---------------------------------------------------------
# Note: -SkipClean is an optional flag that skips cleaning the build/package folders.

param(
    [string]$RepoRoot = $PSScriptRoot,
    [string]$InstallPrefix = "",
    [string]$VcpkgRoot = "C:\dev\vcpkg",
    [string]$KitwareCMakeBin = "C:\Program Files\CMake\bin",
    [string]$PackageFileName = "openlqm-1.0.1-win64",
    [switch]$SkipClean
)

$ErrorActionPreference = "Stop"
$OriginalLocation = Get-Location

# -------------------------------
# Helpers
# -------------------------------

function Write-Step {
    param([string]$Message)

    Write-Host ""
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Assert-PathExists {
    param(
        [string]$Path,
        [string]$Description
    )

    if (-not (Test-Path $Path)) {
        throw "$Description not found: $Path"
    }
}

function Assert-SafeDeletePath {
    param([string]$PathToDelete)

    if ([string]::IsNullOrWhiteSpace($PathToDelete)) {
        throw "Refusing to delete an empty path."
    }

    $fullPath = [System.IO.Path]::GetFullPath($PathToDelete).TrimEnd('\')

    $dangerousPaths = @(
        "C:",
        "C:\",
        $env:SystemDrive,
        $env:SystemRoot,
        $env:USERPROFILE,
        "C:\Users",
        "C:\Program Files",
        "C:\Program Files (x86)"
    ) | Where-Object { $_ } | ForEach-Object {
        [System.IO.Path]::GetFullPath($_).TrimEnd('\')
    }

    if ($dangerousPaths -contains $fullPath) {
        throw "Refusing to delete dangerous path: $fullPath"
    }

    return $fullPath
}

function Add-ToPathOnce {
    param([string]$PathToAdd)

    if ([string]::IsNullOrWhiteSpace($PathToAdd)) {
        return
    }

    $FullPathToAdd = [System.IO.Path]::GetFullPath($PathToAdd).TrimEnd('\')

    $ExistingPaths = $env:Path -split ';' | ForEach-Object {
        if (-not [string]::IsNullOrWhiteSpace($_)) {
            try {
                [System.IO.Path]::GetFullPath($_).TrimEnd('\')
            }
            catch {
                $_.TrimEnd('\')
            }
        }
    }

    if ($ExistingPaths -notcontains $FullPathToAdd) {
        $env:Path = "$FullPathToAdd;$env:Path"
    }
}

# -------------------------------
# Resolve paths
# -------------------------------

$RepoRoot = [System.IO.Path]::GetFullPath($RepoRoot)

# Default install/package staging folder:
#   <repo root>\package
if ([string]::IsNullOrWhiteSpace($InstallPrefix)) {
    $InstallPrefix = Join-Path $RepoRoot "package"
}

$InstallPrefix = [System.IO.Path]::GetFullPath($InstallPrefix)
$BuildDir = Join-Path $RepoRoot "build"

$VcpkgExe = Join-Path $VcpkgRoot "vcpkg.exe"
$ToolchainFile = Join-Path $VcpkgRoot "scripts\buildsystems\vcpkg.cmake"

# -------------------------------
# Basic validation
# -------------------------------
try {

Write-Step "Validating paths"

Assert-PathExists $RepoRoot "OpenLQM repo root"
Assert-PathExists (Join-Path $RepoRoot "CMakeLists.txt") "Top-level CMakeLists.txt"
Assert-PathExists $KitwareCMakeBin "Kitware CMake bin directory"
Assert-PathExists $VcpkgExe "vcpkg executable"
Assert-PathExists $ToolchainFile "vcpkg toolchain file"

Write-Host "Repo root:       $RepoRoot"
Write-Host "Build dir:       $BuildDir"
Write-Host "Install prefix:  $InstallPrefix"
Write-Host "vcpkg root:      $VcpkgRoot"
Write-Host "Package name:    $PackageFileName"

# -------------------------------
# Session-only environment setup
# -------------------------------

Write-Step "Setting session-only PATH for Kitware CMake, standalone vcpkg, and WiX dotnet tool"

$env:VCPKG_ROOT = $VcpkgRoot

Add-ToPathOnce $KitwareCMakeBin
Add-ToPathOnce $VcpkgRoot
Add-ToPathOnce "$env:USERPROFILE\.dotnet\tools"

# -------------------------------
# Tool checks
# -------------------------------

Write-Step "Checking build tools"

Write-Host "`nCMake:"
cmake --version

Write-Host "`nCMake path:"
where.exe cmake

Write-Host "`nvcpkg:"
& $VcpkgExe version

Write-Host "`nvcpkg toolchain:"
Write-Host $ToolchainFile

Write-Host "`nMSVC compiler:"
cl

Write-Host "`nWiX:"
where.exe wix
wix --version

# -------------------------------
# Clean old build/package folders
# -------------------------------

if (-not $SkipClean) {
    Write-Step "Cleaning old build and package folders"

    $SafeBuildDir = Assert-SafeDeletePath $BuildDir
    $SafeInstallPrefix = Assert-SafeDeletePath $InstallPrefix

    Write-Host "Deleting build dir:     $SafeBuildDir"
    Write-Host "Deleting package dir:   $SafeInstallPrefix"

    Remove-Item -Recurse -Force $SafeBuildDir -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force $SafeInstallPrefix -ErrorAction SilentlyContinue
}
else {
    Write-Step "Skipping clean because -SkipClean was provided"
}

# -------------------------------
# Configure
# -------------------------------

Write-Step "Configuring OpenLQM"

Set-Location $RepoRoot

$InstallPrefixForCMake = $InstallPrefix.Replace('\', '/')

cmake -S . -B build `
    -DCMAKE_CONFIGURATION_TYPES=Release `
    -DCMAKE_INSTALL_PREFIX="$InstallPrefixForCMake" `
    -DCMAKE_TOOLCHAIN_FILE="$ToolchainFile"

# -------------------------------
# Build
# -------------------------------

Write-Step "Building OpenLQM Release"

cmake --build build --config Release

# -------------------------------
# Install / stage package folder
# -------------------------------

Write-Step "Installing OpenLQM into package folder"

cmake --install build --config Release

# -------------------------------
# Verify installed files
# -------------------------------

Write-Step "Verifying package folder"

$InstallBin = Join-Path $InstallPrefix "bin"
$InstalledExe = Join-Path $InstallBin "openlqm.exe"
$InstalledModel = Join-Path $InstallBin "openlqm_model.xml"

Assert-PathExists $InstallBin "Package bin folder"
Assert-PathExists $InstalledExe "Packaged openlqm.exe"
Assert-PathExists $InstalledModel "Packaged openlqm_model.xml"

Get-ChildItem $InstallBin

Write-Step "Testing packaged openlqm.exe --version"

& $InstalledExe --version

# -------------------------------
# Package MSI with WiX
# -------------------------------

Write-Step "Creating MSI with CPack/WiX"

Set-Location $BuildDir

cpack -C Release -G WIX -D CPACK_PACKAGE_FILE_NAME="$PackageFileName"

# -------------------------------
# Final output and hash
# -------------------------------

Write-Step "Final MSI output"

$MsiPath = Join-Path $BuildDir "$PackageFileName.msi"

Assert-PathExists $MsiPath "MSI package"

Get-ChildItem $MsiPath

Write-Host ""
Write-Host "SHA-256:" -ForegroundColor Green
Get-FileHash $MsiPath -Algorithm SHA256

Write-Host ""
Write-Host "Done." -ForegroundColor Green
Write-Host "MSI: $MsiPath" -ForegroundColor Green
Write-Host "Package folder: $InstallPrefix" -ForegroundColor Green

}
finally {
    Set-Location $OriginalLocation
}
