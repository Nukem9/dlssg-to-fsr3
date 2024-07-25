<!-- @page page_samples_depth-of-field FidelityFX Depth of Field -->

<h1>FidelityFX Depth of Field</h1>

This sample demonstrates the use of the FidelityFX Depth of Field effect.

For details on the techniques that underpin the Depth of Field effect you can refer to the respective [technique documentation](../techniques/depth-of-field.md).

<h2>Requirements</h2>

 - Windows
 - DirectX(R)12
 - Vulkan(R)

<h2>UI elements</h2>

The sample contains various UI elements to help you explore the technique it demonstrates. The table below summarises the UI elements and what they control within the sample.

| Element name | Value | Description |
|--------------|-------|-------------|
| **Aperture** | `0...0.1` | Simulated camera aperture radius in meters. At zero, a pinhole camera is simulated, which has no depth of field. |
| **Sensor Size** | `0...0.1` | Simulated camera sensor width in meters. This is used along with the field of view for calculating focal length. |
| **Focus Distance** | `Range dynamically adjusted to scene` | Distance from the camera at which objects will be in focus. |
| **Quality** | `1..50` | Maximum number of sample rings used in the blur kernel. |
| **Blur Size Limit** | `0...1` | Maximum blur radius in multiples of y resolution. 0.02 means ~20px at 1080p or ~40px at 4K. |
| **Enable Kernel Ring Merging** | `Checked, Unchecked` | Controls whether the inner rings of the blur kernel may be merged for better performance. |

<h2>Setting up FidelityFX Depth of Field</h2>

The sample contains a [render module](../../samples/dof/dofrendermodule.h) for DOF which manages context lifecycle and dispatch. As it's a post-processing effect it takes only the color and depth buffers as inputs.

The output is produced in-place (in the same color buffer) as an optimization. Before dispatch, the scale and bias parameters for computing the circle of confusion are calculated using helper functions provided in the SDK.

<h2>Sample Controls and Configurations</h2>

For sample controls, configuration, and FidelityFX Cauldron Framework UI element details, see [Running the samples](../getting-started/running-samples.md).

<h2>See also</h2>

- [FidelityFX Depth of Field](../techniques/depth-of-field.md)
- [FidelityFX Naming guidelines](../getting-started/naming-guidelines.md)
