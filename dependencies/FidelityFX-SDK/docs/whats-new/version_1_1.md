<!-- @page page_whats-new_index AMD FidelityFX SDK: What's new in FidelityFX SDK 1.1 -->

<h1>What's new in the AMD FidelityFX™ SDK 1.1?</h1>

Welcome to the AMD FidelityFX SDK. This updated version of the SDK contains various fixes, support for the Microsoft® GDK (available from Microsoft through the GDK), and 3 new effects in the FidelityFX family.

<h2>New effects and features</h2>

<h3>AMD FidelityFX Breadcrumbs library</h3>

The first release of a library to help detect sources of GPU crashes. With a breadcrumb markers technique you can track down where your submitted commands cause GPU crashes and save time when analysing the rendering process in search of faulty commands.

<h3>AMD FidelityFX Brixelizer</h3>

A highly-optimized GPU sparse distance field builder that can handle both static and dynamic geometry and features a shader API that allows you to trace rays against the scene from any shader of your choice.  

<h3>AMD FidelityFX Brixelizer GI</h3>

A compute-based real-time dynamic global illumination solution built upon the sparse distance fields built by FidelityFX Brixelizer. It provides you with denoised indirect diffuse and specular lighting outputs that you can composite into your final lighting output and is ideal as a fallback for ray-traced global illumination on lower end hardware.

<h3>AMD FidelityFX Super Resolution 3 (FSR 3)</h3>

AMD FidelityFX FSR 3.1 has been added to the FidelityFX FSR sample. FSR 3.1 contains significant updates:
* FSR 3.1 Upscaler reduces ghosting, improves convergence speed and reduces flickering. 
* Frame pacing for FSR Frame Interpolation has been improved for an even better experience.
* FSR3.1 adds the ability to decouple the frame interpolation process from that of upscaling, so it can be used with any upscaler the user desires, or at native resolution.
* FSR 3.1 introduces the FSR API, which makes integrations of FSR future proof, so applications using this interface can be updated to upcoming versions of FSR more easily.

The sample also includes support for dynamic resolution scaling cases of upscaler/frame interpolation usage.

FSR 3.1 also includes a number of fixes for issues discovered with FSR 3.0.2 and 3.0.4.

<h3>AMD FidelityFX Super Resolution (FSR) API</h3>

A new API has been added for forward compatibility.

Major differences to the previous API for FSR:

* Only has 5 functions with most information exchanged through structures with the ability to add extensions.
* Requires linking to a single FidelityFX DLL, pre-built and signed by AMD.
* Backend contexts are created by the API.
  * Custom backends are no longer supported. Only the native DirectX 12 and Vulkan backends are available through this API.

<h3>AMD FidelityFX GDK Backend</h3>

The FidelityFX SDK now supports compiling natively to the Microsoft GDK for desktop and Xbox Series X/S. In order to use the GDK backend, please see the FidelityFX samples shipped with the latest version of Microsoft's GDK.

<h2>Updated effects</h2>

<h3>AMD FidelityFX Hybrid Shadows</h3>

* Fixed memory leak issue.
* General cleanup of the shadow classifier.
* Fix Hybrid Shadows issues on RX 7900 XTX.

<h3>AMD FidelityFX Hybrid Reflections</h3>

* Various fixes and improvements.

<h3>AMD FidelityFX DOF</h3>

* Fixes and improvements to a few corner cases.

<h3>AMD FidelityFX Backend Updates</h3>

* Fixed issue with resources needing data initialization.
* Added marker support to DX12/GDK backends.
* Fixes on 'wave64' device checks.
* Fixed debugging issue with shader compiler tool.
* Vulkan performance improvements.
* Added options for DLL-linked samples.
* Resource binding improvements and additions for bindless support.
* Added RelWithDebInfo configuration.
* Added version checks to safeguard against effect/backend mismatches.

<h3>Graphics Framework Updates</h3>

* Fixed issue with markers not being visible in PIX for release builds.
* Fixes to rigid body animation.
* Support for skeletal animation.
* Fixed numerous validation warnings (Vulkan® and DirectX® 12).
* Dynamic scaling on scene displacement.
* Fixes to shader PDB generation through FidelityFX Shader Compiler.
* Added support for auto-exposure.
* Clean up of GPU particle code to make better use of FidelityFX Parallel Sort.
* Optimization to sky dome rendering.
* Addition of automatic IBL generation for procedural skydomes.
* Various HDR and tonemapper fixes/cleanup.
* Fixed shadow rasterization issue.
* Updated media delivery bundle with new sample content.
* New UI component support.
* Added animated texture render module.
* Better logging for content loading.
* Vulkan performance improvements.
* Fixed a particle rendering issue.
* Fixes for GLTF specular roughness materials.
* Fixed memory leak in the task manager.
* Fixed Vulkan swapchain creation issues on some NVIDIA® cards.
* Added RelWithDebInfo configuration.
* Added version checks to safeguard against sample/effect/backend mismatches.

<h2>Updated documentation</h2>

* Various fixes to layout and punctuation.
* Added missing image reference for Parallel Sort.
* Fixed incorrect information in FSR 2 documentation.

<h2>Deprecated effects</h2>

None.

<h2>Deprecated components</h2>

None.


<!-- - @subpage page_whats-new_index_1_0 "AMD FidelityFX SDK: What's new in FidelityFX SDK 1.0" -->

