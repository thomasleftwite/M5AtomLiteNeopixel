param(
    [Parameter(Mandatory)][string]$PlatformJar,
    [Parameter(Mandatory)][string]$BuildTools,
    [Parameter(Mandatory)][string]$JavaHome,
    [Parameter(Mandatory)][string]$Keystore,
    [Parameter(Mandatory)][string]$Output,
    [string]$KeyAlias = 'atomneopixel'
)

$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
if (!$env:ATOM_APK_STORE_PASSWORD) { throw 'Set ATOM_APK_STORE_PASSWORD for signing.' }
$PlatformJar = (Resolve-Path -LiteralPath $PlatformJar).Path
$BuildTools = (Resolve-Path -LiteralPath $BuildTools).Path
$JavaHome = (Resolve-Path -LiteralPath $JavaHome).Path
$Keystore = (Resolve-Path -LiteralPath $Keystore).Path
$Output = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($Output)
if (Test-Path -LiteralPath $Output) { throw 'The output APK already exists.' }

function Invoke-BuildTool([string]$Tool, [string[]]$Arguments) {
    & $Tool @Arguments
    if ($LASTEXITCODE) { throw "$Tool failed with exit code $LASTEXITCODE." }
}

$tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
$build = Join-Path $tempRoot ('AtomNeoPixel-apk-' + [guid]::NewGuid())
$previousJavaHome = $env:JAVA_HOME
try {
    $env:JAVA_HOME = $JavaHome
    foreach ($folder in 'assets', 'classes', 'dex', 'generated') {
        New-Item -ItemType Directory -Path (Join-Path $build $folder) -Force | Out-Null
    }
    foreach ($item in 'AndroidManifest.xml', 'src', 'res') {
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot $item) -Destination $build -Recurse
    }
    $root = Split-Path -Parent $PSScriptRoot
    Invoke-BuildTool 'node' @((Join-Path $root 'tools/serve-ui.mjs'), '--export', (Join-Path $build 'assets/index.html'))
    Push-Location -LiteralPath $build
    try {
        Invoke-BuildTool (Join-Path $BuildTools 'aapt2.exe') @('compile', '--dir', 'res', '-o', 'resources.zip')
        Invoke-BuildTool (Join-Path $BuildTools 'aapt2.exe') @(
            'link', '-I', $PlatformJar, '--manifest', 'AndroidManifest.xml', '--java', 'generated',
            '-A', 'assets', '-o', 'unsigned.apk', 'resources.zip'
        )
        $sources = Get-ChildItem -LiteralPath 'src', 'generated' -Recurse -Filter '*.java' |
            Select-Object -ExpandProperty FullName
        Invoke-BuildTool (Join-Path $JavaHome 'bin/javac.exe') (
            @('--release', '8', '-encoding', 'UTF-8', '-cp', $PlatformJar, '-d', 'classes') + $sources
        )
        Invoke-BuildTool (Join-Path $JavaHome 'bin/jar.exe') @('cf', 'classes.jar', '-C', 'classes', '.')
        Invoke-BuildTool (Join-Path $BuildTools 'd8.bat') @(
            '--lib', $PlatformJar, '--min-api', '26', '--output', 'dex', 'classes.jar'
        )
        Move-Item -LiteralPath 'dex/classes.dex' -Destination 'classes.dex'
        Invoke-BuildTool (Join-Path $BuildTools 'aapt.exe') @('add', 'unsigned.apk', 'classes.dex')
        Invoke-BuildTool (Join-Path $BuildTools 'zipalign.exe') @('-f', '4', 'unsigned.apk', 'aligned.apk')
        Invoke-BuildTool (Join-Path $BuildTools 'apksigner.bat') @(
            'sign', '--ks', $Keystore, '--ks-key-alias', $KeyAlias, '--ks-pass', 'env:ATOM_APK_STORE_PASSWORD',
            '--out', 'signed.apk', 'aligned.apk'
        )
        Invoke-BuildTool (Join-Path $BuildTools 'apksigner.bat') @('verify', '--verbose', 'signed.apk')
        Invoke-BuildTool (Join-Path $BuildTools 'zipalign.exe') @('-c', '4', 'signed.apk')
        [IO.File]::Copy((Join-Path $build 'signed.apk'), $Output)
    } finally {
        Pop-Location
    }
} finally {
    $env:JAVA_HOME = $previousJavaHome
    $absoluteBuild = [IO.Path]::GetFullPath($build)
    if ((Split-Path -Parent $absoluteBuild).TrimEnd('\', '/') -eq $tempRoot.TrimEnd('\', '/') -and
        (Split-Path -Leaf $absoluteBuild) -like 'AtomNeoPixel-apk-*' -and (Test-Path -LiteralPath $absoluteBuild)) {
        Remove-Item -LiteralPath $absoluteBuild -Recurse -Force
    }
}
