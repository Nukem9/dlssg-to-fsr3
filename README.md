# DLSSG to FSR4 FG

This is a GPLv3 fork of [Nukem9/dlssg-to-fsr3](https://github.com/Nukem9/dlssg-to-fsr3). It keeps the project's low-overhead DirectX 12 DLSS-G input path and, on supported AMD hardware, upgrades the FSR 3.1.6 frame-generation provider to AMD FSR 4 Frame Generation.

## What This Changes

- Games continue to submit their existing DLSS-G frame-generation inputs.
- The mod uses the AMD FSR 3.1.6 provider API required by the original project.
- On an AMD driver that exposes FSR 4 Frame Generation, the provider is upgraded to FSR 4 FG.
- If the FSR 4 runtime or driver provider is unavailable, the original FSR 3 path remains active.
- Camera, depth, motion-vector, and display-resolution calibration are retained to avoid changing the original low-overhead dispatch model.

This is a frame-generation change only. It does not enable FSR 4 Super Resolution for games that do not otherwise use an AMD-supported FSR upscaling path.

## Scope And Requirements

- The FSR 4 path is DirectX 12 only.
- A game must already expose a working DLSS-G frame-generation path that this project can replace.
- AMD Radeon RX 9000-series hardware and an AMD driver with the official FSR 4 FG provider are required for FSR 4 FG.
- The AMD FidelityFX loader and frame-generation runtime must be obtained and distributed under AMD's applicable terms. They are intentionally not included in this source repository or its build packaging.
- The normal fallback remains FSR 3 FG when these requirements are not met.

## Validation Status

The FSR 4 FG path has been tested successfully in:

- Zenless Zone Zero
- FINAL FANTASY XVI

Other DLSS-G games are expected to use the same input path but are unverified. Compatibility, image quality, and stability are game-dependent.

## License

The project is licensed under [GPLv3](LICENSE.md). Existing third-party notices for the original project are retained in [resources/binary_dist_license.txt](resources/binary_dist_license.txt). AMD-signed runtime binaries are not redistributed by this repository.
