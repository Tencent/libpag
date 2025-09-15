$OutputEncoding = [System.Text.Encoding]::UTF8
[Console]::InputEncoding = [System.Text.Encoding]::UTF8
[Console]::OutputEncoding = [System.Text.UTF8Encoding]::new($false)

$PSDefaultParameterValues['*:Encoding'] = 'utf8'
$PSDefaultParameterValues['Out-File:Encoding'] = 'utf8'
$PSDefaultParameterValues['Export-Csv:Encoding'] = 'utf8'
$PSDefaultParameterValues['Set-Content:Encoding'] = 'utf8'

function Fix-ConsoleEncoding {
    if ($Host.Name -eq 'ConsoleHost') {
        $console = [Console]::OutputEncoding
        [Console]::OutputEncoding = [System.Text.Encoding]::GetEncoding(936)
        [Console]::OutputEncoding = $console
    }
}
Fix-ConsoleEncoding

function Print-Text {
    param (
        [string]$text,
        [int]$width = 40
    )
    
    $text = [System.Text.Encoding]::UTF8.GetString(
        [System.Text.Encoding]::GetEncoding(936).GetBytes($text)
    )
    
    $textLength = $text.Length
    $paddingLength = [math]::Max(0, [math]::Floor(($width - $textLength) / 2))
    $padding = '=' * $paddingLength
    
    [Console]::ForegroundColor = [ConsoleColor]::Green
    [Console]::WriteLine("${padding}${text}${padding}")
    [Console]::ResetColor()
}


# 1 Initialize variables
Print-Text "[ Initialize variables ]"

$majorVersion = if ($env:MajorVersion) { $env:MajorVersion } else { "1" }
$minorVersion = if ($env:MinorVersion) { $env:MinorVersion } else { "0" }
$buildNO = if ($env:BK_CI_BUILD_NO) { $env:BK_CI_BUILD_NO } else { "0" }

$AppVersion = "${majorVersion}.${minorVersion}.${buildNO}"
$CurrentTime = Get-Date -Format "yyyyMMddHHmmss"
$RFCTime = Get-Date -Format "r"
$SourceDir = Split-Path $PSScriptRoot -Parent
$LibpagDir = Split-Path $SourceDir -Parent
$BuildDir = Join-Path $SourceDir "build_$CurrentTime"
$IsBetaVersion = if ($env:isBetaVersion) { $env:isBetaVersion } else { false }

if ([string]::IsNullOrEmpty($env:PAG_DeployQt_Path) -or [string]::IsNullOrEmpty($env:PAG_Qt_Path) -or [string]::IsNullOrEmpty($env:PAG_AE_SDK_Path) -or [string]::IsNullOrEmpty($env:PAG_Openssl_Path)) {
    Write-Host "Please set [PAG_DeployQt_Path], [PAG_Qt_Path], [PAG_AE_SDK_Path] and [PAG_Openssl_Path] before build on Windows" -ForegroundColor Red
    exit 1
}

$Deployqt = $env:PAG_DeployQt_Path
$OpenSSL = $env:PAG_Openssl_Path
$QtPath = $env:PAG_Qt_Path
$QtCMakePath = Join-Path $QtPath -ChildPath "lib" | Join-Path -ChildPath "cmake"
$AESDKPath = $env:PAG_AE_SDK_Path


# 2 Compile
Print-Text "[ Compile ]"
$x64BuildDir = Join-Path $BuildDir "x64"

# 2.1 Set environment variables
Print-Text "[ Set environment variables ]"
$VSPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
$VSDevEnv = Join-Path $VSPath -ChildPath "Common7" | Join-Path -ChildPath "IDE" | Join-Path -ChildPath "devenv.exe"
$WindowsSdkDir = [System.IO.Path]::Combine(${env:ProgramFiles(x86)}, "Windows Kits", "10")
$LatestSdkVersion = (Get-ChildItem (Join-Path $WindowsSdkDir "Lib") | Sort-Object Name -Descending | Select-Object -First 1).Name
$env:LIB = "${WindowsSdkDir}\Lib\${LatestSdkVersion}\um\x64;${WindowsSdkDir}\Lib\${LatestSdkVersion}\ucrt\x64;" + $env:LIB

