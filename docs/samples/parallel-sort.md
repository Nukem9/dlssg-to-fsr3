<!-- @page page_samples_parallel-sort FidelityFX Parallel Sort -->

<h1>FidelityFX Parallel Sort</h1>

![alt text](media/parallel-sort/parallel-sort_resized.jpg "A screenshot of the FidelityFX Parallel Sort sample.")

This sample demonstrates the use of the FidelityFX Parallel Sort effect.

For details on the technique that underpins the FidelityFX Parallel Sort effect you can refer to the respective [technique documentation](../techniques/parallel-sort.md).

<h2>Requirements</h2>

 - Windows
 - DirectX(R)12
 - Vulkan(R)

<h2>UI elements</h2>

The sample contains various UI elements to help you explore the technique it demonstrates. The table below summarises the UI elements and what they control within the sample.

| Element name               | Value                             | Description                                                                                                                                     |
|----------------------------|-----------------------------------|-------------------------------------------------------------------------------------------------------------------------------------------------|
| **Buffer Resolution**      | `1920x1080, 2560x1440, 3840x2160` | Toggles between the various key buffer sizes to sort through.                                                                                   |
| **Render Sorted Keys**     | `Checked/Unchecked`               | Toggles the output buffer to show in the validation step. Unchecked renders the unsorted buffer, checked renders the image with the sorted keys. |
| **Sort Playload**          | `Checked/Unchecked`               | Toggles whether we are only sorting keys, or also sorting an accompanying payload.                                                              |
| **Use Indirect Execution** | `Checked/Unchecked`               | Toggles between direct and indirect compute execution of the sort algorithm.                                                                    |

<h2>Sample controls and configurations</h2>

For sample controls, configuration, and FidelityFX Cauldron Framework UI element details, see [Running the samples](../getting-started/running-samples.md).

<h2>See also</h2>

- [FidelityFX Parallel Sort](../techniques/parallel-sort.md)
- [FidelityFX Naming guidelines](../getting-started/naming-guidelines.md)
