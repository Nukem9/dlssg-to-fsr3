<!-- @page page_migration Migrating from FSR 3.0 to FSR 3.1 -->

<h1>Migrating from FSR 3.0 to FSR 3.1</h1>

The new API for FSR 3.1 changes how applications interact with FSR in a few ways:

* Upscaling and frame generation are separated.
* Custom backends are no longer supported.
* Backends are created in tandem with the effect context and their lifetimes are tied together.
* All calls are done using ABI-stable structs with support for future extensions.

See [Introduction to FidelityFX API](../docs/getting-started/ffx-api.md) for more detailed information about the new API.

<h2>API setup</h2>

Setup your build system to include headers in ffx-api/include and to link against one of the amd_fidelityfx_dx12 or amd_fidelityfx_vk libraries.

In source files, include `ffx_api/ffx_api.h` for the C api or `ffx_api/ffx_api.hpp` for
C++ convenience utilities.

For upscaling, include `ffx_api/ffx_fsr_upscale.h` (or `.hpp`) and for frame generation,
include `ffx_api/ffx_fg.h` (or `.hpp`).

For backend creation and the frame generation swapchain, you will also need one of `ffx_api/dx12/ffx_api_dx12.h` or `ffx_api/vk/ffx_api_vk.h` (or `.hpp`).

<h2>Context creation</h2>

Instead of calling `ffxFsr3ContextCreate`, call `ffxCreateContext`.

`ffxCreateContextDescFsrUpscale` takes a subset of parameters from `FfxFsr3ContextDescription`, except that
flags now are prefixed `FFX_FSR_` instead of `FFX_FSR3_`.

A backend creation description must be passed in the `pNext` field of the `header` in `ffxCreateContextDescFsrUpscale`.
This can be either a `ffxCreateBackendDX12Desc` or a `ffxCreateBackendVKDesc`.

The `type` field on the `header` of each structure must be set to the associated type id.
In the C headers, these are adjacent to their associated types.
When using the C++ namespaced types, structure types are initialized by the constructor.

For frame generation, a separate context has to be created, as well as one for the frame generation swapchain (backend specific).

For memory allocation, a set of callbacks can be passed. If set to `NULL`, the system `malloc` and `free` are
used.

<h2>Querying Information from FSR</h2>

The functions `ffxFsr3GetUpscaleRatioFromQualityMode`, `ffxFsr3GetRenderResolutionFromQualityMode`,
`ffxFsr3GetJitterPhaseCount` and `ffxFsr3GetJitterOffset` are replaced with structs passed to the
`ffxQuery` function. Pointer parameters to these query functions are struct members in the new API.

<h2>Configuring FSR</h2>

`ffxFsr3ConfigureFrameGeneration` is replaced by `ffxConfigure` with the frame generation context.
The configuration structure is nearly identical. A new optional callback context has been added.
See the comparison table for details.

<h2>Dispatching FSR</h2>

All dispatches are done using the `ffxDispatch` function.

Resources are passed in a new struct `FfxApiResource`, which is very similar to `FfxResource`,
but with more platform stability. With DirectX, a helper function `ffxApiGetResourceDX12` can be
used to fill the resource description.

When using frame generation, a new preparation dispatch must be called every frame.
This can be placed at the same location as the upscaling dispatch and takes a subset of the upscaling resources.

<h2>Comparison</h2>

The following section shows FSR 3.0 code patterns and their FSR 3.1 equivalents.

<h3>Context names</h3>

<h4>FSR 3.0</h4>

```c
FfxFsr3Context fsr3Context;
```

<h4>FSR 3.1 / FFX API C</h4>

```c
ffxContext upscalingContext, framegenContext;
```

<h4>FFX API C++</h4>

```c++
ffx::Context upscalingContext, framegenContext;
```

<h3>Context creation (upscaling only)</h3>

<h4>FSR 3.0</h4>

