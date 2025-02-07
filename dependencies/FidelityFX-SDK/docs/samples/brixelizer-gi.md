<!-- @page page_samples_brixelizer FidelityFX Brixelizer GI 1.0 -->

<h1>FidelityFX Brixelizer GI</h1>

This sample demonstrates the use of the FidelityFX Brixelizer GI effect.

For details on the techniques that underpin the Brixelizer GI effect you can refer to the respective [technique documentation](../techniques/brixelizer-gi.md). The implementation of the Brixelizer GI sample can be found in the `samples/brixelizergi` directory.

![FidelityFX Brixelizer GI](media/brixelizergi/sample.png)

<h2>Requirements</h2>

`Windows` `DirectX(R)12` `Vulkan(R)`

<h2>UI elements</h2>

Settings in Brixelizer GI are split into the two categories static and dynamic. Static settings refer to settings which must be specified at context creation time, and thus require destroying and re-creating the Brixelizer context. Dynamic settings refer to settings which may be varied per frame. The table below summarises the static settings and what they control within the sample.

| Element name           | Value      | Description |
|------------------------|------------|-------------|
| **Mesh Unit Size**     | `0...1`    | The size in world space units of a Brixelizer brick in the most detailed cascade. To see the effect of this setting clearly, the output type `BRICK_ID` may be selected so that individual bricks are clearly visible as the setting is varied. |
| **Cascade Ratio Size** | `1.1...3` | The ratio of brick sizes between each cascade level. |

The table below summarises the dynamic settings and what they control within the sample.

| Element name                    | Value                                                                      | Description |
|---------------------------------|----------------------------------------------------------------------------|-------------|
| **Output Mode**                 | `None` / `Example Shader` / `Debug Visualization` / `Diffuse GI` / `Specular GI` / `Radiance Cache` / `Irradiance Cache` | Select the output mode of the sample. Values explained below. |
| **Output Type**                 | `DISTANCE` / `UVW` / `ITERATIONS` / `GRADIENT` / `BRICK_ID` / `CASCADE_ID` | Only available with the example shader. Values explained below. |
| **Start Cascade**               | `0...7`                                                                    | The index of the first cascade to use for ray marching. |
| **End Cascade**                 | `0...7`                                                                    | The index of the last cascade to use for ray marching. |
| **SDF Solve Epsilon**           | `0...1`                                                                    | The value used to determine when a hit has been encountered in ray marching. |
| **SDF Center Follow Camera**    | `true` / `false`                                                           | Toggle whether or not to update the center of the SDF as the camera is moved. |
| **TMin**                        | `0...10`                                                                   | The minimum distance to be used in calculating hits. |
| **TMax**                        | `0...10000`                                                                   | The maximum distance to be used in calculating hits. |
| **Ray Pushoff**        | `0...10`         | The distance from a surface along the normal vector to offset the diffuse ray. |
| **Diffuse GI Factor**  | `0...10`         | The factor used for blending diffuse GI in the final output. |
| **Specular GI Factor** | `0...10`         | The factor used for blending specular GI in the final output. |
| **Enable GI**          | `true` / `false` | Toggle whether or not to use Brixelizer GI. If not used, the scene is shown with direct lighting only. |
| **Multi-Bounce**       | `true` / `false` | Toggle whether or not to simulate multi-bounce GI with Brixelizer GI or only do single bounce GI. |
| **Reset Stats**        | Click            | Reset the statistics collected on Brixelizer. Statistics explained below. |

The following table summaries the different output modes available for the sample.

| Value                   | Description |
|-------------------------|-------------|
| **None**                | Draw the scene normally. Whether or not Brixelizer GI is used is toggled with the **Enable GI** control. |
| **Example Shader**      | Show the output of the example shader from the sample demonstrating the underlying Brixelizer SDF. |
| **Debug Visualization** | Show the debug visualization from the Brixelizer GI API. |
| **Diffuse GI**          | Show the diffuse GI calculated by Brixelizer GI. |
| **Specular GI**         | Show the specular GI calculated by Brixelizer GI. |
| **Radiance Cache**      | Show a visualization of the radiance cache calculated by Brixelizer GI. |
| **Irradiance Cache**    | Show a visualization of the irradiance cache calculated by Brixelizer GI. |

The following table summarises the output options available for the example shader. This shader demonstrates the underlying Brixelizer technique and its SDF representation of the scene.

|  Value       | Description |
|--------------|-------------|
| `DISTANCE`   | Display a colour gradient showing the distance to hit. |
| `UVW`        | Display the UVW coordinate of each hit. The UVW coordinates of hits are XYZ positions of a hit relative to the brick origin. |
| `ITERATIONS` | Display a heatmap gradient showing number of iterations before convergence. |
| `GRADIENT`   | Display the normal at each hit. |
| `BRICK_ID`   | Display each brick in a random colour. |
| `CASCADE_ID` | Display the cascade of each hit in a different colour. |

The following table details the statistics collected for Brixelizer. Each statistic has a static and dynamic version corresponding to static and dynamic geometry, which are handled separately by Brixelizer.

| Value              | Description |
|--------------------|-------------|
| **Free Bricks**    | The current number of free bricks available with Brixelizer. |
| **Max Bricks**     | The highest number of brick allocations attempted in a single frame. |
| **Max Triangles**  | The highest number of triangles allocations attempted in a single frame. |
| **Max References** | The highest number of reference allocations attempted in a single frame. |

<h2>Setting up FidelityFX Brixelizer GI</h2>

The sample contains a [render module](../../samples/brixelizergi/brixelizergirendermodule.h) for Brixelizer GI which manages context lifecycle and dispatch.

<h2>Sample Controls and Configurations</h2>

For sample controls, configuration and FidelityFX Cauldron Framework UI element details, please see [Running the samples](../getting-started/running-samples.md)

<h2>See also</h2>

- [FidelityFX Brixelizer](../techniques/brixelizer.md)
- [FidelityFX Brixelizer GI](../techniques/brixelizer-gi.md)
- [FidelityFX Naming guidelines](../getting-started/naming-guidelines.md)
