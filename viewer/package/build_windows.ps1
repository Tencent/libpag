param($PAGPath, $PAGOptions, $DSAPrivateKey)

$OutputEncoding = [System.Text.Encoding]::UTF8
[Console]::InputEncoding = [System.Text.Encoding]::UTF8
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)

$PSDefaultParameterValues['*:Encoding'] = 'utf8'
$PSDefaultParameterValues['Out-File:Encoding'] = 'utf8'
$PSDefaultParameterValues['Export-Csv:Encoding'] = 'utf8'
$PSDefaultParameterValues['Set-Content:Encoding'] = 'utf8'

# ═══════════════════════════════════════════════════════════
# Logging functions
# ═══════════════════════════════════════════════════════════
$TOTAL_SECTIONS = 6
$SECTION_WIDTH = 60

function Print-Section {
    param([int]$Index, [string]$Title)
    $line = [string]::new([char]0x2550, $SECTION_WIDTH)
    Write-Host ""
    Write-Host $line -ForegroundColor Blue
    Write-Host "  $([char]0x25B6) [$Index/$TOTAL_SECTIONS] $Title" -ForegroundColor Blue
    Write-Host $line -ForegroundColor Blue
}

function Print-Step {
    param([string]$Text)
    Write-Host "  $([char]0x2192) $Text" -ForegroundColor Blue
}

function Log-Info {
    param([string]$Text)
    Write-Host "    [INFO] $Text" -ForegroundColor Blue
}

function Log-Success {
    param([string]$Text)
    Write-Host "    [OK] $Text" -ForegroundColor Green
}

function Log-Warning {
    param([string]$Text)
    Write-Host "    [WARN] $Text" -ForegroundColor Yellow
}

function Log-Error {
    param([string]$Text)
    Write-Host "    [ERROR] $Text" -ForegroundColor Red
}

function Exit-WithError {
    param([string]$Text)
    Log-Error $Text
    exit 1
}

function Invoke-CMakeBuild {
    param(
        [string]$SourceDir,
        [string]$BuildDir,
        [string]$Target,
        [string[]]$ConfigureArgs,
        [int]$Jobs = 16
    )
    & cmake -S $SourceDir -B $BuildDir @ConfigureArgs
    if ($LASTEXITCODE -ne 0) { Exit-WithError "CMake configure $Target failed" }

    & cmake --build $BuildDir --config Release -j $Jobs
    if ($LASTEXITCODE -ne 0) { Exit-WithError "CMake build $Target failed" }
}

# ═══════════════════════════════════════════════════════════
# 1 Initialize variables
# ═══════════════════════════════════════════════════════════
$BuildStartTime = Get-Date
Print-Section 1 "Initialize variables"

$majorVersion = if ($env:MajorVersion) { $env:MajorVersion } else { "1" }
$minorVersion = if ($env:MinorVersion) { $env:MinorVersion } else { "0" }
$buildNO = if ($env:BuildNumber) { $env:BuildNumber } else { "0" }

$AppVersion = "${majorVersion}.${minorVersion}.${buildNO}"
$CurrentTime = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
$RFCTime = Get-Date -Format "r"
$SourceDir = Split-Path $PSScriptRoot -Parent
$LibpagDir = Split-Path $SourceDir -Parent
$BuildDir = Join-Path $SourceDir "build_viewer"
$IsBetaVersion = if ($env:isBetaVersion -eq "true") { $true } else { $false }

Log-Info "Build Time: $CurrentTime"
Log-Info "Version: $AppVersion"

if ([string]::IsNullOrEmpty($env:PAG_DeployQt_Path) -or [string]::IsNullOrEmpty($env:PAG_Qt_Path) -or [string]::IsNullOrEmpty($env:PAG_AE_SDK_Path) -or [string]::IsNullOrEmpty($env:PAG_Openssl_Path)) {
    Exit-WithError "Please set [PAG_DeployQt_Path], [PAG_Qt_Path], [PAG_AE_SDK_Path] and [PAG_Openssl_Path] before build on Windows"
}