```c
FfxFsr3ContextDescription fsr3ContextDesc = {0};
// create backend interface ...
// fill fsr3ContextDesc ...
fsr3ContextDesc.flags |= FFX_FSR3_ENABLE_UPSCALING_ONLY;
ffxFsr3ContextCreate(&fsr3Context, &fsr3ContextDesc);
```

<h4>FSR 3.1 / FFX API C</h4>

```c
ffxCreateContextDescFsrUpscale fsrContextDesc = {0};
fsrContextDesc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FSR_UPSCALE;
// fill fsrContextDesc ...
// backend interface desc ...
fsrContextDesc.header.pNext = &backendDesc.header;
ffxCreateContext(&upscalingContext, &fsrContextDesc.header, NULL);
```

<h4>FFX API C++</h4>

```c++
ffx::CreateContextDescFsrUpscale fsrContextDesc{};
// fill fsrContextDesc ...
// backend interface desc ...
ffx::CreateContext(&upscalingContext, nullptr, fsrContextDesc, backendDesc);
```

<h3>DX12 backend creation</h3>

<h4>FSR 3.0</h4>

```c
FfxFsr3ContextDescription fsr3ContextDesc = {0};
// create backend interface (DX12)
size_t scratchBufferSize = ffxGetScratchMemorySizeDX12(1);
void* scratchBuffer = malloc(scrathBufferSize);
memset(scratchBuffer, 0, scrathBufferSize);
errorCode = ffxGetInterfaceDX12(&fsr3ContextDesc.backendInterfaceUpscaling, ffxGetDeviceDX12(dx12Device), scratchBuffer, scratchBufferSize, 1);
assert(errorCode == FFX_OK);
```

<h4>FSR 3.1 / FFX API C</h4>

```c
ffxCreateContextDescFsrUpscale fsrContextDesc = {0};
// fill fsrContextDesc ...
// backend interface desc (dx12)
ffxCreateBackendDX12Desc backendDesc = {0};
backendDesc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12;
backendDesc.device = dx12Device;
fsrContextDesc.header.pNext = &backendDesc.header;
// create context and backend
ffxCreateContext(&upscalingContext, &fsrContextDesc.header, NULL);
```

<h4>FFX API C++</h4>

```c++
ffx::CreateContextDescFsrUpscale fsrContextDesc{};
// fill fsrContextDesc ...
// backend interface desc (dx12)
ffx::CreateBackendDX12Desc backendDesc{};
backendDesc.device = dx12Device;
ffx::CreateContext(&upscalingContext, nullptr, fsrContextDesc, backendDesc);
```

<h3>Context creation (upscale + framegen)</h3>

<h4>FSR 3.0</h4>

```c
FfxFsr3ContextDescription fsr3ContextDesc = {0};
// create backend interface (x3) ...
// fill fsr3ContextDesc ...
ffxFsr3ContextCreate(&fsr3Context, &fsr3ContextDesc);
```

<h4>FSR 3.1 / FFX API C</h4>

```c
ffxCreateContextDescFsrUpscale fsrContextDesc = {0};
fsrContextDesc.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FSR_UPSCALE;
// fill fsrContextDesc ...
// backend interface desc ...
fsrContextDesc.header.pNext = &backendDesc.header;
ffxCreateContext(&upscalingContext, &fsrContextDesc.header, NULL);
ffxCreateContextDescFsrFrameGeneneration fgContextDesc = {0};
// fill fgContextDesc ...
// backend interface desc ...
fgContextDesc.header.pNext = &backendDesc.header;
ffxCreateContext(&frameGenContext, &fgContextDesc.header, NULL);
```

<h4>FFX API C++</h4>

```c++
ffx::CreateContextDescFsrUpscale fsrContextDesc{};
// fill fsrContextDesc ...
// backend interface desc ...
ffx::CreateContext(&upscalingContext, nullptr, fsrContextDesc, backendDesc);
ffx::CreateContextDescFsrFrameGen fgContextDesc{};
// fill fgContextDesc ...
// backend interface desc ...
ffx::CreateContext(&frameGenContext, nullptr, fgContextDesc, backendDesc);
```

