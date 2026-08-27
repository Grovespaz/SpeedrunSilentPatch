<#
.SYNOPSIS
Builds and packages SpeedrunSilentPatch for one or more GTA games.

.DESCRIPTION
Rebuilds the selected projects in Shipping|Win32 and creates both a release
archive and a debugging-symbol archive for each selected game. III and VC also
build and package the shared DDraw project.

.PARAMETER Game
The games to package. Accepts III, VC, SA, or All. Multiple games may be passed
as a comma-separated list. The default is All.

.PARAMETER OutputDirectory
The directory that receives the ZIP archives. The default is the repository's
Shipping directory.

.PARAMETER MSBuildPath
An optional path to MSBuild.exe. When omitted, the script checks PATH and then
uses Visual Studio's vswhere.exe to locate MSBuild.

.PARAMETER SkipBuild
Packages the files already present in Shipping without invoking MSBuild.

.EXAMPLE
.\Build-SpeedrunSilentPatch.ps1 -Game VC

.EXAMPLE
.\Build-SpeedrunSilentPatch.ps1 -Game III,SA

.EXAMPLE
.\Build-SpeedrunSilentPatch.ps1 -Game All -OutputDirectory .\Packages
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('III', 'VC', 'SA', 'All')]
    [string[]] $Game = @('All'),

    [string] $OutputDirectory,

    [string] $MSBuildPath,

    [switch] $SkipBuild
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = $PSScriptRoot
$shippingDirectory = Join-Path $repositoryRoot 'Shipping'

function Resolve-MSBuildPath {
    param([string] $RequestedPath)

    if ($RequestedPath) {
        if (Test-Path -LiteralPath $RequestedPath -PathType Leaf) {
            return (Resolve-Path -LiteralPath $RequestedPath).Path
        }

        $requestedCommand = Get-Command -Name $RequestedPath -CommandType Application -ErrorAction SilentlyContinue
        if ($requestedCommand) {
            return $requestedCommand.Path
        }

        throw "MSBuild was not found at '$RequestedPath'."
    }

    $pathCommand = Get-Command -Name 'MSBuild.exe' -CommandType Application -ErrorAction SilentlyContinue
    if ($pathCommand) {
        return $pathCommand.Path
    }

    $vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (Test-Path -LiteralPath $vswherePath -PathType Leaf) {
        $locatedPaths = @(& $vswherePath -latest -products '*' -requires Microsoft.Component.MSBuild -find 'MSBuild\**\Bin\MSBuild.exe')
        $locatedPath = $locatedPaths | Select-Object -First 1
        if ($locatedPath -and (Test-Path -LiteralPath $locatedPath -PathType Leaf)) {
            return (Resolve-Path -LiteralPath $locatedPath).Path
        }
    }

    throw 'Unable to locate MSBuild.exe. Install Visual Studio Build Tools, add MSBuild to PATH, or pass -MSBuildPath.'
}

function Invoke-ShippingBuild {
    param(
        [string] $Executable,
        [string] $ProjectPath
    )

    $projectName = [IO.Path]::GetFileNameWithoutExtension($ProjectPath)
    Write-Host "Building $projectName (Shipping|Win32)..."

    $arguments = @(
        $ProjectPath
        '/t:Rebuild'
        '/p:Configuration=Shipping'
        '/p:Platform=Win32'
        '/p:SpeedrunSilentPatchGTAIIIDir='
        '/p:SpeedrunSilentPatchVCDir='
        '/p:SpeedrunSilentPatchSADir='
        '/m'
        '/nologo'
        '/verbosity:minimal'
    )
    & $Executable @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "MSBuild failed for $projectName with exit code $LASTEXITCODE."
    }
}

