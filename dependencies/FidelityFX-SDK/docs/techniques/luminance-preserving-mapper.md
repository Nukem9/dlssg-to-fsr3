<!-- @page page_techniques_lpm FidelityFX Luminance Preserving Mapper 1.4 -->

<h1>FidelityFX Luminance Preserving Mapper 1.4</h1>

<h2>What is FidelityFX Luminance Preserving Mapper?</h2>

The FidelityFX Luminance Preserving Mapper (LPM) is a tone mapping and gamut mapping solution for high dynamic range (HDR) and wide gamut content. FidelityFX LPM tone maps the luminance (luma) of the red-green-blue (RGB) pixel instead of the color itself, but ensures sure that the `tonemap(luma(RGB))` would be very similar to the `luma(tonemap(RGB))`, that is to say it preserves the luminance information of the pixel.

<h3>FidelityFX LPM off</h3>

![NoLPM](media/lpm/NoLPM.jpg)

<h2>FidelityFX LPM on</h2>

![LPM](media/lpm/LPM.jpg)

<h2>High-level overview:</h2>

FidelityFX LPM is split into two parts: a setup call and filter call. 

The setup call writes pertinent data to a fixed size control block with regards to what the tone and gamut mapping calculations need, and the filter call reads from the control block, calculates, and outputs a tone and gamut mapped color value or pair of values for the FP16 version.

`ffx_lpm.h`:
 - A common header file for CPU-side setup of the mapper and GPU-side setup and tone and gamut map calculation functions.
 - `LpmSetup()` is used to setup all the data required by mapper in a control block:
   - What is the content gamut.
   - What is the display gamut.
   - Max brightness value of content in RGB.
   - Exposure steps above SDR/LDR 1.0
 - `LPMFilter()` is used to do the calculations for mapper by reading data from the control block.
 - For detailed intructions please read the comments in `ffx_lpm.h`

`ffx_lpm.cpp`:
 - CPU-side setup code for FidelityFX LPM.
 - Select the right `LPM_config_*_*` and `LPM_color_*_*` configurations based on content gamut and display mode selected.

`ffx_lpm_filter.hlsl`:
 - GPU-side call to do the tone and gamut mapping.
 - Select right configurations of `LPM_config_*_*` based on content gamut and display mode selected.

<h2>See also</h2>

- [FidelityFX Luminance Preserving Mapper Sample](../samples/luminance-preserving-mapper.md)
- [FidelityFX Naming guidelines](../getting-started/naming-guidelines.md)
