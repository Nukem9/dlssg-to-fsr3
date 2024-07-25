# SPIRV-Reflect

SPIRV-Reflect is a lightweight library that provides a C/C++ reflection API for
[SPIR-V](https://www.khronos.org/spir/) shader bytecode in
[Vulkan](https://www.khronos.org/vulkan/) applications.

SPIRV-Reflect has been tested on Linux and Windows.

## Features

- Extract descriptor bindings from SPIR-V bytecode, to assist in the generation of
  Vulkan descriptor set and pipeline layouts.
- Extract push constant block size from SPIR-V bytecode to assist in the generation
  of pipeline layouts.
- Extract full layout data for uniform buffer and push constant blocks from SPIR-V
  bytecode, to assist in application updates of these structures.
- Extract input/output variables from SPIR-V bytecode (including semantic decorations
  for HLSL shaders), to assist in validation of pipeline input/output settings.
- Remap descriptor bindings at runtime, and update the source SPIR-V bytecode
  accordingly.
- Log all reflection data as human-readable text.

## Integration

SPIRV-Reflect is designed to make integration as easy as possible. The only
external dependency is the [Vulkan SDK](https://vulkan.lunarg.com/sdk/home).

To integrate SPIRV-Reflect into a project, simply add `spirv_reflect.h` and
`spirv_reflect.c` in the project's build, and include `spirv_reflect.h` from
the necessary source files.

## Building Samples

**This step is only necessary when building/running SPIRV-Reflect's example applications.**

SPIRV-Reflect includes a collection of sample programs in the `examples/` directory which
demonstrate various use cases:

- **descriptors**: This sample demonstrates the retrieval of descriptor bindings, including
  the population of `VkDescriptorSetLayoutCreateInfo` structures from reflection data.
- **hlsl_resource_types**: This sample shows how various HLSL resource types are represented
  in SPIR-V.
- **io_variables**: This sample demonstrates the retrieval of input/output variables, including
  the population of `VkPipelineVertexInputStateCreateInfo` structures from reflection data.

To build the included sample applications, use [CMake](https://cmake.org/) to generate the
appropriate project files for your platform, then build them as usual.

Note that you can set VulkanSDK directory as your preference. For example, on Linux:
```
VULKAN_SDK=$HOME/VulkanSDK/1.1.70.1/x86_64 cmake -G Ninja  ..
```


## Usage

SPIRV-Reflect's core C API should be familiar to Vulkan developers:

```c++
#include "spirv_reflect.h"

int SpirvReflectExample(const void* spirv_code, size_t spirv_nbytes)
{
  // Generate reflection data for a shader
  SpvReflectShaderModule module;
  SpvReflectResult result = spvReflectCreateShaderModule(spirv_nbytes, spirv_code, &module);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  // Enumerate and extract shader's input variables
  uint32_t var_count = 0;
  result = spvReflectEnumerateInputVariables(&module, &var_count, NULL);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);
  SpvReflectInterfaceVariable** input_vars =
    (SpvReflectInterfaceVariable**)malloc(var_count * sizeof(SpvReflectInterfaceVariable*));
  result = spvReflectEnumerateInputVariables(&module, &var_count, input_vars);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  // Output variables, descriptor bindings, descriptor sets, and push constants
  // can be enumerated and extracted using a similar mechanism.

  // Destroy the reflection data when no longer required.
  spvReflectDestroyShaderModule(&module);
}
```

A C++ wrapper is also provided.

## Building Self-Test Suite

SPIRV-Reflect uses [googletest](https://github.com/google/googletest) for self-testing.
This component is optional, and generally only of interest to developers modifying SPIRV-Reflect.
To run the self-tests:

- `git submodule init`
- `git submodule update`
- Enable `SPIRV_REFLECT_BUILD_TESTS` in CMake
- Build and run the `test-spirv-reflect` project.

## Bazel build

We tested the following bazel build command using Linux Bazel 1.2.1.
```
bazel build :all
```

## License

Copyright 2017-2018 Google Inc.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

[http://www.apache.org/licenses/LICENSE-2.0](http://www.apache.org/licenses/LICENSE-2.0)

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