function Get-BuildNumber {
    param([string] $MetadataPath)

    [xml] $metadata = Get-Content -LiteralPath $MetadataPath -Raw
    $buildIdNode = $metadata.SelectSingleNode("/*[local-name()='Project']/*[local-name()='PropertyGroup']/*[local-name()='SILENTPATCH_BUILD_ID']")
    if (-not $buildIdNode -or [string]::IsNullOrWhiteSpace($buildIdNode.InnerText)) {
        throw "SILENTPATCH_BUILD_ID was not found in '$MetadataPath'."
    }

    $buildNumber = $buildIdNode.InnerText.Trim()
    if ($buildNumber.IndexOfAny([IO.Path]::GetInvalidFileNameChars()) -ge 0) {
        throw "SILENTPATCH_BUILD_ID '$buildNumber' in '$MetadataPath' is not valid in a file name."
    }

    return $buildNumber
}

function New-FileSpec {
    param(
        [string] $Source,
        [string] $Destination = ([IO.Path]::GetFileName($Source))
    )

    return [pscustomobject]@{
        Source = $Source
        Destination = $Destination
    }
}

function Get-DebugArtifactSpecs {
    param(
        [string] $BaseName,
        [string] $BinaryExtension
    )

    New-FileSpec -Source "Shipping\$BaseName$BinaryExtension"
    foreach ($extension in @('.exp', '.iobj', '.ipdb', '.lib', '.pdb')) {
        New-FileSpec -Source "Shipping\$BaseName$extension"
    }
}

function New-PackageArchive {
    param(
        [string] $ArchiveName,
        [object[]] $Files,
        [string] $StagingParent,
        [string] $DestinationDirectory
    )

    $stagingDirectory = Join-Path $StagingParent ([IO.Path]::GetFileNameWithoutExtension($ArchiveName))
    [void] [IO.Directory]::CreateDirectory($stagingDirectory)

    foreach ($file in $Files) {
        $sourcePath = Join-Path $repositoryRoot $file.Source
        if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
            throw "Required package file is missing: $sourcePath"
        }

        $destinationPath = Join-Path $stagingDirectory $file.Destination
        $destinationParent = Split-Path -Parent $destinationPath
        [void] [IO.Directory]::CreateDirectory($destinationParent)
        Copy-Item -LiteralPath $sourcePath -Destination $destinationPath
    }

    $temporaryArchive = Join-Path $StagingParent ([guid]::NewGuid().ToString('N') + '.zip')
    $archivePath = Join-Path $DestinationDirectory $ArchiveName
    [IO.Compression.ZipFile]::CreateFromDirectory(
        $stagingDirectory,
        $temporaryArchive,
        [IO.Compression.CompressionLevel]::Optimal,
        $false
    )
    Move-Item -LiteralPath $temporaryArchive -Destination $archivePath -Force

    Write-Host "Created $archivePath"
}

$selectedGames = if ($Game -contains 'All') {
    @('III', 'VC', 'SA')
} else {
    @('III', 'VC', 'SA') | Where-Object { $Game -contains $_ }
}

if (-not $selectedGames) {
    throw 'Select at least one game.'
}

if (-not $OutputDirectory) {
    $OutputDirectory = $shippingDirectory
}
[void] [IO.Directory]::CreateDirectory($OutputDirectory)
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path

$ddrawDebugFiles = @(Get-DebugArtifactSpecs -BaseName 'ddraw' -BinaryExtension '.dll')

$iiiDebugFiles = @(
    Get-DebugArtifactSpecs -BaseName 'SpeedrunSilentPatchIII' -BinaryExtension '.asi'
    $ddrawDebugFiles
    New-FileSpec -Source 'Config\SpeedrunSilentPatchIII.ini'
)
$vcDebugFiles = @(
    Get-DebugArtifactSpecs -BaseName 'SpeedrunSilentPatchVC' -BinaryExtension '.asi'
    $ddrawDebugFiles
    New-FileSpec -Source 'Config\SpeedrunSilentPatchVC.ini'
)
$saDebugFiles = @(
    Get-DebugArtifactSpecs -BaseName 'SpeedrunSilentPatchSA' -BinaryExtension '.asi'
    New-FileSpec -Source 'Config\SpeedrunSilentPatchSA.ini'
    New-FileSpec -Source 'Assets\vorbisFile.dll'
    New-FileSpec -Source 'Assets\vorbisHooked.dll'
)

