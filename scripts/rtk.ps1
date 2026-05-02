param(
  [string]$Command = "run"
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$BuildDir = if ($env:RTK_BUILD_DIR) { $env:RTK_BUILD_DIR } else { Join-Path $Root "build" }
$Config = if ($env:RTK_CONFIG) { $env:RTK_CONFIG } else { "Release" }
$PreviewArgs = $args

function Write-Step($Message) {
  Write-Host "[rtk] $Message"
}

function Require-Tool($Name) {
  $tool = Get-Command $Name -ErrorAction SilentlyContinue
  if (-not $tool) {
    throw "Missing required tool '$Name'. Install it and make sure it is on PATH."
  }
  return $tool
}

function Test-Dependencies {
  Write-Step "checking dependencies"
  Require-Tool "cmake" | Out-Null

  $cmakeVersion = (& cmake --version | Select-Object -First 1)
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
  }
  & cmake @configure
}

function Build-Project {
  Test-Dependencies
  Configure-Project
  Write-Step "building latest sources"
  & cmake --build $BuildDir --config $Config
}

function Find-PreviewExecutable {
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
  $exe = Find-PreviewExecutable
  if ($PreviewArgs.Count -eq 0) {
    $PreviewArgs = @("--out", (Join-Path $Root "out/preview.ppm"))
  }
  Write-Step "running $exe $($PreviewArgs -join ' ')"
  & $exe @PreviewArgs
}

function Run-Tests {
  Build-Project
  Write-Step "running tests"
  & ctest --test-dir $BuildDir --build-config $Config --output-on-failure
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
  "clean" { Clean-Build }
  default {
    Write-Host "Usage: scripts/rtk.ps1 [check|build|test|run|clean] [preview args...]"
    exit 2
  }
}
