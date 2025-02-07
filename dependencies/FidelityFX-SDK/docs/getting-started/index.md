<!-- @page page_getting-started_index Introduction to the MD FidelityFX SDK -->

<h1>Introduction to the AMD FidelityFX SDK</h1>

The FidelityFX SDK is a collection of heavily optimized, open source effects (shader and runtime code) that can be used by developers to improve their DirectX®12 or Vulkan® applications. The FidelityFX SDK includes a number of effects:

- [Combined Adaptive Compute Ambient Occlusion 1.4](../techniques/combined-adaptive-compute-ambient-occlusion.md)
- [Contrast Adaptive Sharpening 1.2](../techniques/contrast-adaptive-sharpening.md)
- [Denoiser 1.3](../techniques/denoiser.md)
- [Classifier 1.3](../techniques/classifier.md)
- [Luminance Preserving Mapper 1.4](../techniques/luminance-preserving-mapper.md)
- [Parallel Sort 1.3](../techniques/parallel-sort.md)
- [Single Pass Downsampler 2.2](../techniques/single-pass-downsampler.md)
- [Stochastic Screen-Space Reflections 1.5](../techniques/stochastic-screen-space-reflections.md)
- [Super Resolution 1.2](../techniques/super-resolution-spatial.md)
- [Super Resolution 2.3.2](../techniques/super-resolution-temporal.md)
- [Super Resolution 3.1.3](../techniques/super-resolution-interpolation.md)
- [Super Resolution 3.1.3 Upscaler](../techniques/super-resolution-upscaler.md)
- [Variable Shading 1.2](../techniques/variable-shading.md)
- [Blur 1.1](../techniques/blur.md)
- [Depth of Field 1.1](../techniques/depth-of-field.md)
- [Lens 1.1](../techniques/lens.md)
- [Breadcrumbs 1.0](../techniques/breadcrumbs.md)

<h2>Supported ecosystems</h2>

This version of the AMD FidelityFX SDK comes with samples that run on the following APIs:

- DirectX(R)12
- Vulkan(R)

The shader code which comprises the core of AMD FidelityFX SDK is written in HLSL and GLSL, and can easily be ported to other platforms which support modern shader models.

If you are a registered Xbox developer, you can find AMD FidelityFX features available as part of the Microsoft Game Development Kit (GDK).

<h2>Samples</h2>

All samples are written in C++, and use the [FidelityFX Cauldron Framework](../../framework/cauldron) sample framework.

<h2>Open source</h2>

AMD FidelityFX SDK is open source, and distributed under the MIT license.

For more information on the license terms please refer to the [license](../license.md).

<h2>Support</h2>

We endeavour to keep the AMD FidelityFX SDK updated with new features and bug fixes as often as we can, and perform compatibility and performance testing on a wide range of hardware.

If you find an issue, or have a request for the SDK, please consider opening an issue.

<!-- - @subpage page_getting-started_sdk-structure "SDK Structure" -->
<!-- - @subpage page_building-samples_index "Building the samples for the SDK" -->
<!-- - @subpage page_running-samples_index "Running the samples for the SDK" -->
<!-- - @subpage page_getting-started_naming-guidelines "FidelityFX naming guidelines for game applications" -->
<!-- - @subpage page_ffx-api "Introduction to FidelityFX API" -->
<!-- - @subpage page_migration "Migrating from FSR 3.0 to FSR 3.1" -->