# 2.2 Compile PAGViewer
Print-Text "[ Compile PAGViewer ]"
cmake -S $SourceDir -B $x64BuildDir -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QtCMakePath"
if (-not $?) {
    Write-Host "Build PAGViewer-x64 failed" -ForegroundColor Red
    exit 1
}

cmake --build $x64BuildDir --config Release -j 16
if (-not $?) {
    Write-Host "Compile PAGViewer-x64 failed" -ForegroundColor Red
    exit 1
}

# 2.3 Compile H264EncoderTools
Print-Text "[ Compile H264EncoderTools ]"
$EncoderToolsSourceDir = Join-Path $SourceDir -ChildPath "third_party" | Join-Path -ChildPath "H264EncoderTools"
$EncoderToolsSlnPath = Join-Path $EncoderToolsSourceDir -ChildPath "H264EncoderTools.sln"
& $VSDevEnv $EncoderToolsSlnPath /Rebuild "Release|x64"
if (-not $?) {
    Write-Host "Build H264EncoderTools-x64 failed" -ForegroundColor Red
    exit 1
}

# 2.4 Compile PAGExporter
Print-Text "[ Compile PAGExporter ]"
$PluginSourceDir = Join-Path $LibpagDir -ChildPath "exporter"
$x64BuildDirForPlugin = Join-Path $x64BuildDir -ChildPath "Plugin"
cmake -S $PluginSourceDir -B $x64BuildDirForPlugin -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$QtCMakePath" -DAE_SDK_PATH="$AESDKPath"
if (-not $?) {
    Write-Host "Build PAGExporter-x64 failed" -ForegroundColor Red
    exit 1
}

cmake --build $x64BuildDirForPlugin --config Release -j 16
if (-not $?) {
    Write-Host "Compile PAGExporter-x64 failed" -ForegroundColor Red
    exit 1
}

# 3 Organize resources
Print-Text "[ Organize resources ]"
# 3.1 Copy files
Print-Text "[ Copy files ]"
$TemplatePackageDir = Join-Path $SourceDir -ChildPath "package" | 
                      Join-Path -ChildPath "templates" | 
                      Join-Path -ChildPath "windows-packages"
$PackageDir = Join-Path $BuildDir "com.tencent.pagplayer"
Copy-Item -Path $TemplatePackageDir -Destination $PackageDir -Recurse -Force

$GeneratedExePath = Join-Path $x64BuildDir "Release" | 
                    Join-Path -ChildPath "PAGViewer.exe"
$ExeDir = Join-Path $PackageDir "data"
$ExePath = Join-Path $ExeDir "PAGViewer.exe"
Copy-Item -Path $GeneratedExePath -Destination $ExePath -Force

# 3.2 Copy PAGExporter and tools
Print-Text "[ Copy PAGExporter and tools ]"
$GeneratedPluginPath = Join-Path $x64BuildDirForPlugin -ChildPath "Release" | Join-Path -ChildPath "PAGExporter.aex"
Copy-Item -Path $GeneratedPluginPath -Destination $ExeDir -Force

$LibFfaudioPath = Join-Path $PluginSourceDir -ChildPath "vendor" | Join-Path -ChildPath "ffaudio" | Join-Path -ChildPath "win" | Join-Path -ChildPath "x64" | Join-Path -ChildPath "ffaudio.dll"
Copy-Item -Path $LibFfaudioPath -Destination $ExeDir -Force

$GeneratedEncorderToolsPath = Join-Path $EncoderToolsSourceDir -ChildPath "x64" | Join-Path -ChildPath "Release" | Join-Path -ChildPath "H264EncoderTools.exe"
Copy-Item -Path $GeneratedEncorderToolsPath -Destination $ExeDir -Force

# 3.3 Obtain the dependencies of PAGViewer
Print-Text "[ Obtain the dependencies of PAGViewer ]"
$QmlDir = Join-Path $SourceDir "qml"
& $Deployqt $ExePath --qmldir=$QmlDir

$WinSparklePath = Join-Path $SourceDir "vendor" | 
                  Join-Path -ChildPath "winsparkle" | 
                  Join-Path -ChildPath "win" |
                  Join-Path -ChildPath "x64" | 
                  Join-Path -ChildPath "Release" | 
                  Join-Path -ChildPath "WinSparkle.dll"
Copy-Item -Path $WinSparklePath -Destination $ExeDir -Force

