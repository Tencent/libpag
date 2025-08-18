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


# 1 初始化变量
Print-Text "[ 初始化变量 ]"

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

# Add your Path here
$Deployqt = "C:\Qt\6.8.1\msvc2022_64\bin\windeployqt.exe"
$OpenSSL = "C:\msys64\usr\bin\openssl.exe"


# 2 编译
Print-Text "[ 编译 ]"
$x64BuildDir = Join-Path $BuildDir "x64"

# 2.1 设置环境变量
Print-Text "[ 设置环境变量 ]"
$VSPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
$VSDevEnv = Join-Path $VSPath -ChildPath "Common7" | Join-Path -ChildPath "IDE" | Join-Path -ChildPath "devenv.exe"
$WindowsSdkDir = [System.IO.Path]::Combine(${env:ProgramFiles(x86)}, "Windows Kits", "10")
$LatestSdkVersion = (Get-ChildItem (Join-Path $WindowsSdkDir "Lib") | Sort-Object Name -Descending | Select-Object -First 1).Name
$env:LIB = "${WindowsSdkDir}\Lib\${LatestSdkVersion}\um\x64;${WindowsSdkDir}\Lib\${LatestSdkVersion}\ucrt\x64;" + $env:LIB

# 2.2 编译PAGViewer
Print-Text "[ 编译PAGViewer ]"
cmake -S $SourceDir -B $x64BuildDir -DCMAKE_BUILD_TYPE=Release
if (-not $?) {
    Write-Host "构建PAGViewer x64失败" -ForegroundColor Red
    exit 1
}

cmake --build $x64BuildDir --config Release -j 16
if (-not $?) {
    Write-Host "编译PAGViewer x64失败" -ForegroundColor Red
    exit 1
}

# 2.3 编译H264编码工具
Print-Text "[ 编译H264编码工具 ]"
$EncoderToolsSourceDir = Join-Path $SourceDir -ChildPath "third_party" | Join-Path -ChildPath "H264EncoderTools"
$EncoderToolsSlnPath = Join-Path $EncoderToolsSourceDir -ChildPath "H264EncoderTools.sln"
& $VSDevEnv $EncoderToolsSlnPath /Rebuild "Release|x64"
if (-not $?) {
    Write-Host "构建H264编码工具 x64失败" -ForegroundColor Red
    exit 1
}

# 2.4 编译AE导出插件
Print-Text "[ 编译AE导出插件 ]"
$PluginSourceDir = Join-Path $LibpagDir -ChildPath "exporter"
$x64BuildDirForPlugin = Join-Path $x64BuildDir -ChildPath "Plugin"
cmake -S $PluginSourceDir -B $x64BuildDirForPlugin -DCMAKE_BUILD_TYPE=Release
if (-not $?) {
    Write-Host "构建AE导出插件 x64失败" -ForegroundColor Red
    exit 1
}

cmake --build $x64BuildDirForPlugin --config Release -j 16
if (-not $?) {
    Write-Host "构建AE导出插件 x64失败" -ForegroundColor Red
    exit 1
}

# 3 资源整理
Print-Text "[ 资源整理 ]"
# 3.1 复制文件
Print-Text "[ 复制文件 ]"
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

# 3.2 复制AE导出插件和工具
Print-Text "[ 复制AE导出插件和工具 ]"
$GeneratedPluginPath = Join-Path $x64BuildDirForPlugin -ChildPath "Release" | Join-Path -ChildPath "PAGExporter.aex"
Copy-Item -Path $GeneratedPluginPath -Destination $ExeDir -Force

$LibFfaudioPath = Join-Path $PluginSourceDir -ChildPath "vendor" | Join-Path -ChildPath "ffaudio" | Join-Path -ChildPath "win" | Join-Path -ChildPath "x64" | Join-Path -ChildPath "ffaudio.dll"
Copy-Item -Path $LibFfaudioPath -Destination $ExeDir -Force

$GeneratedEncorderToolsPath = Join-Path $EncoderToolsSourceDir -ChildPath "x64" | Join-Path -ChildPath "Release" | Join-Path -ChildPath "H264EncoderTools.exe"
Copy-Item -Path $GeneratedEncorderToolsPath -Destination $ExeDir -Force

# 3.3 获取PAGViewer的依赖
Print-Text "[ 获取PAGViewer的依赖 ]"
$QmlDir = Join-Path $SourceDir "qml"
& $Deployqt $ExePath --qmldir=$QmlDir

$WinSparklePath = Join-Path $SourceDir "vendor" | 
                  Join-Path -ChildPath "winsparkle" | 
                  Join-Path -ChildPath "win" |
                  Join-Path -ChildPath "x64" | 
                  Join-Path -ChildPath "Release" | 
                  Join-Path -ChildPath "WinSparkle.dll"
Copy-Item -Path $WinSparklePath -Destination $ExeDir -Force

# 3.3 安装工具
# 3.3.1 安装InnoSetup
Print-Text "[ 安装Inno-Setup工具 ]"
$UninstallInnoSetup = $false
$InnoSetupDir = Join-Path $SourceDir "tools" | 
                Join-Path -ChildPath "InnoSetup"
$ISCCPath = Join-Path $InnoSetupDir "ISCC.exe"
if (-not (Test-Path $ISCCPath)) {
    winget install --id JRSoftware.InnoSetup -e -s winget -h -l $InnoSetupDir
    if (-not $?) {
        Write-Host "安装InnoSetup失败" -ForegroundColor Red
        exit 1
    }
    $UninstallInnoSetup = $true
}


# 4 生成Installer
Print-Text "[ 生成Installer ]"
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
    Write-Host "生成PAGViewer installer失败" -ForegroundColor Red
    exit 1
}

# 5 签名
Print-Text "[ 签名PAGViewer ]"

# 5.1 获取私钥文件
Print-Text "[ 获取私钥文件 ]"
if ([string]::IsNullOrEmpty($DSAPrivateKey) -eq $false) {
    # 5.2 签名
    Print-Text "[ 签名PAGViewer ]"
    $SignScript = Join-Path $SourceDir "package" |
            Join-Path -ChildPath "sign_update.bat"

    $Base64Code = cmd /c $SignScript $OpenSSL $PAGViewerInstallerPath $DSAPrivateKey

    # 5.3 更新Appcast
    Print-Text "[ 更新Appcast ]"
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


# 6 卸载InnoSetup
if ($UninstallInnoSetup -eq $true) {
    Print-Text "[ 卸载InnoSetup ]"
    winget uninstall --id JRSoftware.InnoSetup
}