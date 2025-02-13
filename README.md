**dlssg-to-fsr3** is a drop-in mod/replacement for games utilizing [Nvidia's DLSS-G Frame Generation](https://nvidianews.nvidia.com/news/nvidia-introduces-dlss-3-with-breakthrough-ai-powered-frame-generation-for-up-to-4x-performance) technology that allows people to use [AMD's FSR 3 Frame Generation](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK) technology instead. Only RTX 1600, RTX 2000, and RTX 3000 series GPUs are supported.

Game-specific compatibility can be [found here](https://github.com/Nukem9/dlssg-to-fsr3/wiki/Game-Compatibility-List). Using dlssg-to-fsr3 in multiplayer games is ill advised and may lead to account bans. **Use at your own risk.**

## Download Link

[https://www.nexusmods.com/site/mods/738](https://www.nexusmods.com/site/mods/738?tab=files)

## Installation for Users

<details>
<summary>Universal Method <b>(Recommended)</b></summary><br/>

  1. Pick **one** of the included generic DLLs to use. Your options are `version.dll`, `winhttp.dll`, or `dbghelp.dll`. We'll choose `version.dll` as an example.
  2. Find your game's installation folder. For Hogwarts Legacy, this is the directory containing `HogwartsLegacy.exe`. An example path is `C:\Program Files (x86)\Steam\steamapps\common\Hogwarts Legacy\Phoenix\Binaries\Win64\`.
  3. Copy `dlssg_to_fsr3_amd_is_better.dll` and `version.dll` to your game's installation folder.
  4. Done. A log file named `dlssg_to_fsr3.log` will be created after you launch the game.
</details>

<details>
<summary>NVNGX Method</summary><br/>
  
  1. Double click on `DisableNvidiaSignatureChecks.reg` and select **Run**. Click **Yes** on the next few dialogs.
  2. Find your game's installation folder. For Cyberpunk 2077, this is the directory containing `Cyberpunk2077.exe`. An example path is `C:\Program Files (x86)\Steam\steamapps\common\Cyberpunk 2077\bin\x64\`.
  3. Copy `dlssg_to_fsr3_amd_is_better.dll` and the new `nvngx.dll` to your game's installation folder.
  4. A log file named `dlssg_to_fsr3.log` will be created after you launch the game.
</details>

<details>
<summary>DLSSTweaks Method</summary><br/>
  
  1. Please see the [included readme](/resources/read_me_dlsstweaks.txt).
</details>

## Installation for Developers

1. Open `CMakeUserEnvVars.json` with a text editor and rename `___GAME_ROOT_DIRECTORY` to `GAME_ROOT_DIRECTORY`.
2. Change the path in `GAME_ROOT_DIRECTORY` to your game of choice. Built DLLs are automatically copied over.
3. Change the path in `GAME_DEBUGGER_CMDLINE` to your executable of choice. This allows direct debugging from Visual Studio's interface.
4. Manually copy `resources\dlssg_to_fsr3.ini` to the game directory for FSR 3 visualization and debug options.

## Building

### Requirements

- This repository and all of its submodules cloned.
- The [Vulkan SDK](https://vulkan.lunarg.com/) and `VULKAN_SDK` environment variable set.
- **Visual Studio 2022** 17.9.6 or newer.
- **CMake** 3.26 or newer.
- **Vcpkg**.

### FidelityFX SDK

1. Open a `Visual Studio 2022 x64 Tools Command Prompt` instance.
2. Navigate to the `dependencies\FidelityFX-SDK\sdk\` subdirectory.
3. Run `BuildFidelityFXSDK.bat` and wait for compilation.
4. Done.

### dlssg-to-fsr3 (Option 1, Visual Studio UI)

1. Open `CMakeLists.txt` directly or open the root folder containing `CMakeLists.txt`.
2. Select one of the preset configurations from the dropdown, e.g. `Universal Release x64`.
3. Build and wait for compilation.
4. Build files are written to the bin folder. Done.

### dlssg-to-fsr3 (Option 2, Powershell Script)

1. Open a Powershell command window.
2. Run `.\Make-Release.ps1` and wait for compilation.
3. Build files from each configuration are written to the bin folder and archived. Done.

## Changelog

<details>
  <summary>Click to expand.</summary><br/>

**Version 0.121**
  - Added additional error logging.
  - Added future proofing for RTX Remix-based games.
  - Fixed issues reading configuration settings when supplied through environment variables.
  
**Version 0.120**
  - Added support for intercepting/hooking over-the-air Streamline plugin updates with the Universal edition.
  - Added support for games that pass in bidirectional distortion field resources.
  - Added workarounds to support Indiana Jones and the Great Circle.
  - Added workarounds to better support RTX Remix-based games.
  - Added workarounds for games providing incorrect camera far, near, and field of view values.
  - Migrated to the latest AMD FidelityFX SDK (v1.1 -> v1.1.3).
  - Fixed occasional blurry rectangle (interpolation rect) issues when switching upscalers or changing output resolutions.
  
**Version 0.110**
  - Added native Vulkan support for FSR 3.1.
  
**Version 0.100**
  - Tentative support for FSR 3.1 frame generation.
  - Added extremely experimental support for Vulkan. Expect artifacts and disocclusion issues.
  - Implemented even more aggressive hooking in the universal variants due to recent DLSS SDK changes.
  - Revised a number of debug log prints.
  
**Version 0.90**
  - Added a Universal zip archive for maximum game support. Separate READ ME.txts are included within each folder. Registry key tweaks are not required.
  - Universal DLLs now automatically disable the EGS overlay due to hooking conflicts.
  - Universal DLLs now bypass GPU architecture checks for stubborn games (Dying Light 2, Returnal).
  - HDR luminance values are now queried from the active monitor, falling back to defaults when necessary.
  - Fixed GPU driver crashes in Dying Light 2 with universal DLLs.
  - Hardware accelerated GPU scheduling status is now logged.
  
**Version 0.81**
  - Fixed GPU hangs in certain games with major scene transitions (e.g. The Witcher 3).
  - Miscellaneous smaller stability fixes and error checking.
  - Added the ability to rename nvngx.dll to version.dll, winhttp.dll, or dbghelp.dll to avoid the registry key signature override requirement.
  
**Version 0.80**
  - Hopefully fixed all texture format conversion crashes (e.g. Hogwarts Legacy).
  - Improved error logging, again.
  
**Version 0.70**
  - Error checking code rewritten.
  - Logging code rewritten.
  - Added better support for texture dimensions/formats changing at runtime.
  - Added a developer config option to show only interpolated frames.
  - Improved nvngx wrapper dll compatibility.
  
**Version 0.60**
  - The nag prompt at startup has been removed.
  - Added a log file ("dlssg_to_fsr3.log") in the game directory.
  - Added support for developer options and debug overlay ("dlssg_to_fsr3.ini").
  - More stability fixes.
  
**Version 0.50**
  - Experimental format conversion support. This mainly includes HDR-enabled games along with mismatched UI render target formats.
  
**Version 0.41**
  - Fixed accidental inclusion of debug overlay.
  
**Version 0.40**
  - Replaced dbghelp.dll with nvngx.dll for better game compatibility. Please delete the old dbghelp.dll version from earlier releases.
  - DisableNvidiaSignatureChecks.reg is now required for usage in games.
  - Various stability fixes.
  
**Version 0.30**
  - Fixed numerous game crashes (e.g. Starfield).
  
**Version 0.21**
  - Fixed a typo in DLL path determination.
  - Added explicit binary license.
  
**Version 0.20**
  - First working build.
  
**Version 0.10**
  - Initial test release.
</details>

## License

- [GPLv3](LICENSE.md)
- [Third party licenses](/resources/binary_dist_license.txt)