<h3>Query upscale ratio / render resolution</h3>

<h4>FSR 3.0</h4>

```c
float upscaleRatio = ffxFsr3GetUpscaleRatioFromQualityMode(FFX_FSR3_QUALITY_MODE_BALANCED);
<!-- -->
uint32_t renderWidth, renderHeight;
ffxFsr3GetRenderResolutionFromQualityMode(&renderWidth, &renderHeight, displayWidth, displayHeight, FFX_FSR3_QUALITY_MODE_BALANCED);
```

<h4>FSR 3.1 / FFX API C</h4>

```c
float upscaleRatio;
struct ffxQueryDescFsrGetUpscaleRatioFromQualityMode queryDesc1 = {0};
queryDesc1.header.type = FFX_API_QUERY_DESC_TYPE_FSR_GETUPSCALERATIOFROMQUALITYMODE;
queryDesc1.qualityMode = FFX_FSR_QUALITY_MODE_BALANCED;
queryDesc1.pOutUpscaleRatio = &upscaleRatio;
ffxQuery(&fsrContext, &queryDesc1.header);
<!-- -->
uint32_t renderWidth, renderHeight;
struct ffxQueryDescFsrGetRenderResolutionFromQualityMode queryDesc2 = {0};
queryDesc2.header.type = FFX_API_QUERY_DESC_TYPE_FSR_GETRENDERRESOLUTIONFROMQUALITYMODE;
queryDesc2.displayWidth = displayWidth;
queryDesc2.displayHeight = displayHeight;
queryDesc2.qualityMode = FFX_FSR_QUALITY_MODE_BALANCED;
queryDesc2.pOutRenderWidth = &renderWidth;
queryDesc2.pOutRenderHeight = &renderHeight;
ffxQuery(&fsrContext, &queryDesc2.header);
```

<h4>FFX API C++</h4>

```c++
float upscaleRatio;
ffx::QueryDescFsrGetUpscaleRatioFromQualityMode queryDesc1{};
queryDesc1.qualityMode = FFX_FSR_QUALITY_MODE_BALANCED;
queryDesc1.pOutUpscaleRatio = &upscaleRatio;
ffx::Query(fsrContext, queryDesc1);
<!-- -->
uint32_t renderWidth, renderHeight;
ffx::QueryDescFsrGetRenderResolutionFromQualityMode queryDesc2{};
queryDesc2.displayWidth = displayWidth;
queryDesc2.displayHeight = displayHeight;
queryDesc2.qualityMode = FFX_FSR_QUALITY_MODE_BALANCED;
queryDesc2.pOutRenderWidth = &renderWidth;
queryDesc2.pOutRenderHeight = &renderHeight;
ffx::Query(fsrContext, queryDesc2);
```

<h3>Query Jitter count / offset</h3>

<h4>FSR 3.0</h4>

```c
int32_t jitterCount = ffxFsr3GetJitterPhaseCount(renderWidth, displayWidth);
<!-- -->
float jitterX, jitterY;
ffxFsr3GetJitterOffset(&jitterX, &jitterY, jitterIndex, jitterCount);
```

<h4>FSR 3.1 / FFX API C</h4>

```c
int32_t jitterCount;
struct ffxQueryDescFsrGetJitterPhaseCount queryDesc1 = {0};
queryDesc1.header.type = FFX_API_QUERY_DESC_TYPE_FSR_GETJITTERPHASECOUNT;
queryDesc1.renderWidth = renderWidth;
queryDesc1.displayWidth = displayWidth;
queryDesc1.pOutPhaseCount = &jitterCount;
ffxQuery(&fsrContext, &queryDesc1.header);
<!-- -->
float jitterX, jitterY;
struct ffxQueryDescFsrGetJitterOffset queryDesc2 = {0};
queryDesc2.header.type = FFX_API_QUERY_DESC_TYPE_FSR_GETJITTEROFFSET;
queryDesc2.index = jitterIndex;
queryDesc2.phaseCount = jitterCount;
queryDesc2.pOutX = &jitterX;
queryDesc2.pOutY = &jitterY;
ffxQuery(&fsrContext, &queryDesc2.header);
```