$Deployqt = $env:PAG_DeployQt_Path
$OpenSSL = $env:PAG_Openssl_Path
$QtPath = $env:PAG_Qt_Path
$QtCMakePath = Join-Path $QtPath "lib" | Join-Path -ChildPath "cmake"
$AESDKPath = $env:PAG_AE_SDK_Path

# ═══════════════════════════════════════════════════════════
# 2 Compile
# ═══════════════════════════════════════════════════════════
Print-Section 2 "Compile"
$x64BuildDir = Join-Path $BuildDir "x64"

# 2.1 Set environment variables
Print-Step "Detect MSVC toolset"
$VSPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
$VSDevEnv = Join-Path $VSPath "Common7\IDE\devenv.exe"
$MSVCToolsDir = Join-Path $VSPath 'VC\Tools\MSVC'
$MSVCVersions = Get-ChildItem $MSVCToolsDir -Directory | Select-Object -ExpandProperty Name
$MSVCToolSets = @()
foreach ($v in $MSVCVersions) {
    if ($v -match "^14\.(\d+)\.") {
        $MajorMinor = [int]$Matches[1]
        if ($MajorMinor -ge 30) { $MSVCToolSets += "v143" }
        elseif ($MajorMinor -ge 20) { $MSVCToolSets += "v142" }
        elseif ($MajorMinor -ge 10) { $MSVCToolSets += "v141" }
    }
}
$MSVCToolSets = $MSVCToolSets | Sort-Object -Unique -Descending
if ($MSVCToolSets.Count -eq 0) {
    Exit-WithError "No available MSVC toolset found"
}
$LatestMSVCToolSet = $MSVCToolSets | Select-Object -First 1
Log-Info "Using MSVC toolset: $LatestMSVCToolSet"

$WindowsSdkDir = [System.IO.Path]::Combine(${env:ProgramFiles(x86)}, "Windows Kits", "10")
$LatestSdkVersion = (Get-ChildItem (Join-Path $WindowsSdkDir "Lib") | Sort-Object Name -Descending | Select-Object -First 1).Name
if ([string]::IsNullOrEmpty($LatestSdkVersion)) {
    Exit-WithError "Windows SDK Lib directory not found or empty: $WindowsSdkDir\Lib"
}
$env:LIB = "${WindowsSdkDir}\Lib\${LatestSdkVersion}\um\x64;${WindowsSdkDir}\Lib\${LatestSdkVersion}\ucrt\x64;" + $env:LIB
Log-Info "Windows SDK: $LatestSdkVersion"

# 2.2 Compile PAGExporter
Print-Step "PAGExporter"
$PluginSourceDir = Join-Path $LibpagDir "exporter"
$x64BuildDirForPlugin = Join-Path $x64BuildDir "Plugin"

Invoke-CMakeBuild -SourceDir $PluginSourceDir -BuildDir $x64BuildDirForPlugin -Target "PAGExporter" `
    -ConfigureArgs @("-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_PREFIX_PATH=$QtCMakePath", "-DAE_SDK_PATH=$AESDKPath")

# 2.3 Compile PAGViewer
Print-Step "PAGViewer"
Invoke-CMakeBuild -SourceDir $SourceDir -BuildDir $x64BuildDir -Target "PAGViewer" `
    -ConfigureArgs @("-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_PREFIX_PATH=$QtCMakePath", "-DPAG_PATH=$PAGPath", "-DPAG_OPTIONS=$PAGOptions")

