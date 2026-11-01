param(
    [string]$VulkanSdk = 'B:\Environment\VulkanSDK\1.4.350.0',
    [switch]$ConfigureOnly,
    [switch]$InteractionTests
)
$ErrorActionPreference = 'Stop'
$project = Split-Path $PSScriptRoot -Parent
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ Build Tools are required.' }
Import-Module (Join-Path $vs 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll')
Enter-VsDevShell -VsInstallPath $vs -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64' | Out-Null
$cmake = Join-Path $vs 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
if (!(Test-Path -LiteralPath (Join-Path $VulkanSdk 'Bin\glslc.exe'))) {
    throw "Vulkan SDK missing at $VulkanSdk. Install it under B:\Environment or E:\Environment; do not fall back to CPU."
}
$env:VULKAN_SDK = $VulkanSdk
$env:Path = (Join-Path $VulkanSdk 'Bin') + ';' + $env:Path
$build = Join-Path $project 'build'
$configure = @('-S', $project, '-B', $build, '-G', 'Visual Studio 17 2022', '-A', 'x64',
    '-DMUSCRIPTOR_VULKAN=ON', '-DNEURALNOTE_REQUIRE_GPU=ON', '-DNEURALNOTE_ASIO=OFF',
    '-DLTO=OFF', '-DNEURALNOTE_SYMBOLS=OFF', '-DNEURALNOTE_LOCALISATION_TESTS=ON',
    '-DPython3_EXECUTABLE=E:/Environment/Python311/python.exe')
if (Test-Path -LiteralPath 'B:\Environment\NeuralNote\ggml\CMakeLists.txt') {
    $configure += '-DFETCHCONTENT_SOURCE_DIR_GGML=B:/Environment/NeuralNote/ggml'
}
if ($InteractionTests) { $configure += '-DNEURALNOTE_INTERACTION_TESTS=ON' }
& $cmake @configure
if ($LASTEXITCODE) { throw "GPU configure failed: $LASTEXITCODE" }
& 'E:\Environment\Python311\python.exe' (Join-Path $project 'Tools\check_localisation.py')
if ($LASTEXITCODE) { throw 'Localisation coverage check failed.' }
if ($ConfigureOnly) { return }
& $cmake --build $build --config Release --target NeuralNote_Standalone LocalisationTests --parallel 6
if ($LASTEXITCODE) { throw "GPU build failed: $LASTEXITCODE" }
& (Join-Path $build 'LocalisationTests_artefacts\Release\LocalisationTests.exe')
if ($LASTEXITCODE) { throw 'Localisation tests failed.' }
if ($InteractionTests) {
    & $cmake --build $build --config Release --target InteractionTests --parallel 6
    if ($LASTEXITCODE) { throw 'Interaction test build failed.' }
    & (Join-Path $build 'InteractionTests_artefacts\Release\InteractionTests.exe')
    if ($LASTEXITCODE) { throw 'Interaction tests failed.' }
}
