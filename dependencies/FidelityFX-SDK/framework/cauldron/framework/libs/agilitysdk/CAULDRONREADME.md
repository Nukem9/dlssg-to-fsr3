# DX12 Agility SDK

## Current Version
1.614.1

## How to update
- Download the latest version (as a .nupkg) from https://devblogs.microsoft.com/directx/directx12agility/
- Rename extension (.nupkg) to .zip
- Unzip it
- Copy contents of /build/native/bin/x64 to bin/x64 in this folder
- Copy /build/native/include folder to this folder
- Update D3D12SDKVersion extern defined at the top of device_dx12.cpp with the subversion number (i.e. 1.614.1 -> 614)
- Force a rebuild of the entire solution to ensure proper linking occurs.
- Update this `CAULDRONREADME.md` with the new version number.
- Take a look at `FFX_SDK_README.md` because you may want to update that agility as well.
