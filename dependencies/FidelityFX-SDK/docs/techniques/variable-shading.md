<!-- @page page_techniques_variable-shading FidelityFX Variable Shading 1.2 -->

<h1>FidelityFX Variable Shading 1.2</h1>

<h2>Introduction</h2>

One of the major performance bottlenecks, when rendering a frame in modern games, is the high computational power required to shade every pixel at high resolutions. A side effect of rendering at high resolution is that the world space position of neighboring fragments within the same primitive are typically quite close, and the color value resulting from adjacent pixel shader executions in the same primitive often produces visibly similar results.

Geometry edges typically contain more important visual information than the insides of primitives. Multi-sample anti-aliasing (MSAA) takes advantage of this by keeping the number of pixel shader executions low, relative to the increase in fragments used to compute the final frame.

FidelityFX Variable Shading (VRS) provides a similar way to reduce the number of pixel shader executions and reused the same output value for multiple neighboring pixels, if they are covered by the same primitive. This can significantly reduce the amount of ALU and bandwidth required to compute a frame and improve frame rate without a noticeable loss of visual quality, which means the GPU time saved can then be repurposed for effects that have a bigger impact on the final frame, if you have room in your frame budget.

One way to think about how VRS works is to think of MSAA: if a 4K frame is rendered with 2x2 VRS for everything, the required computational power and the final quality of the image would be comparable to rendering a 1080p image with 4x MSAA enabled. However, VRS provides additional features to enable more fine-grained control over which parts of the frame should get rendered with lower resolution.

<h2>Implementing Variable Shading</h2>

<h3>Cases allowing VRS</h3>

Multiple cases exist where VRS can be applied without significantly impacting the quality of the final
image:

- Distant objects are usually more defined by their geometry, rather than by texture detail (using lower MIP levels) and color variance of distant pixels is often further reduced by fog, atmospheric effects or semi-transparent geometry (e.g. particle systems).
- Some objects may be known to be out of focus or known to be blurred by postprocessing effects like motion blur or bloom.
- Some objects may get distorted or blurred by being behind haze or semi-transparent objects like water or frosted glass.
- Some objects might be known to have little detail variance (such as rendering for toon shaded games) or due to being in very dark parts of the scene (e.g. the unlit or shadowed parts of objects in scenes with little ambient light).
- In fast moving scenes, high framerate and low input lag are important, but small details are less likely to get noticed by the player, so aggressively using VRS can help to achieve the performance goals.

In addition to the cases mentioned above, some pixels might be known to be of little interest to the player, either through eye-tracking (foveated rendering) or other systems, or because the game design aims to steer the focus of the player to certain parts of the screen. As an example, the game might choose to reduce the shading rate on the background geometry but make sure all enemies are rendered at highest quality.

Due to saving computational power by focusing usage of GPU resources where it matters most, VRS can be used to make sure target frame times are achieved (similar to dynamic resolution scaling but with more fine-grained control over where detail needs to be preserved), as well as for power saving on portable devices without noticeably sacrificing image quality.

<h3>VRS control options</h3>

VRS support in DirectX12 comes in 2 tiers:

- Tier 1 allows setting a shading rate per draw call. This way the shading rate can be adjusted based on object type (importance), distance to camera, movement speed or screen space position of the object.
- Tier 2 adds two additional techniques for more fine-grained control over the shading rate:
    - SV_ShadingRate can be exported from the vertex or geometry shader to control shading rate at a per-primitive level. This way, for example, primitives facing away from the main light source, fast moving parts of a skinned mesh, or simply based on artist specification in a vertex attribute, can get instructed to be shaded at a lower rate.
    - A VRS control image containing information which VRS rate to use on a per screen tile basis, can be bound to specify a VRS rate per screen region. This can be used to control the shading independent of the geometric granularity of the objects rendered, based on screen region, user focus area, or by analyzing the previous frame’s final back buffer to compute which regions of the screen are likely not to suffer much from reduced shading rates.

All three modes can be used simultaneously: Tier 2 defines a combiner tree which determines how the result of each state should be combined with the result of the previous state to compute the final shading rate. The options for each combiner are: passthrough the previous state (i.e. disable the current stage), override (ignore previous stages), min, max and sum.

<h2>Technical details</h2>

VRS works similarly to MSAA, where the pixel shader usually gets executed once per pixel and the resulting value gets written to all fragments of a pixel which are covered by the primitive. VRS executes one pixel shader thread per VRS coarse pixel, which can contain 1, 2 or 4 pixels (or up to 16 if additional shading rates are supported), and then writes the result to all pixels within the coarse pixel region that are covered by the primitive.

This can significantly improve performance in cases where the scene mainly consists of large polygons using expensive pixel shaders. Performance for draw calls containing lots of very small primitives will profit less from VRS, but at the same time visual quality will not be impacted. Also, since VRS does not reduce the amount of pixels which get written to the rendertargets, fillrate will not get reduced by enabling VRS, so using VRS in fillrate bound passes is not recommended.

Likewise, since depth and stencil values are usually computed and written before the pixel shader gets executed. In most cases depth-only passes do not have a pixel shader, or at least not an expensive one, so depth-only passes (like shadow map rendering) will not profit from VRS.

When working with depth values and VRS, it's important to note that the sample position of a pixel shader may be different than the pixel centre for which the depth value got stored in the depth buffer. When rendering geometry that reads the depth buffer, like decals or soft particles, it has to be taken into account that the difference between values read from the depth buffer, and the depth value of the pixel passed from the vertex shader, may be considerably larger than without VRS enabled.

Finally, since the centre of the VRS tile may be outside the rendered geometry, centroid interpolation should be enabled in the pixel shader when using VRS, otherwise this can cause artefacts like sampling outside the region of a texture map that is intended to be used for a primitive. Using centroid interpolation will ensure the barycentric coordinates, used to interpolate vertex attributes, are always within the area covered by the primitive.

Should the information on which pixels of a VRS tile are covered by the geometry be required in the pixel shader, e.g. for alpha-tested geometry, they can be evaluated by reading `SV_Coverage`. Exporting the coverage mask from the pixel shader is also possible, however it is not recommended for performance reasons.

<h2>See also</h2>

- [FidelityFX Variable Shading](../samples/variable-shading.md)
- [FidelityFX Naming guidelines](../getting-started/naming-guidelines.md)
