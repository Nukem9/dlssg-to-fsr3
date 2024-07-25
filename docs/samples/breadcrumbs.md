<!-- @page page_samples_breadcrumbs FidelityFX Breadcrumbs -->

<h1>FidelityFX Breadcrumbs</h1>

This sample demonstrates the use of the AMD FidelityFX Breadcrumbs Library. After few seconds invalid data is passed to the vertex shader resulting
in infinite loop for single vertex, which causes a GPU hang and issuing a device lost error.
After that the crash log is generated and outputed into text file named *breadcrumbs_sample.txt*.

<h2>Requirements</h2>

- Windows
- DirectX(R)12
- Vulkan(R)

For details on the techniques you can refer to the respective [technique documentation](../techniques/breadcrumbs.md).
