<!-- @page page_techniques_contrast-adaptive-sharpening FidelityFX Contrast Adaptive Sharpening 1.2 -->

<h1>FidelityFX Contrast Adaptive Sharpening 1.2</h1>

FidelityFX Contrast Adaptive Sharpening (CAS) is a low overhead adaptive sharpening algorithm with optional up-sampling. The technique is developed by Timothy Lottes (creator of FXAA) and was created to provide natural sharpness without artifacts.

<h2>Integration guidelines</h2>

<h3>Sharpness</h3>

A float value controls the sharpening strength of CAS. You can pass the sharpness through `FfxCasDispatchDescription::sharpness` to the `ffxCasContextDispatch` function in [`ffx_cas.h`](../../sdk/include/FidelityFX/host/ffx_cas.h), in which the value will be packed into an internal constant buffer.

<h3>Sharpening modes</h3>

In the CAS sample, there are three sharpening modes that can be selected:

1. Sharpening disabled.
2. Sharpening enabled, upsampling disabled.
3. Both sharpening and upsampling enabled.

You can also toggle the upscaling mode by yourself by setting `FFX_CAS_SHARPEN_ONLY` in `FfxCasContextDescription::flags`. If you want upsampling enabled, please don't forget to set render and display resolution info in the `FfxCasContextDescription`.

<h3>Color space conversion</h3>

CAS needs linear input color to perform correctly. If your color buffer is not in linear color space, you can set `FfxCasContextDescription::colorSpaceConversion` to a color space supported in `FfxCasColorSpaceConversion` to denote your input color space. CAS will perform color space conversion (if necessary) when loading or outputting color. It also ensures the output will be in the same color space as your input. 

If you need to create a new color space conversion function, here's how:

1.  Create corresponding enum values inside `FfxCasColorSpaceConversion` and `CasShaderPermutationOptions`.
2. Add new branch inside the `POPULATE_PERMUTATION_KEY` macro in [`ffx_cas_shaderblobs.cpp`](../../sdk/src/backends/shared/blob_accessors/ffx_cas_shaderblobs.cpp). Please take the existing branches as reference.
3. Add new pre-compiler branches inside the `casInput`, `casOuput`, `casInputHalf` and `casOutputHalf` functions in [`ffx_cas_callbacks.glsl`](../../sdk/include/FidelityFX/gpu/cas/ffx_cas_callbacks_glsl.h) or [`ffx_cas_callbacks.hlsl`](../../sdk/include/FidelityFX/gpu/cas/ffx_cas_callbacks_hlsl.h). Under these branches you should add your new color space conversion as needed.

<h2>See also</h2>

- [FidelityFX Contrast Adaptive Sharpening](../samples/contrast-adaptive-sharpening.md)
- [FidelityFX Naming guidelines](../getting-started/naming-guidelines.md)