# 3.3 Install tool
# 3.3.1 Install InnoSetup
Print-Text "[ Install InnoSetup ]"
$UninstallInnoSetup = $false
$InnoSetupDir = Join-Path $SourceDir "tools" | 
                Join-Path -ChildPath "InnoSetup"
$ISCCPath = Join-Path $InnoSetupDir "ISCC.exe"
if (-not (Test-Path $ISCCPath)) {
    winget install --id JRSoftware.InnoSetup -e -s winget -h -l $InnoSetupDir
    if (-not $?) {
        Write-Host "Install InnoSetup failed" -ForegroundColor Red
        exit 1
    }
    $UninstallInnoSetup = $true
}


# 4 Generate Installer
Print-Text "[ Generate Installer ]"
$TemplateIssPath = Join-Path $SourceDir "package" | 
                   Join-Path -ChildPath "templates" | 
                   Join-Path -ChildPath "windows-setup.iss"
$IssPath = Join-Path $BuildDir "PAGViewer.iss"
Copy-Item -Path $TemplateIssPath -Destination $IssPath -Force

$TemplateIslPath = Join-Path $SourceDir "package" |
                   Join-Path -ChildPath "templates" | 
                   Join-Path -ChildPath "windows-ChineseSimplified.isl"
$IslPath = Join-Path $BuildDir "ChineseSimplified.isl"
Copy-Item -Path $TemplateIslPath -Destination $IslPath -Force

(Get-Content $IssPath -Raw -Encoding UTF8) -replace "~PAGViewerVersion~", $AppVersion | Set-Content $IssPath -Encoding UTF8
& $ISCCPath $IssPath

$PAGViewerInstallerPath = Join-Path $BuildDir "PAGViewer_installer.exe"
if (-not (Test-Path $PAGViewerInstallerPath)) {
    Write-Host "Generate PAGViewer installer failed" -ForegroundColor Red
    exit 1
}

# 5 Sign
Print-Text "[ Sign PAGViewer ]"

# 5.1 Obtain private key
Print-Text "[ Obtain private key ]"
if ([string]::IsNullOrEmpty($DSAPrivateKey) -eq $false) {
    # 5.2 Sign
    Print-Text "[ Sign PAGViewer ]"
    $SignScript = Join-Path $SourceDir "package" |
            Join-Path -ChildPath "sign_update.bat"

    $Base64Code = cmd /c $SignScript $OpenSSL $PAGViewerInstallerPath $DSAPrivateKey

    # 5.3 Update Appcast
    Print-Text "[ Update Appcast ]"
    $URL = (Invoke-WebRequest -Uri "https://pag.qq.com/server.html").Content
    if ($IsBetaVersion -eq $true) {
        $URL = $URL + "beta/"
    }
    $URL = $URL + "PAGViewer_installer.exe"

    $FileLength = (Get-Item $PAGViewerInstallerPath).Length

    $TemplateAppcastPath = Join-Path $SourceDir "package" |
            Join-Path -ChildPath "templates" |
            Join-Path -ChildPath "PagAppcastWindows.xml"
    $AppcastPath = Join-Path $BuildDir "PagAppcastWindows.xml"
    Copy-Item -Path $TemplateAppcastPath -Destination $AppcastPath -Force

    (Get-Content $AppcastPath -Raw -Encoding UTF8) -replace "~url~", $URL | Set-Content $AppcastPath -Encoding UTF8
    (Get-Content $AppcastPath -Raw -Encoding UTF8) -replace "~date~", $RFCTime | Set-Content $AppcastPath -Encoding UTF8
    (Get-Content $AppcastPath -Raw -Encoding UTF8) -replace "~sha1~", $Base64Code | Set-Content $AppcastPath -Encoding UTF8
    (Get-Content $AppcastPath -Raw -Encoding UTF8) -replace "~length~", $FileLength | Set-Content $AppcastPath -Encoding UTF8
    (Get-Content $AppcastPath -Raw -Encoding UTF8) -replace "~version~", $AppVersion | Set-Content $AppcastPath -Encoding UTF8
}


# 6 Uninstall InnoSetup
if ($UninstallInnoSetup -eq $true) {
    Print-Text "[ Uninstall InnoSetup ]"
    winget uninstall --id JRSoftware.InnoSetup
}