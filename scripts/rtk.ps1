param(
  [string]$Command = "run"
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildDir = if ($env:RTK_BUILD_DIR) { $env:RTK_BUILD_DIR } else { Join-Path $Root "build" }
$Config = if ($env:RTK_CONFIG) { $env:RTK_CONFIG } else { "Release" }
$PreviewArgs = @($args)
$ToolsDir = Join-Path $Root ".rtk\tools"
$CMakeVersion = if ($env:RTK_CMAKE_VERSION) { $env:RTK_CMAKE_VERSION } else { "3.29.8" }
$script:CMakeExe = $null
$script:CTestExe = $null

function Write-Step($Message) {
  Write-Host "[rtk] $Message"
}

function Assert-NativeSuccess($Action) {
  if ($LASTEXITCODE -ne 0) {
    throw "$Action failed with exit code $LASTEXITCODE. If the preview app is open, close it and rerun the command."
  }
}

function Find-Tool($Name) {
  $tool = Get-Command $Name -ErrorAction SilentlyContinue
  if ($tool) {
    return $tool.Source
  }
  return $null
}

function Get-CMake {
  $fromPath = Find-Tool "cmake"
  if ($fromPath) {
    return $fromPath
  }

  $portable = Join-Path $ToolsDir "cmake-$CMakeVersion-windows-x86_64\bin\cmake.exe"
  if (Test-Path $portable) {
    return $portable
  }

  Write-Step "cmake not found; downloading portable CMake $CMakeVersion"
  New-Item -ItemType Directory -Force -Path $ToolsDir | Out-Null

  $zip = Join-Path $ToolsDir "cmake-$CMakeVersion-windows-x86_64.zip"
  $url = "https://github.com/Kitware/CMake/releases/download/v$CMakeVersion/cmake-$CMakeVersion-windows-x86_64.zip"
  Invoke-WebRequest -Uri $url -OutFile $zip
  Expand-Archive -Path $zip -DestinationPath $ToolsDir -Force

  if (-not (Test-Path $portable)) {
    throw "Portable CMake download did not produce $portable"
  }
  return $portable
}

function Get-CTest {
  if (-not $script:CMakeExe) {
    $script:CMakeExe = Get-CMake
  }

  $besideCMake = Join-Path (Split-Path $script:CMakeExe -Parent) "ctest.exe"
  if (Test-Path $besideCMake) {
    return $besideCMake
  }

  $fromPath = Find-Tool "ctest"
  if ($fromPath) {
    return $fromPath
  }

  throw "ctest was not found beside cmake or on PATH."
}

function Find-VisualStudioGenerator {
  $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
  if (-not (Test-Path $vswhere)) {
    return $null
  }

  $version = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationVersion
  if (-not $version) {
    return $null
  }

  if ($version.StartsWith("17.")) {
    return "Visual Studio 17 2022"
  }
  if ($version.StartsWith("16.")) {
    return "Visual Studio 16 2019"
  }
  if ($version.StartsWith("15.")) {
    return "Visual Studio 15 2017"
  }
  return $null
}

function Test-Dependencies {
  Write-Step "checking dependencies"
  $script:CMakeExe = Get-CMake
  $script:CTestExe = Get-CTest

  $cmakeVersion = (& $script:CMakeExe --version | Select-Object -First 1)
  Write-Step $cmakeVersion

  $ninja = Get-Command "ninja" -ErrorAction SilentlyContinue
  if ($ninja) {
    Write-Step "ninja found at $($ninja.Source)"
  } else {
    Write-Step "ninja not found; CMake will use the platform default generator"
  }

  $compilerNames = @("cl", "clang++", "g++")
  $compiler = $compilerNames | ForEach-Object { Get-Command $_ -ErrorAction SilentlyContinue } | Select-Object -First 1
  if ($compiler) {
    Write-Step "compiler candidate found at $($compiler.Source)"
  } elseif (Find-VisualStudioGenerator) {
    Write-Step "Visual Studio generator available"
  } else {
    Write-Step "no compiler found directly on PATH; CMake may still find a Visual Studio toolchain"
  }
}

function Configure-Project {
  if (Test-Path (Join-Path $BuildDir "CMakeCache.txt")) {
    return
  }

  Write-Step "configuring build in $BuildDir"
  $configure = @("-S", $Root, "-B", $BuildDir, "-DCMAKE_BUILD_TYPE=$Config")
  if (Get-Command "ninja" -ErrorAction SilentlyContinue) {
    $configure += @("-G", "Ninja")
  } else {
    $vsGenerator = Find-VisualStudioGenerator
    if ($vsGenerator) {
      $configure += @("-G", $vsGenerator, "-A", "x64")
    }
  }
  & $script:CMakeExe @configure
  Assert-NativeSuccess "CMake configure"
}

function Build-Project {
  Test-Dependencies
  Configure-Project
  Write-Step "building latest sources"
  & $script:CMakeExe --build $BuildDir --config $Config
  Assert-NativeSuccess "CMake build"
}

function Find-PreviewExecutable([bool]$PreferGui) {
  $names = if ($PreferGui) {
    @("rtk_preview_gui.exe", "rtk_preview_gui", "rtk_preview_cli.exe", "rtk_preview_cli")
  } else {
    @("rtk_preview_cli.exe", "rtk_preview_cli")
  }

  foreach ($name in $names) {
    $candidate = Get-ChildItem -Path $BuildDir -Recurse -File -Filter $name -ErrorAction SilentlyContinue |
      Sort-Object LastWriteTimeUtc -Descending |
      Select-Object -First 1

    if ($candidate) {
      return $candidate.FullName
    }
  }

  throw "Preview harness was not found under $BuildDir after build."
}

function Find-PreviewCliExecutable {
  $names = @("rtk_preview_cli.exe", "rtk_preview_cli")
  $candidate = Get-ChildItem -Path $BuildDir -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { $names -contains $_.Name } |
    Sort-Object LastWriteTimeUtc -Descending |
    Select-Object -First 1

  if (-not $candidate) {
    throw "Preview harness was not found under $BuildDir after build."
  }
  return $candidate.FullName
}

function Run-Preview {
  Build-Project
  $preferGui = $PreviewArgs.Count -eq 0
  $exe = Find-PreviewExecutable $preferGui
  Write-Step "running $exe $($PreviewArgs -join ' ')"
  & $exe @PreviewArgs
  Assert-NativeSuccess "Preview run"
}

function Run-GuiPreview {
  Build-Project
  $exe = Find-PreviewExecutable $true
  Write-Step "running $exe $($PreviewArgs -join ' ')"
  & $exe @PreviewArgs
  Assert-NativeSuccess "GUI preview run"
}

function Run-Benchmark {
  Build-Project
  $exe = Find-PreviewExecutable $true
  $benchmarkOut = Join-Path $Root "out\benchmark.txt"
  $benchArgs = @("--benchmark", "--benchmark-out", $benchmarkOut) + $PreviewArgs
  Write-Step "running benchmark $exe $($benchArgs -join ' ')"
  $process = Start-Process -FilePath $exe -ArgumentList $benchArgs -Wait -PassThru
  if ($process.ExitCode -ne 0) {
    throw "Benchmark failed with exit code $($process.ExitCode)"
  }
  if (Test-Path $benchmarkOut) {
    Get-Content $benchmarkOut
  } else {
    throw "Benchmark did not write $benchmarkOut"
  }
}

function Run-Tests {
  Build-Project
  Write-Step "running tests"
  & $script:CTestExe --test-dir $BuildDir --build-config $Config --output-on-failure
  Assert-NativeSuccess "CTest"
}

function Clean-Build {
  $resolvedRoot = [System.IO.Path]::GetFullPath($Root)
  $resolvedBuild = [System.IO.Path]::GetFullPath($BuildDir)
  if (-not $resolvedBuild.StartsWith($resolvedRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to remove build dir outside repo: $resolvedBuild"
  }
  if (Test-Path $resolvedBuild) {
    Write-Step "removing $resolvedBuild"
    Remove-Item -LiteralPath $resolvedBuild -Recurse -Force
  }
}

switch ($Command.ToLowerInvariant()) {
  "check" { Test-Dependencies }
  "build" { Build-Project }
  "test" { Run-Tests }
  "run" { Run-Preview }
  "gui" { Run-GuiPreview }
  "bench" { Run-Benchmark }
  "clean" { Clean-Build }
  default {
    Write-Host "Usage: scripts/rtk.ps1 [check|build|test|run|gui|bench|clean] [preview args...]"
    exit 2
  }
}
