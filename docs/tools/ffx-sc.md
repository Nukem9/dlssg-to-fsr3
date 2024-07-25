<!-- @page page_tools_ffx-sc FidelityFX Shader Compiler -->

<h1>FidelityFX Shader Compiler</h1>

The FidelityFX SDK Shader Compiler tool is distributed as part of the FidelityFX SDK package and is the command line tool used by the SDK to pre-generate all shader permutation binaries that are loaded at runtime.

<h2>Using the Shader Compiler</h2>

The current version of the Shader Compiler and its needed libraries/dlls are kept in the /sdk/tools/binary_store folder.

To use it, simply invoke it from your build process (see the existing backend `CMakeList.txt` files as an example of launching it as part of the build process).

The following arguments will allow you to control how compilation occurs:

<h3>Command line syntax:</h3>

`FidelityFX_SC.exe [Options] <InputFile>`

**Options**

| Option                                                  | Descriptions                                                                                                                                                        |  
|---------------------------------------------------------|---------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| **&lt;CompilerArgs&gt;**                                | A list of arguments accepted by the target compiler, separated by spaces.                                                                                           |
| **-output=\<Path\>**                                    | Path to where the shader permutations should be output to.                                                                                                          |
| **-D\<Name\>**                                          | Define a macro that is defined in all shader permutations.                                                                                                          |
| **-D\<Name\>={\<Value1\>, \<Value2\>, \<Value3\> ...}** | Declare a shader option that will generate permutations with the macro defined using the given values. Use a '-' to define a permutation where no macro is defined. |
| **-num-threads=\<Num\>**                                | Number of threads to use for generating shaders. Sets to the max number of threads available on the current CPU by default.                                         |
| **-name=\<Name\>**                                      | The name used for prefixing variables in the generated headers. Uses the file name by default.                                                                      |
| **-reflection**                                         | Generate header containing reflection data.                                                                                                                         |
| **-embed-arguments**                                    | Write the compile arguments used for each permutation into their respective headers.                                                                                |
| **-print-arguments**                                    | Print the compile arguments used for each permuations.                                                                                                              |
| **-disable-logs**                                       | Prevent logging of compile warnings and errors.                                                                                                                     |
| **-compiler=\<Compiler\>**                              | Select the compiler to generate permutations from (`dxc`, `fxc` or `glslang`).                                                                                      |
| **-dxcdll=\<DXC DLL Path\>**                            | Path to the dxccompiler dll to use.                                                                                                                                 |
| **-d3ddll=\<D3D DLL Path\>**                            | Path to the `d3dcompiler` dll to use.                                                                                                                               |
| **-glslangexe=\<glslangValidator.exe Path\>**           | Path to the `glslangValidator` executable to use.                                                                                                                   |
| **-deps=\<Format\>**                                    | Dump depfile which recorded the include file dependencies in format of (`gcc` or `msvc`).                                                                           |
| **-debugcompile**                                       | Compile shader with debug information.                                                                                                                              |
| **-debugcmdline**                                       | Print all the input arguments.                                                                                                                                      |

  
<h2>Modifying the Shader Compiler</h2>

Should the need arise to build and/or modify the shader compiler tool, a solution can be generated by navigating to `/sdk/tools/ffx_shader_compiler/` sub-folder and launching `GenerateSolution.bat`. This will in turn create a solution for the shader compiler in an `/build` subfolder.

When building a new shader compiler, the output will be sent to `/sdk/tools/ffx_shader_compiler/bin/` sub-folder in a release or debug folder (based on configuration built). In order to use the newly compiled tool, it needs to have all binary files copied from the binary output location (`bin` directory) to the `binary_store` directory.