<h4>FFX API C++</h4>

```c++
int32_t jitterCount;
ffx::QueryDescFsrGetJitterPhaseCount queryDesc1{};
queryDesc1.renderWidth = renderWidth;
queryDesc1.displayWidth = displayWidth;
queryDesc1.pOutPhaseCount = &jitterCount;
ffx::Query(fsrContext, queryDesc1);
<!-- -->
float jitterX, jitterY;
ffxQueryDescFsrGetJitterOffset queryDesc2{};
queryDesc2.index = jitterIndex;
queryDesc2.phaseCount = jitterCount;
queryDesc2.pOutX = &jitterX;
queryDesc2.pOutY = &jitterY;
ffx::Query(fsrContext, queryDesc2);
```

<h3>Configure FrameGen</h3>

<h4>FSR 3.0</h4>

```c
FfxFrameGenerationConfig config = {0};
// fill config ...
ffxFsr3ConfigureFrameGeneration(&fsr3Context, &config);
```

<h4>FSR 3.1 / FFX API C</h4>

```c
struct ffxConfigureDescFrameGeneration config = {0};
config.header.type = FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION;
// fill config ...
ffxConfigure(&fsrContext, &config);
```

<h4>FFX API C++</h4>

```c++
ffx::ConfigureDescFrameGeneration config{};
// fill config ...
ffxConfigure(fsrContext, config);
```

<h3>Present callback</h3>

<h4>FSR 3.0</h4>

```c
FfxErrorCode myPresentCallback(const FfxPresentCallbackDescription* params, void* userCtx) // note: pre-3.1 does not have user context pointer
{
    // pre-presentation work, e.g. UI
    return FFX_OK;
}
<!-- -->
// ...
FfxFrameGenerationConfig config;
config.presentCallback = myPresentCallback;
config.presentCallbackContext = &myEngineContext;
```

<h4>FSR 3.1 / FFX API C</h4>

```c
ffxReturnCode_t myPresentCallback(struct ffxCallbackDescFrameGenerationPresent* params, void* pUserCtx)
{
    // pre-presentation work, e.g. UI
    return FFX_API_RETURN_OK;
}
<!-- -->
// ...
struct ffxConfigureDescFrameGeneration config;
config.presentCallback = myPresentCallback;
config.presentCallbackUserContext = &myEngineContext;
```

<h4>FFX API C++</h4>

```c++
extern "C" ffxReturnCode_t myPresentCallback(ffxCallbackDescFrameGenerationPresent* params, void* pUserCtx)
{
    // pre-presentation work, e.g. UI
    return FFX_API_RETURN_OK;
}
<!-- -->
// ...
ffx::ConfigureDescFrameGeneration config;
config.presentCallback = myPresentCallback;
config.presentCallbackUserContext = &myEngineContext;
```

<h3>Dispatch upscaling (no fg)</h3>

<h4>FSR 3.0</h4>

```c
// (context created with UPSCALING_ONLY flag)
FfxFsr3DispatchUpscaleDescription params = {0};
// fill params ...
ffxFsr3ContextDispatchUpscale(&fsr3Context, &params);
```

<h4>FSR 3.1 / FFX API C</h4>

```c
struct ffxDispatchDescFsrUpscale params = {0};
params.header.type = FFX_API_DISPATCH_DESC_TYPE_FSR_UPSCALE;
// fill params ...
ffxDispatch(&fsrContext, &params);
```

<h4>FFX API C++</h4>

```c++
ffx::DispatchDescFsrUpscale params{};
// fill params ...
ffx::Dispatch(fsrContext, params);
```

<h3>Dispatch upscaling and fg prepare</h3>

