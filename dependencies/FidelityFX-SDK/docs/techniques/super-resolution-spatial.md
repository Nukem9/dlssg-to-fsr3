<!-- @page page_techniques_super-resolution-spatial FidelityFX Super Resolution 1.2 -->

<h1>FidelityFX Super Resolution 1.2 (FSR1)</h1>

![Screenshot](media/super-resolution-spatial/fsr-sample_resized.jpg "A screenshot showcasing the final output of the effect")

<h2>Table of contents</h2>

* [Introduction](#introduction)
* [Integration guidelines](#integration-guidelines)
    * [Shading language and API requirements](#shading-language-and-api-requirements)
    * [Expected input](#expected-input)
    * [Where in the Frame?](#where-in-the-frame)
    * [Walkthrough](#walkthrough)
* [The technique](#the-technique)

<h2>Introduction</h2>

AMD FidelityFX Super Resolution (FSR1) is our open source, high-quality solution for producing high resolution frames from lower resolution inputs.

It uses a collection of cutting-edge algorithms with a particular emphasis on creating high-quality edges, giving large performance improvements compared to rendering at native resolution directly. FSR1 enables "practical performance" for costly render operations, such as hardware ray tracing. 

<h2>Integration guidelines</h2>

<h3>Shading language and API requirements</h3>

<h4>DirectX 12 + HLSL</h4>

- `HLSL`
  - `CS_6_2`

<h4>Vulkan + GLSL</h4>

- Vulkan 1.x

- `GLSL 4.50` with the following extensions:
  - `GL_EXT_samplerless_texture_functions`

Note that the GLSL compiler must also support `GL_GOOGLE_include_directive` for `#include` handling used throughout the GLSL shader system.

<h3>Expected input</h3>

* Image should already be well anti-aliased by a technique like TAA, MSAA etc. 
* Image should be normalized to [0-1] and be in perceptual color space (sRGB, not linear).
    * A negative input to RCAS will output NaN!
* Image should be generated using negative MIP bias to increase texture detail.
* Image should be noise free.  

<h3>Where in the frame?</h3>

FSR1 should be integrated into your pipeline after anti-aliased rendering and tone mapping, but before any post-processing effects that introduce noise such as Film Grain, Chromatic Aberration etc or User Interface rendering.

![alt text](media/super-resolution-spatial/fsr-where-in-the-frame.jpg "An image showing where in the frame FidelityFX Super Resolution 1.1 should be integrated to.")

<h3>Walkthrough</h3>

Include the [`ffx_fsr1.h`](../../sdk/include/FidelityFX/gpu/fsr1/ffx_fsr1.h) header:

```C++
#include <FidelityFX/host/ffx_fsr1.h>
```

Query the amount of scratch memory required for the FFX Backend using `ffxGetScratchMemorySize`:

```C++
const size_t scratchBufferSize = ffxGetScratchMemorySize(FFX_FSR1_CONTEXT_COUNT);
```

Allocate the scratch memory for the backend and retrieve the interface using `ffxGetInterface`:

```C++
void* scratchBuffer  = malloc(scratchBufferSize);
FfxErrorCode errorCode = ffxGetInterface(&backendInterface, DevicePtr(), scratchBuffer, scratchBufferSize, FFX_FSR1_CONTEXT_COUNT);
FFX_ASSERT(errorCode == FFX_OK);
```

Create the `FfxFsr1Context` by filling out the `FfxFsr1ContextDescription` structure with the required arguments:

```C++
FfxFsr1Context fsr1Context;

FfxFsr1ContextDescription contextDesc = {};

// Fill out arguments
// If RCAS sharpening is required, add the FFX_FSR1_ENABLE_RCAS flag
contextDesc.flags                = FFX_FSR1_ENABLE_HIGH_DYNAMIC_RANGE;
contextDesc.maxRenderSize.width  = resInfo.RenderWidth;
contextDesc.maxRenderSize.height = resInfo.RenderHeight;
contextDesc.displaySize.width    = resInfo.DisplayWidth;
contextDesc.displaySize.height   = resInfo.DisplayHeight;
contextDesc.backendInterface     = backendInterface;

// Create the FSR1 context
ffxFsr1ContextCreate(&fsr1Context, &contextDesc);
```

When the time comes for upscaling, fill out the `FfxFsr1DispatchDescription` structure and call `ffxFsr1ContextDispatch` using it:

```C++
FfxFsr1DispatchDescription dispatchParameters = {};

dispatchParameters.commandList = ffxGetCommandList(pCmdList);
dispatchParameters.renderSize = { resInfo.RenderWidth, resInfo.RenderHeight };
dispatchParameters.enableSharpening = m_RCASSharpen;
dispatchParameters.sharpness = m_Sharpness;
dispatchParameters.color = ffxGetResource(m_pTempColorTarget->GetResource(), L"FSR1_InputColor", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);
dispatchParameters.output = ffxGetResource(m_pColorTarget->GetResource(), L"FSR1_OutputUpscaledColor", FFX_RESOURCE_STATE_PIXEL_COMPUTE_READ);

FfxErrorCode errorCode = ffxFsr1ContextDispatch(&m_FSR1Context, &dispatchParameters);
FFX_ASSERT(errorCode == FFX_OK);
```

During shutdown, destroy the FSR1 context:

```C++
ffxFsr1ContextDestroy(&m_FSR1Context);
```

<h2>The technique</h2>

FidelityFX Super Resolution is a spatial upscaler: it works by taking the current anti-aliased frame at render resolution and upscaling it to display resolution without relying on other data such as frame history or motion vectors.

At the heart of FSR1 is a cutting-edge algorithm that detects and recreates high-resolution edges from the source image. Those high-resolution edges are a critical element required for turning the current frame into a "super resolution" image. 

FSR1 provides consistent upscaling quality regardless of whether the frame is in movement, which can provide quality advantages compared to other types of upscalers.

FSR1 is composed of two main passes:

An upscaling pass called EASU (Edge-Adaptive Spatial Upsampling) that also performs edge reconstruction. In this pass the input frame is analyzed and the main part of the algorithm detects gradient reversals – essentially looking at how neighboring gradients differ – from a set of input pixels. The intensity of the gradient reversals defines the weights to apply to the reconstructed pixels at display resolution.

A sharpening pass called RCAS (Robust Contrast-Adaptive Sharpening) that extracts pixel detail in the upscaled image.

FSR1 also comes with helper functions for color space conversions, dithering, and tone mapping to assist with integrating it into common rendering pipelines used with today's games.

![alt text](media/super-resolution-spatial/fsr-easu.jpg "A diagram showing how FidelityFX Super Resolution looks for gradient reversals in the source image to reconstruct high-definition edges at upscaled resolution.")

<h2>See also</h2>

- [FidelityFX Super Resolution](../samples/super-resolution.md)
- [FidelityFX Naming guidelines](../getting-started/naming-guidelines.md)
