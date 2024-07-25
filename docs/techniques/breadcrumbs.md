<!-- @page page_techniques_breadcrumbs FidelityFX Breadcrumbs 1.0 -->

<h1>FidelityFX Breadcrumbs 1.0</h1>

Breadcrumbs library aiding with post-mortem GPU crash analysis.

<h2>Problem</h2>

Due to explicit nature of modern graphics APIs like **Direct3D 12** and **Vulkan**, it's easier to induce GPU crash due to incorrect usage of the API.
When such crash happens, operating systems is using a mechanism called **Timeout Detection Recovery** (TDR in short) to restart the GPU and bring it back online.
However in this process all GPU processes are terminated and the applications are receiving **Device Lost** error, meaning that they have to reinitialize all their graphics facilities.
Usually it means displaying error message to the user, saving all possible state and restarting whole application.
To debug such crashes it's essential to understand which part of complex rendering process failed and where to start looking to fix the problem.
Only available data for such post-mortem analysis is sparse and leads to investigation of the engine's whole graphics subsystem that takes extended periods of time.

<h2>Solution</h2>

To help developers in debbuging GPU crashes, there has been developed a technique based on placing breadcrumbs around actual GPU work to determine which commands were executing at the time of crash.
These breadcrumbs consits of writes to special buffer that can only happen just before the desired command runs and shortly after it completes.
By examining this buffer it is possible to reconstruct what the graph of actual workload that has been executing on GPU and which commands have completed, were in flight or not started at all during the crash.
With this information the developer can start looking for the source of the crash with better knowledge of the problem in the rendering process and thus greatly reducing time to solve it.

<h2>Example output</h2>

Below is the example output of the library with breadcrumbs tree after GPU crash.
Command from frame 251 and 252 all haven't started yet, while GPU finished performing up to `ClearRenderTarget` operations in frame 250.
There was single command in flight, that was the main cause of the crash: `DrawIndexed: "Draw simple triangle"` in frame 250.

```
[BREADCRUMBS]
<Frame 250>
 - [>] Queue type <0>, submission no. 0, command list 1: "VK test command list"
    ├─[X] RESOURCE_BARRIER: "Backbuffer barrier to RT"
    ├─[>] Main Rendering
    │  ├─[X] CLEAR_RENDER_TARGET: "Reset current backbuffer contents"
    │  └─[>] DRAW_INDEXED: "Draw simple triangle"
    └─[ ] RESOURCE_BARRIER: "Backbuffer barrier to PRESENT"
<Frame 251>
 - [ ] Queue type <0>, submission no. 0, command list 1: "VK test command list"
    ├─[ ] RESOURCE_BARRIER: "Backbuffer barrier to RT"
    ├─[ ] Main Rendering
    │  ├─[ ] CLEAR_RENDER_TARGET: "Reset current backbuffer contents"
    │  └─[ ] DRAW_INDEXED: "Draw simple triangle"
    └─[ ] RESOURCE_BARRIER: "Backbuffer barrier to PRESENT"
<Frame 252>
 - [ ] Queue type <0>, submission no. 0, command list 1: "VK test command list"
    ├─[ ] RESOURCE_BARRIER: "Backbuffer barrier to RT"
    ├─[ ] Main Rendering
    │  ├─[ ] CLEAR_RENDER_TARGET: "Reset current backbuffer contents"
    │  └─[ ] DRAW_INDEXED: "Draw simple triangle"
    └─[ ] RESOURCE_BARRIER: "Backbuffer barrier to PRESENT"
```

<h2>Features</h2>

This library implements previously described breadcrumbs technique allowing placing Begin-End markers around single draw calls or whole groups of them.
Additionally you can give each marker a meaningful name and tag that will appear later in the output.

For direct API reference please take a look at [`host/ffx_breadcrumbs.h`](../../sdk/include/FidelityFX/host/ffx_breadcrumbs.h) header file with all the options and parameters described.
This library supports Direct3D 12 and Vulkan backends by default but you can develop your own backend by providing callbacks with prefix `fpBreadcrumbs` in [`FfxInterface`](../../sdk/include/FidelityFX/host/ffx_interface.h).
After GPU crash you can simply call `ffxBreadcrumbsPrintStatus()` to generate text buffer with marker tree and crashing device info to save it and analyze later.

Please note that the accuracy of the breadcrumbs markers is highly dependent on type of crash and characteristics of work scheduled on GPU.
In the specific workloads the situation might occur that cache flush would happen earlier or later than the crashing command or GPU would continue
to perform proceeding work in command lists delaying report of crash which may lead to offset in indicators which commands were in flight.

All string names passed to the library can be selectively marked as "externally owned", meaning that their memory won't be copied and original pointer will be used.
This allows to save the memory in presence of static strings and the ones that have their underlaying memory preserved by other techniques.
When performing similar consecutive draw calls that each name would be formed in the pattern of "**Draw triangle 1**", "**Draw triangle 2**", etc, you can omit the increasing number in the name string
as the library will automatically add the ordinal number to same tags, ex. "**DRAW_INDEXED 1**", "**DRAW_INDEXED 2**", etc.
With this feature you can rely on static string names and in result avoid the unnecessary copies.

<h2>Usage</h2>

