dlssg-to-fsr3 may be obtained from: https://www.nexusmods.com/site/mods/738 or https://github.com/Nukem9/dlssg-to-fsr3
DLSSTweaks may be obtained from: https://www.nexusmods.com/site/mods/550 or https://github.com/emoose/DLSSTweaks

DO NOT USE IN MULTIPLAYER GAMES.

=====================================
===== Installation instructions =====
=====================================

1. Locate your game's installation folder with DLSSTweaks (e.g. _nvngx.dll) installed.

2. Copy "dlssg_dlsstweaks_wrapper.dll" and "dlssg_to_fsr3_amd_is_better.dll" to your game's installation folder.

3. Find "dlsstweaks.ini" and open it in Notepad.

4. Find the line "[DLLPathOverrides]" after scrolling down to the middle of the INI file.

5. Find the line ";nvngx_dlssg = C:\Users\Username\...\nvngx_dlssg.dll" underneath.

6. Replace the line with "nvngx_dlssg = C:\Path\To\The\Game\dlssg_dlsstweaks_wrapper.dll".

   You MUST TYPE THE FULL DLL PATH and remove the ";" at the beginning. dlssg-to-fsr3 may not work otherwise.

7. Done. A log file named "dlssg_to_fsr3.log" will be created after you launch the game.

=====================================
==== Uninstallation instructions ====
=====================================

1. Delete "dlssg_dlsstweaks_wrapper.dll" and "dlssg_to_fsr3_amd_is_better.dll" in your game's installation folder.

2. Remove the line "nvngx_dlssg = C:\Path\To\The\Game\dlssg_dlsstweaks_wrapper.dll" in "dlsstweaks.ini".