# 2.4 Compile H264EncoderTools
Print-Step "H264EncoderTools"
$EncoderToolsSourceDir = Join-Path $SourceDir "third_party\H264EncoderTools"
$EncoderToolsSlnPath = Join-Path $EncoderToolsSourceDir "H264EncoderTools.sln"
$EncoderToolsVcxprojPath = Join-Path $EncoderToolsSourceDir "H264EncoderTools.vcxproj"
(Get-Content $EncoderToolsVcxprojPath -Raw) -replace '<PlatformToolset>v14\d</PlatformToolset>', "<PlatformToolset>$LatestMSVCToolSet</PlatformToolset>" | Set-Content $EncoderToolsVcxprojPath
# Verify the substitution actually took effect; the regex above only matches v14x toolsets
$ToolsetCheck = Get-Content $EncoderToolsVcxprojPath -Raw
if ($ToolsetCheck -notmatch "<PlatformToolset>$([regex]::Escape($LatestMSVCToolSet))</PlatformToolset>") {
    Exit-WithError "Failed to set PlatformToolset to $LatestMSVCToolSet in vcxproj"
}

$process = Start-Process -FilePath $VSDevEnv -ArgumentList "`"$EncoderToolsSlnPath`" /Rebuild `"Release|x64`"" -Wait -PassThru
if ($process.ExitCode -ne 0) { Exit-WithError "Build H264EncoderTools failed" }

# ═══════════════════════════════════════════════════════════
# 3 Organize resources
# ═══════════════════════════════════════════════════════════
Print-Section 3 "Organize resources"

# 3.1 Copy files
Print-Step "Copy application files"
$TemplatePackageDir = Join-Path $SourceDir "package\templates\windows-packages"
New-Item -ItemType Directory -Path "$TemplatePackageDir\data\scripts" -Force | Out-Null
New-Item -ItemType Directory -Path "$TemplatePackageDir\meta" -Force | Out-Null
$PackageDir = Join-Path $BuildDir "com.tencent.pagplayer"

# Clean previous package to avoid incremental build issues
if (Test-Path $PackageDir) { Remove-Item -Path $PackageDir -Recurse -Force }
Copy-Item -Path $TemplatePackageDir -Destination $PackageDir -Recurse -Force

$GeneratedExePath = Join-Path $x64BuildDir "Release\PAGViewer.exe"
$ExeDir = Join-Path $PackageDir "data"
$ExePath = Join-Path $ExeDir "PAGViewer.exe"
Copy-Item -Path $GeneratedExePath -Destination $ExePath -Force

# 3.2 Copy PAGExporter and tools
Print-Step "Copy PAGExporter and tools"
$GeneratedPluginPath = Join-Path $x64BuildDirForPlugin "Release\PAGExporter.aex"
Copy-Item -Path $GeneratedPluginPath -Destination $ExeDir -Force

$GeneratedEncoderToolsPath = Join-Path $EncoderToolsSourceDir "x64\Release\H264EncoderTools.exe"
if (-not (Test-Path $GeneratedEncoderToolsPath)) {
    Exit-WithError "H264EncoderTools.exe not found at $GeneratedEncoderToolsPath"
}
Copy-Item -Path $GeneratedEncoderToolsPath -Destination $ExeDir -Force

# 3.3 Copy symbol files
Print-Step "Copy symbol files"
$PAGViewerPdbPath = Join-Path $x64BuildDir "Release\PAGViewer.pdb"
Copy-Item -Path $PAGViewerPdbPath -Destination $BuildDir -Force

$PAGExporterPdbPath = Join-Path $x64BuildDirForPlugin "Release\PAGExporter.pdb"
Copy-Item -Path $PAGExporterPdbPath -Destination $BuildDir -Force

# 3.4 Deploy Qt dependencies
Print-Step "Deploy Qt dependencies"
$QmlDir = Join-Path $SourceDir "assets\qml"
& $Deployqt $ExePath --qmldir=$QmlDir
if (-not $?) { Exit-WithError "Deploy Qt dependencies failed" }

# 3.5 Copy vendor libraries
Print-Step "Copy vendor libraries"
$WinSparklePath = Join-Path $SourceDir "vendor\winsparkle\win\x64\Release\WinSparkle.dll"
Copy-Item -Path $WinSparklePath -Destination $ExeDir -Force

$FfmoviePath = Join-Path $SourceDir "vendor\ffmovie\win\x64\ffmovie.dll"
Copy-Item -Path $FfmoviePath -Destination $ExeDir -Force

