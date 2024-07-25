<!-- @page page_samples_single-pass-downsampler FidelityFX Single Pass Downsampler -->

<h1>FidelityFX Single Pass Downsampler</h1>

![alt text](media/single-pass-downsampler/spd-sample.jpg "A screenshot of the SPD sample.")

This sample demonstrates the use of the FidelityFX Single Pass Downsampler (SPD) effect.

For details on the technique that underpin the FidelityFX SPD effect, you can refer to the respective [technique documentation](../techniques/single-pass-downsampler.md).

<h2>Requirements</h2>

 - Windows
 - DirectX(R)12
 - Vulkan(R)

<h2>UI elements</h2>

The sample contains various UI elements to help you explore the technique it demonstrates. The table below summarises the UI elements and what they control within the sample.

| Element name | Value | Description |
| -------------|-------|-------------|
| **Downsampler Option** | `SPD CS, Multipass CS, Multipass PS` | Toggles between the FidelityFX Single Pass Downsampler compute shader (SPD CS), a multipass compute shader implementation (Multipass CS), and a multipass pixel shader (Multipass PS) implementation. |
| **SPD Load/Linear** | `Load, Linear` | Toggles the type of sampling; linearly interpolated sampling or pixel loads. Used for gathering input color in the SPD compute shader. |
| **SPD Wave Interop** | `WaveOps, LocalDataShare` | Toggles the method used for interoperation between threads; wave intrinsic operations (e.g. `WaveReadLaneAt`) or local data share (LDS) and atomic operations. `WaveOps` are generally faster. |
| **SPD Math** | `Packed, Non-Packed` | Toggles between packed 16 bit floating point math and non-packed 32 bit floating point math. |

<h2>Setting up the FidelityFX Single Pass Downsampler compute shader</h2>

The single pass downsampler header, `ffx_spd.h`, provides a function called `SpdSetup` that computes the thread group dimensions required to launch the compute shader dispatch as well as the work group offset, number of work groups, and mips. The   latter parameters are required as input to the `SpdDownsample` function defined in `ffx_spd.h`, which performs the downsampling in the compute shader.

The `ffx_spd.h` header file is designed to be included from both C++ and HLSL/GLSL via `#define` macros (see below).

C++:

```C++
#define FFX_CPU
#include <gpu/ffx_core.h>
#include <gpu/ffx_spd.h>
```

HLSL:

```HLSL
#define FFX_GPU
#define FFX_HLSL
#include "ffx_core.h"
#include "ffx_spd.h"
```

GLSL:

```GLSL
#define FFX_GPU
#define FFX_GLSL
#include "ffx_core.h"
#include "ffx_spd.h"
```

Note that `ffx_spd.h` is dependent on `ffx_core.h`, so both must be included.

<h3>Shader options</h3>

The FidelityFX SPD shader, or more specifically the `SpdDownsampler` has a few different options. 

One option is support for using 16 bit floating point (FP16) math versus 32 bit floating point (FP32) math. In general, FP16 math is more efficient than FP32 math on AMD hardware. 

Another option is the use of a linear sampling versus texel loads for gathering the input texels for the downsampled output. 

The last option is the use of wave intrinsic operations for thread interoperation versus the local data share (LDS). 

For performance considerations, the sample compiles these options into the multiple shader permutations and chooses the specific permutation of the shader at runtime depending on the sample's current configuration (see UI Elements section above) and hardware capabilities. In all, the sample demonstrates eight permutations of the SPD compute shader:

* FP16-wave-ops-load (A_HALF=1 with spd_integration.hlsl)
* FP16-no-wave-ops-load (A_HALF=1, SPD_NO_WAVE_OPERATIONS=1 with spd_integration.hlsl)
* FP16-wave-ops-linear-sampler (A_HALF=1 with spd_integration_linear_sampler.hlsl)
* FP16-no-wave-ops-linear-sampler (A_HALF=1, SPD_NO_WAVE_OPERATIONS=1 with spd_integration_linear_sampler.hlsl)
* FP32-wave-ops-load (spd_integration.hlsl only)
* FP32-no-wave-ops-load (SPD_NO_WAVE_OPERATION=1 with spd_integration.hlsl)
* FP32-wave-ops-linear-sampler (spd_integration_linear_sampler.hlsl only)
* FP32-no-wave-ops-linear-sampler (SPD_NO_WAVE_OPERATION=1 with spd_integration_linear_sampler.hlsl)

<h3>FidelityFX SPD usage</h3>

SPD can be used in 2 ways:

1. Low level usage - including shader code from `gpu\spd\ffx_spd.h`, and setting up inputs manually. This can yield more flexibility from the calling code.

2. High level usage - use the effect component as in `host\ffx_spd.h`, which will automatically downsample a given texture acording to passed in options with a simple dispatch call.

<h2>Sample controls and configurations</h2>

For sample controls, configuration, and FidelityFX Cauldron Framework UI element details, see [Running the samples](../getting-started/running-samples.md).

<h2>See also</h2>

- [FidelityFX Single Pass Downsampler](../techniques/single-pass-downsampler.md)
- [FidelityFX Naming guidelines](../getting-started/naming-guidelines.md)