$gameSpecs = @{
    III = [pscustomobject]@{
        Project = 'SilentPatchIII\SilentPatchIII.vcxproj'
        Metadata = 'SilentPatchIII\versionmeta.props'
        PublicFiles = @(
            New-FileSpec -Source 'Shipping\SpeedrunSilentPatchIII.asi'
            New-FileSpec -Source 'Config\SpeedrunSilentPatchIII.ini'
            New-FileSpec -Source 'ReadMes\ReadMe-III.txt'
            New-FileSpec -Source 'Shipping\ddraw.dll'
        )
        DebugFiles = $iiiDebugFiles
    }
    VC = [pscustomobject]@{
        Project = 'SilentPatchVC\SilentPatchVC.vcxproj'
        Metadata = 'SilentPatchVC\versionmeta.props'
        PublicFiles = @(
            New-FileSpec -Source 'Shipping\SpeedrunSilentPatchVC.asi'
            New-FileSpec -Source 'Config\SpeedrunSilentPatchVC.ini'
            New-FileSpec -Source 'ReadMes\ReadMe-VC.txt'
            New-FileSpec -Source 'Shipping\ddraw.dll'
            New-FileSpec -Source 'Assets\FIST-DMCA.mp3' -Destination 'SSP\FIST-DMCA.mp3'
        )
        DebugFiles = $vcDebugFiles
    }
    SA = [pscustomobject]@{
        Project = 'SilentPatchSA\SilentPatchSA.vcxproj'
        Metadata = 'SilentPatchSA\versionmeta.props'
        PublicFiles = @(
            New-FileSpec -Source 'Shipping\SpeedrunSilentPatchSA.asi'
            New-FileSpec -Source 'Config\SpeedrunSilentPatchSA.ini'
            New-FileSpec -Source 'ReadMes\ReadMe-SA.txt'
            New-FileSpec -Source 'Assets\vorbisFile.dll'
            New-FileSpec -Source 'Assets\vorbisHooked.dll'
        )
        DebugFiles = $saDebugFiles
    }
}

if (-not $SkipBuild) {
    $resolvedMSBuildPath = Resolve-MSBuildPath -RequestedPath $MSBuildPath

    if (($selectedGames -contains 'III') -or ($selectedGames -contains 'VC')) {
        Invoke-ShippingBuild -Executable $resolvedMSBuildPath -ProjectPath (Join-Path $repositoryRoot 'DDraw\DDraw.vcxproj')
    }

    foreach ($selectedGame in $selectedGames) {
        Invoke-ShippingBuild -Executable $resolvedMSBuildPath -ProjectPath (Join-Path $repositoryRoot $gameSpecs[$selectedGame].Project)
    }
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$stagingRoot = Join-Path ([IO.Path]::GetTempPath()) ('SpeedrunSilentPatch-package-' + [guid]::NewGuid().ToString('N'))
[void] [IO.Directory]::CreateDirectory($stagingRoot)

try {
    foreach ($selectedGame in $selectedGames) {
        $spec = $gameSpecs[$selectedGame]
        $buildNumber = Get-BuildNumber -MetadataPath (Join-Path $repositoryRoot $spec.Metadata)
        $archivePrefix = "SpeedrunSilentPatch-$selectedGame-build$buildNumber"

        New-PackageArchive -ArchiveName "$archivePrefix.zip" -Files $spec.PublicFiles -StagingParent $stagingRoot -DestinationDirectory $OutputDirectory
        New-PackageArchive -ArchiveName "$archivePrefix-pdbs.zip" -Files $spec.DebugFiles -StagingParent $stagingRoot -DestinationDirectory $OutputDirectory
    }
} finally {
    if ([IO.Directory]::Exists($stagingRoot)) {
        [IO.Directory]::Delete($stagingRoot, $true)
    }
}