# 3.6 Copy VC++ Runtime DLLs
Print-Step "Copy VC++ Runtime DLLs"
$VCRedistDir = Join-Path $VSPath "VC\Redist\MSVC"
if (-not (Test-Path $VCRedistDir)) {
    Exit-WithError "VC++ Redist directory not found: $VCRedistDir"
}
$LatestRedistVersion = (Get-ChildItem $VCRedistDir -Directory | Where-Object { $_.Name -match "^\d+\." } | Sort-Object Name -Descending | Select-Object -First 1).Name
$VCRuntimeDir = Join-Path $VCRedistDir "$LatestRedistVersion\x64\Microsoft.VC143.CRT"
if (-not (Test-Path $VCRuntimeDir)) {
    $VCRuntimeDir = Join-Path $VCRedistDir "$LatestRedistVersion\x64\Microsoft.VC142.CRT"
}
if (-not (Test-Path $VCRuntimeDir)) {
    Exit-WithError "VC++ Runtime directory not found"
}
Log-Info "VC++ Runtime: $VCRuntimeDir"

$VCRuntimeDlls = @("msvcp140.dll", "msvcp140_1.dll", "msvcp140_2.dll", "vcruntime140.dll", "vcruntime140_1.dll")
foreach ($dll in $VCRuntimeDlls) {
    $DllPath = Join-Path $VCRuntimeDir $dll
    if (-not (Test-Path $DllPath)) {
        Exit-WithError "VC++ Runtime DLL not found: $DllPath"
    }
    Copy-Item -Path $DllPath -Destination $ExeDir -Force
}
Log-Success "VC++ Runtime DLLs copied"

# ═══════════════════════════════════════════════════════════
# 4 Generate Installer
# ═══════════════════════════════════════════════════════════
Print-Section 4 "Generate Installer"

# 4.1 Install InnoSetup if needed
Print-Step "Check InnoSetup"
$UninstallInnoSetup = $false
$InnoSetupDir = Join-Path $SourceDir "tools\InnoSetup"
$ISCCPath = Join-Path $InnoSetupDir "ISCC.exe"
if (-not (Test-Path $ISCCPath)) {
    Log-Info "InnoSetup not found, installing..."
    $Installer = "$env:TEMP\innosetup.exe"
    Invoke-WebRequest -Uri "https://www.jrsoftware.org/download.php/is.exe" -OutFile $Installer
    Start-Process -Wait -FilePath $Installer -ArgumentList "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /DIR=$InnoSetupDir"
    $InstallStatus = $?
    Remove-Item -Path $Installer -Force -ErrorAction SilentlyContinue
    if (-not $InstallStatus) { Exit-WithError "Install InnoSetup failed" }
    $UninstallInnoSetup = $true
    Log-Success "InnoSetup installed"
} else {
    Log-Info "InnoSetup found at $ISCCPath"
}

# 4.2 Generate installer
Print-Step "Build installer"
$TemplateIssPath = Join-Path $SourceDir "package\templates\windows-setup.iss"
$IssPath = Join-Path $BuildDir "PAGViewer.iss"
Copy-Item -Path $TemplateIssPath -Destination $IssPath -Force

$TemplateIslPath = Join-Path $SourceDir "package\templates\windows-ChineseSimplified.isl"
$IslPath = Join-Path $BuildDir "ChineseSimplified.isl"
Copy-Item -Path $TemplateIslPath -Destination $IslPath -Force

(Get-Content $IssPath -Raw -Encoding UTF8) -replace "~PAGViewerVersion~", $AppVersion | Set-Content $IssPath -Encoding UTF8
& $ISCCPath $IssPath
if (-not $?) { Exit-WithError "InnoSetup compilation failed" }

$PAGViewerInstallerPath = Join-Path $BuildDir "PAGViewer_Installer.exe"
if (-not (Test-Path $PAGViewerInstallerPath)) {
    Exit-WithError "Installer not found at $PAGViewerInstallerPath"
}
Log-Success "Installer created: $PAGViewerInstallerPath"

