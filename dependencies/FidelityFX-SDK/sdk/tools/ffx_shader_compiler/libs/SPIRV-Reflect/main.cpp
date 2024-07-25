/*
 Copyright 2017-2018 Google Inc.
 
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at
 
 http://www.apache.org/licenses/LICENSE-2.0
 
 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
*/

#if defined(WIN32)
  #define _CRTDBG_MAP_ALLOC  
  #include <stdlib.h>
  #include <crtdbg.h>
#endif

#include <cassert>
#include <cstring>
#include <iostream>
#include <fstream>

#include "spirv_reflect.h"
#include "examples/arg_parser.h"
#include "common/output_stream.h"


// =================================================================================================
// PrintUsage()
// =================================================================================================
void PrintUsage()
{
  std::cout
    << "Usage: spirv-reflect [OPTIONS] path/to/SPIR-V/bytecode.spv" << std::endl
    << "Prints a summary of the reflection data extracted from SPIR-V bytecode." << std::endl
    << "Options:" << std::endl
    << " --help                   Display this message" << std::endl
    << " -y,--yaml                Format output as YAML. [default: disabled]" << std::endl
    << " -v VERBOSITY             Specify output verbosity (YAML output only):" << std::endl
    << "                          0: shader info, block variables, interface variables," << std::endl
    << "                             descriptor bindings. No type descriptions. [default]" << std::endl
    << "                          1: Everything above, plus type descriptions." << std::endl
    << "                          2: Everything above, plus SPIR-V bytecode and all internal" << std::endl
    << "                             type descriptions. If you're not working on SPIRV-Reflect" << std::endl
    << "                             itself, you probably don't want this." << std::endl
    << "-e,--entrypoint           Prints the entry point found in shader module." << std::endl
    << "-s,--stage                Prints the Vulkan shader stage found in shader module." << std::endl
    << "-f,--file                 Prints the source file found in shader module." << std::endl
    << "-fcb,--flatten_cbuffers   Flatten constant buffers on non-YAML output." << std::endl;
}

// =================================================================================================
// main()
// =================================================================================================
int main(int argn, char** argv)
{
  ArgParser arg_parser;
  arg_parser.AddFlag("h", "help", "");
  arg_parser.AddFlag("y", "yaml", "");
  arg_parser.AddOptionInt("v", "verbosity", "", 0);
  arg_parser.AddFlag("e", "entrypoint", "");
  arg_parser.AddFlag("s", "stage", "");
  arg_parser.AddFlag("f", "file", "");
  arg_parser.AddFlag("fcb", "flatten_cbuffers", "");
  if (!arg_parser.Parse(argn, argv, std::cerr)) {
    PrintUsage();
    return EXIT_FAILURE;
  }

  if (arg_parser.GetFlag("h", "help")) {
    PrintUsage();
    return EXIT_SUCCESS;
  }

  bool output_as_yaml = arg_parser.GetFlag("y", "yaml");

  int yaml_verbosity = 0;
  arg_parser.GetInt("v", "verbosity", &yaml_verbosity);

	bool print_entry_point = arg_parser.GetFlag("e", "entrypoint");
  bool print_shader_stage = arg_parser.GetFlag("s", "stage");
  bool print_source_file = arg_parser.GetFlag("f", "file");
  bool flatten_cbuffers = arg_parser.GetFlag("fcb", "flatten_cbuffers");

	std::string input_spv_path;
	if (!arg_parser.GetArg(0, &input_spv_path)) {
    std::cerr << "ERROR: no SPIR-V file specified" << std::endl;
    return EXIT_FAILURE;
	}
			
  std::ifstream spv_ifstream(input_spv_path.c_str(), std::ios::binary);
  if (!spv_ifstream.is_open()) {
    std::cerr << "ERROR: could not open '" << input_spv_path << "' for reading" << std::endl;
    return EXIT_FAILURE;
  }

  spv_ifstream.seekg(0, std::ios::end);
  size_t size = static_cast<size_t>(spv_ifstream.tellg());
  spv_ifstream.seekg(0, std::ios::beg);

  {
    std::vector<char> spv_data(size);
    spv_ifstream.read(spv_data.data(), size);

    spv_reflect::ShaderModule reflection(spv_data.size(), spv_data.data());
    if (reflection.GetResult() != SPV_REFLECT_RESULT_SUCCESS) {
      std::cerr << "ERROR: could not process '" << input_spv_path
        << "' (is it a valid SPIR-V bytecode?)" << std::endl;
      return EXIT_FAILURE;
    }

		if (print_entry_point || print_shader_stage || print_source_file) {
      size_t printed_count = 0;
      if (print_entry_point) {
        std::cout << reflection.GetEntryPointName();
        ++printed_count;
      }

      if (print_shader_stage) {
        if (printed_count > 0) {
          std::cout << ";";
        }
        std::cout << ToStringShaderStage(reflection.GetShaderStage());
      }

      if (print_source_file) {
        if (printed_count > 0) {
          std::cout << ";";
        }
        std::cout << (reflection.GetSourceFile() != NULL ? reflection.GetSourceFile() : "");
      }

      std::cout << std::endl;
		}
		else {
			if (output_as_yaml) {
				SpvReflectToYaml yamlizer(reflection.GetShaderModule(), yaml_verbosity);
				std::cout << yamlizer;
			} else {
        WriteReflection(reflection, flatten_cbuffers, std::cout);
        std::cout << std::endl;
				std::cout << std::endl;
			}
		}
  }

  return EXIT_SUCCESS;
}