To use FidelityFX Breadcrumbs you have to create instance of `FfxBreadcrumbsContext` which will enable setting Begin-End markers.
The best place to use them will be in your general markers facilities, ex. using them along classic **PIX** markers.

<h3>Code sample</h3>

For quick reference on how to use features of this library, please take a look at sample code below.

```cpp
    /* Preceding initialization of graphic's API and GPU device... */
 
    FfxBreadcrumbsContextDescription breadDesc = {}; // Fill out initial parameters as desired
    
    // Initialize given API backend
    const size_t backendSize = ffxGetScratchMemorySizeDX12(1);
    uint8_t* backendBuffer = new uint8_t[backendSize];
    // Pass in required parameters for given graphic's API connection
    ffxGetInterfaceDX12(&breadDesc.backendInterface, ffxGetDeviceDX12(d3d12Device), backendBuffer, backendSize, 1);

    FfxBreadcrumbsContext breadCtx;
    ffxBreadcrumbsContextCreate(&breadCtx, &breadDesc); // Create main Breadcrumbs context

    // After creating pipeline state you can optionally register it inside the context to provide shader details for markers
    FfxBreadcrumbsPipelineStateDescription pipelineDesc = {};
    pipelineDesc.Pipeline = ffxGetPipelineDX12(d3d12Pipeline1);
    pipelineDesc.VertexShader = { "PhongVS", true }; // Static strings don't require copy
    /* Fill out other desired parameters */
    ffxBreadcrumbsRegisterPipeline(&breadCtx, &pipelineDesc);

    pipelineDesc.Pipeline = ffxGetPipelineDX12(d3d12Pipeline2);
    pipelineDesc.VertexShader = { shaderName + "VS", false }; // Dynamic strings require copy
    /* Fill out other desired parameters */
    ffxBreadcrumbsRegisterPipeline(&breadCtx, &pipelineDesc);
 
    // Render loop
    while (true)
    {
        // Start new frame
        ffxBreadcrumbsStartFrame(&breadCtx);
 
        // Register command list before using breadcrumbs with it (can be done before or after opening command list)
        FfxBreadcrumbsCommandListDescription listDesc = {};
        // You can call ffxGetCommandList*() multiple times, no need to cache results
        listDesc.commandList = BffxGetCommandListDX12(d3d12CommandList);
        listDesc.pipeline = ffxGetPipelineDX12(d3d12Pipeline1); // Can be nullptr and set later on
        /* Fill out other desired parameters */
        ffxBreadcrumbsRegisterCommandList(&breadCtx, &listDesc);
 
        // Group multiple calls into single marker...
        ffxBreadcrumbsBeginMarker(&breadCtx, ffxGetCommandListDX12(d3d12CommandList), /* ... */);
 
        /* Multiple draw calls */
 
        // ...or put them per single draw call
        ffxBreadcrumbsBeginMarker(/* ... */);
        d3d12CommandList->DrawInstanced(/* ... */);
        ffxBreadcrumbsEndMarker(/* ... */);
    
        // You can change what pipeline will be used for next markers...
        ffxBreadcrumbsSetPipeline(&breadCtx, ffxGetCommandListDX12(d3d12CommandList),
            ffxGetPipelineDX12(d3d12Pipeline2));
    
        ffxBreadcrumbsBeginMarker(/* ... */);
        d3d12CommandList->DrawIndexedInstanced(/* ... */);
        ffxBreadcrumbsEndMarker(/* ... */);
     
        // ...or disable pipeline at all, it's tracked per single command list so starting new frame will reset this information
        ffxBreadcrumbsSetPipeline(&breadCtx, ffxGetCommandListDX12(d3d12CommandList), nullptr);
    
        // End top grouping call
        ffxBreadcrumbsEndMarker(&breadCtx, ffxGetCommandListDX12(d3d12CommandList));

        // This calls are without pipeline shader info
        ffxBreadcrumbsBeginMarker(/* ... */);
        d3d12CommandList->ClearRenderTargetView(/* ... */);
        d3d12CommandList->DrawIndexedInstanced(/* ... */);
        ffxBreadcrumbsEndMarker(/* ... */);

        /* Execute command list and finish the frame */

        // Retrieve breadcrumbs info when receiving device lost error
        // (check for calls that return DXGI_ERROR_DEVICE_REMOVED for D3D12 or VK_ERROR_DEVICE_LOST for Vulkan)
        HRESULT hr = d3d12Queue->Present(/* ... */);
        if (hr == DXGI_ERROR_DEVICE_REMOVED)
        {
            // Generate text buffer
            FfxBreadcrumbsMarkersStatus markerStatus = {};
            ffxBreadcrumbsPrintStatus(&breadCtx, &markerStatus);

            /* Save pBuffer to a file directly or append it to another crash log (UTF-8 formatted) */
            
            // Free crash log buffer and perform restart or shutdown as desired
            FFX_FREE(markerStatus.pBuffer);
            break;
        }
    }

    /* Wait for gpu to finish all the work before destroying breadcrumbs context */

    // Destroy context and backend buffer during shutdown
    ffxBreadcrumbsContextDestroy(&breadCtx);
    delete[] backendBuffer;
```

<h2>Requirements</h2>

 - At least C++11 compliant compiler.
 - For DirectX 12 backend, SDK and runtime with support for `ID3D12Device3`.
 - For Vulkan backend, any version of SDK installed in the system.
 - For test samples, Windows environment.