<h4>FSR 3.0</h4>

```c
// dispatch upscaling and prepare resources for frame generation
FfxFsr3DispatchUpscaleDescription params = {0};
// fill params ...
ffxFsr3ContextDispatchUpscale(&fsr3Context, &params);
```

<h4>FSR 3.1 / FFX API C</h4>

```c
struct ffxDispatchDescFsrUpscale upscaleParams = {0};
upscaleParams.header.type = FFX_API_DISPATCH_DESC_TYPE_FSR_UPSCALE;
struct ffxDispatchDescFrameGenerationPrepare frameGenParams = {0};
frameGenParams.header.type = FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE;
// fill both structs with params ...
ffxDispatch(&fsrContext, &upscaleParams);
ffxDispatch(&fgContext, &frameGenParams);
```

<h4>FFX API C++</h4>

```c++
ffx::DispatchDescFsrUpscale upscaleParams{};
ffx::DispatchDescFrameGenerationPrepare frameGenParams{};
// fill both structs with params ...
ffx::Dispatch(fsrContext, upscaleParams);
ffx::Dispatch(fgContext, frameGenParams);
```

<h3>Dispatch frame gen (no callback mode)</h3>

<h4>FSR 3.0</h4>

```c
FfxFrameGenerationDispatchDescription fgDesc = {0};
ffxGetFrameinterpolationCommandlistDX12(ffxSwapChain, fgDesc.commandList);
fgDesc.outputs[0]            = ffxGetFrameinterpolationTextureDX12(ffxSwapChain);
// other parameters ...
ffxFsr3DispatchFrameGeneration(&fgDesc);
```

<h4>FSR 3.1 / FFX API C</h4>

```c
struct ffxDispatchDescFrameGeneration dispatchFg = {0};
dispatchFg.header.type = FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION;
<!-- -->
struct ffxQueryDescFrameGenerationSwapChainInterpolationCommandListDX12 queryCmdList = {0};
queryCmdList.header.type = FFX_API_QUERY_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_INTERPOLATIONCOMMANDLIST_DX12;
queryCmdList.pOutCommandList = &dispatchFg.commandList;
ffxQuery(&swapChainContext, &queryCmdList);
<!-- -->
struct ffxQueryDescFrameGenerationSwapChainInterpolationTextureDX12 queryFiTexture{};
queryFiTexture.header.type = FFX_API_QUERY_DESC_TYPE_FRAMEGENERATIONSWAPCHAIN_INTERPOLATIONTEXTURE_DX12;
queryFiTexture.pOutTexture = &dispatchFg.outputs[0];
ffxQuery(&swapChainContext, &queryFiTexture);
<!-- -->
// other parameters ...
ffxDispatch(&fgContext, &dispatchFg);
```

<h4>FFX API C++</h4>

```c++
ffx::DispatchDescFrameGeneration dispatchFg{};
<!-- -->
ffx::QueryDescFrameGenerationSwapChainInterpolationCommandListDX12 queryCmdList{};
queryCmdList.pOutCommandList = &dispatchFg.commandList;
ffx::Query(swapChainContext, queryCmdList);
<!-- -->
ffx::QueryDescFrameGenerationSwapChainInterpolationTextureDX12 queryFiTexture{};
queryFiTexture.pOutTexture = &dispatchFg.outputs[0];
ffx::Query(swapChainContext, queryFiTexture);
<!-- -->
// other parameters ...
ffx::Dispatch(fgContext, dispatchFg);
```

<h3>Destroy context and backend</h3>

<h4>FSR 3.0</h4>

```c
ffxFsr3ContextDestroy(&fsr3Context);
// for each backend interface:
free(backendInterface.scratchBuffer);
```

<h4>FSR 3.1 / FFX API C</h4>

```c
ffxDestroyContext(&fsrContext, NULL);
```

<h4>FFX API C++</h4>

```c++
ffx::DestroyContext(fsrContext, nullptr);
```