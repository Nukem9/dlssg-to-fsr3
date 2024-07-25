<!-- @page page_samples_combined-adaptive-compute-ambient-occlusion FidelityFX Combined Adaptive Compute Ambient Occlusion -->

<h1>FidelityFX Combined Adaptive Compute Ambient Occlusion (CACAO)</h1>

![alt text](media/combine-adaptive-compute-ambient-occlusion/cacao-sample.jpg "A screenshot of the FidelityFX CACAO sample.")

This sample demonstrates the use of the FidelityFX Combine Adaptive Compute Ambient Occlusion technique.

For details on the technique that underpins the FidelityFX CACAO effect you can refer to the respective [technique documentation](../techniques/combined-adaptive-compute-ambient-occlusion.md).

<h2>Requirements</h2>

 - Windows
 - DirectX(R)12
 - Vulkan(R)

<h2>UI elements</h2>

The sample contains UI options to allow experimentation with relevant values and techniques. The table below summarizes the UI elements and what they control within the sample.

| Element name | Value | Description |
| -------------|-------|-------------|
| **Preset** | `Native - Adaptive Quality`, `Native - High Quality`, `Native - Medium Quality`, `Native - Low Quality`, `Native - Lowest Quality`, `Downsampled - Adaptive Quality`, `Downsampled - High Quality`, `Downsampled - Medium Quality`, `Downsampled - Low Quality`, `Downsampled - Lowest Quality` | Selects the quality and toggles between downsampled and native resolution sampling. |
| **Radius** | `0 .. 10` | The radius in world space of the occlusion sphere. A larger radius will make further objects contribute to the ambient occlusion of a point. |
| **Shadow Multiplier** | `0 .. 5` | The linear multiplier for shadows. Higher values intensify shadows. |
| **Shadow Power** | `0.5 .. 5` | The exponent value for shadows values. Larger values create darker shadows. |
| **Shadow Clamp** | `0 .. 1` | Clamps the shadow values to be within a certain range. |
| **Horizon Angle Threshold** | `0 .. 0.2` | Minimum angle necessary between geometry and a point to create occlusion. Adjusting this value helps reduce self-shadowing. |
| **Fade Out From** | `1 .. 20` | The starting world space distance to fade the effect out over. |
| **Fade Out To** | `1 .. 40` | The end world space distance to fade the effect out over. |
| **Quality Level** | `Highest, High, Medium, Low, Lowest` | Determines various aspects of how FidelityFX CACAO is generated, including number of samples taken for screen space ambient occlusion (SSAO) generation, number of pixels SSAO is generated for, etc. |
| **Adaptive Quality Limit** | `0.5 .. 1` | Limits the total number of samples taken at adaptive quality levels. |
| **Blur Pass Count** | `0 .. 8` | The number of edge sensitive blurs to run on the raw SSAO output. |
| **Sharpness** | `0 .. 1` | The sharpness controls how much blur should bleed over edges. |
| **Detail Shadow Strength** | `0 .. 5` | Adds in more detailed shadows based on edges. These are less temporally stable. |
| **Generate Normal Buffer From Buffer Target** | `Toggle: On/Off` | When toggled on, the normal buffer will be generated from the depth buffer. This means SSAO will only use the depth buffer as input. |
| **Use Downsampled SSAO** | `Toggle: On/Off` | When toggled on, SSAO is generated at half resolution instead of native resolution. |
| **Bilateral Sigma Squared** | `0 .. 10` | Only affects downsampled SSAO. Higher values create a larger blur. |
| **Bilateral Similarity Distance Sigma** | `0.1 .. 1` | Only affects downsampled SSAO. Lower values create sharper edges. |

<h2>Setting up the FidelityFX CACAO effect</h2>

The FidelityFX CACAO shaders take as input the depth buffer produced by the geometry rendering passes. In can also optionally take in the normal buffer produced by the geometry rendering passes, otherwise generating this information from the depth buffer. 

The [`ffx_cacao.h`](../../sdk/include//FidelityFX/host/ffx_cacao.h) file contains the effect context creation, dispatch, constants update, and context destruction functions. 

Note, the effect is designed to work from `C++`, `HLSL`, and `GLSL`. These can be selected via `#define` macros (see below).

C++:

```C++
#define FFX_CPU
#include <gpu/ffx_core.h>
#include <gpu/ffx_cacao.h>
```
HLSL:

```HLSL
#define FFX_GPU
#define FFX_HLSL
#include "ffx_core.h"
#include "ffx_cacao.h"
```
GLSL:

```GLSL
#define FFX_GPU
#define FFX_GLSL
#include "ffx_core.h"
#include "ffx_cacao.h"
```

<h3>Shader options</h3>

Some of the FidelityFX CACAO passes support different options. 

One supported option is the use of 16 bit floating point (FP16) maths instead of 32 bit. 

The shaders can also be run in Wave64 or Wave32. How shaders are run is largely determined at run-time given the executing hardware platform.

Lastly, some passes can run with the `Apply Smart` option, enabling edge aware execution for passes such as `Apply` and `Bilateral Upscale`.

<h2>Sample controls and configurations</h2>

For sample controls, configuration, and FidelityFX Cauldron Framework UI element details, see [Running the samples](../getting-started/running-samples.md).

<h2>See also</h2>

- [FidelityFX Combined Adaptive Compute Ambient Occlusion](../techniques/combined-adaptive-compute-ambient-occlusion.md)
- [FidelityFX Naming guidelines](../getting-started/naming-guidelines.md)
