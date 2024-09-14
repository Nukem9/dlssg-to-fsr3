# dlssg-to-fsr3

...is a drop-in mod/replacement for games utilizing [Nvidia's DLSS-G Frame Generation](https://nvidianews.nvidia.com/news/nvidia-introduces-dlss-3-with-breakthrough-ai-powered-frame-generation-for-up-to-4x-performance) technology, allowing people to use [AMD's FSR 3 Frame Generation](https://github.com/GPUOpen-LibrariesAndSDKs/FidelityFX-SDK) technology instead. Only RTX 1600, RTX 2000, and RTX 3000 series GPUs are currently supported.

## Download Link
[https://www.nexusmods.com/site/mods/738](https://www.nexusmods.com/site/mods/738?tab=files)

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

## Installation (User)

1. Make sure [Hardware Accelerated GPU Scheduling](https://devblogs.microsoft.com/directx/hardware-accelerated-gpu-scheduling/) is enabled on Windows
2. Double click on `DisableNvidiaSignatureChecks.reg` and select **Run**. Click **Yes** on the next few dialogs.
3. Find your game's installation folder. For Cyberpunk 2077, this is the directory containing `Cyberpunk2077.exe`. An example path is `C:\Program Files (x86)\Steam\steamapps\common\Cyberpunk 2077\bin\x64\`.
4. Copy `dlssg_to_fsr3_amd_is_better.dll` and the new `nvngx.dll` to your game's installation folder.
5. A log file named `dlssg_to_fsr3.log` will be created after you launch the game.
6. _Additional steps for Epic based games_ - if frame generation is disabled for Epic games, [delete the contents (but keep an empty folder!) of `C:\Program Files (x86)\Epic Games\Launcher\Portal\Extras\Overlay`](https://github.com/Nukem9/dlssg-to-fsr3/issues/152#issue-2049862583)

## Installation (Developer)

1. Open `CMakeUserEnvVars.json` with a text editor and rename `___GAME_ROOT_DIRECTORY` to `GAME_ROOT_DIRECTORY`.
2. Change the path in `GAME_ROOT_DIRECTORY` to your game of choice. Built DLLs are automatically copied over.
3. Change the path in `GAME_DEBUGGER_CMDLINE` to your executable of choice. This allows direct debugging from Visual Studio's interface.
4. Manually copy `resources\dlssg_to_fsr3.ini` to the game directory for FSR3 visualization and debug options.

## License

- [GPLv3](LICENSE.md)
