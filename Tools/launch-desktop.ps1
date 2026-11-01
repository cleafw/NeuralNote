$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$exe = Join-Path $project 'build\NeuralNote_artefacts\Release\Standalone\NeuralNote.exe'
if (!(Test-Path -LiteralPath $exe)) {
    Add-Type -AssemblyName PresentationFramework
    [System.Windows.MessageBox]::Show('NeuralNote has not been built. Build the Standalone Release target first.', 'NeuralNote') | Out-Null
    exit 1
}
Start-Process -FilePath $exe -WorkingDirectory (Split-Path -Parent $exe)
