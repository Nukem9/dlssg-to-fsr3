# Set up powershell equivalent of vcvarsall.bat
$vsPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationpath

Import-Module (Get-ChildItem $vsPath -Recurse -File -Filter Microsoft.VisualStudio.DevShell.dll).FullName
Enter-VsDevShell -VsInstallPath $vsPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64'

# Then build with VS
& cmake --preset final-dtwrapper
& cmake --build --preset final-dtwrapper-release
& cpack --preset final-dtwrapper

& cmake --preset final-nvngxwrapper
& cmake --build --preset final-nvngxwrapper-release
& cpack --preset final-nvngxwrapper

& cmake --preset final-universal
& cmake --build --preset final-universal-release
& cpack --preset final-universal