# ═══════════════════════════════════════════════════════════
# 5 Sign & Update Appcast
# ═══════════════════════════════════════════════════════════
Print-Section 5 "Sign & Update Appcast"

if ([string]::IsNullOrEmpty($DSAPrivateKey) -eq $false) {
    Print-Step "Sign installer"
    $SignScript = Join-Path $SourceDir "package\sign_update.bat"
    $Base64Code = cmd /c $SignScript $OpenSSL $PAGViewerInstallerPath $DSAPrivateKey
    if ($LASTEXITCODE -ne 0) { Exit-WithError "Signing installer failed" }
    # Collapse potentially multi-line cmd output into a single token before substitution;
    # PowerShell -replace with a string[] would yield N substitutions and corrupt the XML.
    $Base64Code = (($Base64Code -join '') -replace '\s', '')
    Log-Success "Installer signed"

    Print-Step "Update Appcast"
    $URL = (Invoke-WebRequest -Uri "https://pag.io/server.html" -UseBasicParsing).Content
    if ($IsBetaVersion -eq $true) {
        $URL = $URL + "beta/"
    }
    $URL = $URL + "PAGViewer_Installer.exe"

    $FileLength = (Get-Item $PAGViewerInstallerPath).Length

    $TemplateAppcastPath = Join-Path $SourceDir "package\templates\PagAppcastWindows.xml"
    $AppcastPath = Join-Path $BuildDir "PagAppcastWindows.xml"
    Copy-Item -Path $TemplateAppcastPath -Destination $AppcastPath -Force

    $AppcastContent = Get-Content $AppcastPath -Raw -Encoding UTF8
    $AppcastContent = $AppcastContent -replace "~url~", $URL
    $AppcastContent = $AppcastContent -replace "~date~", $RFCTime
    $AppcastContent = $AppcastContent -replace "~sha1~", $Base64Code
    $AppcastContent = $AppcastContent -replace "~length~", $FileLength
    $AppcastContent = $AppcastContent -replace "~version~", $AppVersion
    Set-Content $AppcastPath -Value $AppcastContent -Encoding UTF8
    Log-Success "Appcast updated"
} else {
    Log-Warning "No DSA private key provided, skipping signing and Appcast update."
}

# ═══════════════════════════════════════════════════════════
# 6 Cleanup
# ═══════════════════════════════════════════════════════════
Print-Section 6 "Cleanup"

if ($UninstallInnoSetup -eq $true) {
    Print-Step "Uninstall InnoSetup"
    Start-Process -Wait -FilePath "$InnoSetupDir\unins000.exe" -ArgumentList "/VERYSILENT /SUPPRESSMSGBOXES /NORESTART"
    Log-Success "InnoSetup uninstalled"
}

# ═══════════════════════════════════════════════════════════
# Summary
# ═══════════════════════════════════════════════════════════
$BuildEndTime = Get-Date
$BuildDuration = $BuildEndTime - $BuildStartTime
$InstallerSize = ""
if (Test-Path $PAGViewerInstallerPath) {
    $InstallerSize = "{0:N1} MB" -f ((Get-Item $PAGViewerInstallerPath).Length / 1MB)
}

$line = [string]::new([char]0x2550, $SECTION_WIDTH)
Write-Host ""
Write-Host $line -ForegroundColor Green
Write-Host "  $([char]0x2714) Build Complete - $($BuildDuration.Minutes)m $($BuildDuration.Seconds)s" -ForegroundColor Green
Write-Host $line -ForegroundColor Green
Log-Info "  Version:    $AppVersion"
Log-Info "  Installer:  $PAGViewerInstallerPath ($InstallerSize)"
Log-Info "  PDB:        $BuildDir\PAGViewer.pdb"
Log-Info "              $BuildDir\PAGExporter.pdb"
Log-Info "  Signed:     $(if ($DSAPrivateKey) { 'Yes' } else { 'No' })"
Write-Host ""
