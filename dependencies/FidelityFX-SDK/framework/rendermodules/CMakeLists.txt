# This file is part of the FidelityFX SDK.
# 
# Copyright (C) 2024 Advanced Micro Devices, Inc.
# 
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files(the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions :
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

project (RenderModules)

set(registry_src rendermoduleregistry.h rendermoduleregistry.cpp)

# ---------------------------------------------
# Cauldron render modules
# ---------------------------------------------

# GBuffer
file(GLOB gbuffer_src
    ${CMAKE_CURRENT_SOURCE_DIR}/gbuffer/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gbuffer/*.cpp)
set(gbuffer_config ${CMAKE_CURRENT_SOURCE_DIR}/gbuffer/config/gbufferrendermodule.json)
file(GLOB gbuffer_shaders
    ${CMAKE_CURRENT_SOURCE_DIR}/gbuffer/shaders/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gbuffer/shaders/*.hlsl)
set_source_files_properties(${gbuffer_shaders} PROPERTIES VS_TOOL_OVERRIDE "Text")
copyCommand("${gbuffer_shaders}" ${SHADER_OUTPUT})

# Animated textures
file(GLOB animatedtextures_src
    ${CMAKE_CURRENT_SOURCE_DIR}/animatedtextures/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/animatedtextures/*.cpp)
file(GLOB animatedtextures_shaders
    ${CMAKE_CURRENT_SOURCE_DIR}/animatedtextures/shaders/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/animatedtextures/shaders/*.hlsl)
set_source_files_properties(${animatedtextures_shaders} PROPERTIES VS_TOOL_OVERRIDE "Text")
copyCommand("${animatedtextures_shaders}" ${SHADER_OUTPUT})


# Translucency
file(GLOB translucency_src
    ${CMAKE_CURRENT_SOURCE_DIR}/translucency/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/translucency/*.cpp)
set(translucency_config ${CMAKE_CURRENT_SOURCE_DIR}/translucency/config/translucencyrendermodule.json)
file(GLOB translucency_shaders
    ${CMAKE_CURRENT_SOURCE_DIR}/translucency/shaders/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/translucency/shaders/*.hlsl)
set_source_files_properties(${translucency_shaders} PROPERTIES VS_TOOL_OVERRIDE "Text")
copyCommand("${translucency_shaders}" ${SHADER_OUTPUT})

# Lighting
file(GLOB lighting_src
    ${CMAKE_CURRENT_SOURCE_DIR}/lighting/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/lighting/*.cpp)
file(GLOB lighting_shaders
    ${CMAKE_CURRENT_SOURCE_DIR}/lighting/shaders/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/lighting/shaders/*.hlsl)
set_source_files_properties(${lighting_shaders} PROPERTIES VS_TOOL_OVERRIDE "Text")
copyCommand("${lighting_shaders}" ${SHADER_OUTPUT})

# RasterShadow
file(GLOB raster_shadow_src
    ${CMAKE_CURRENT_SOURCE_DIR}/rastershadow/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/rastershadow/*.cpp)
set(raster_shadow_config ${CMAKE_CURRENT_SOURCE_DIR}/rastershadow/config/rastershadowrendermodule.json)
file(GLOB raster_shadow_shaders
    ${CMAKE_CURRENT_SOURCE_DIR}/rastershadow/shaders/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/rastershadow/shaders/*.hlsl)
set_source_files_properties(${raster_shadow_shaders} PROPERTIES VS_TOOL_OVERRIDE "Text")
copyCommand("${raster_shadow_shaders}" ${SHADER_OUTPUT})

# Skydome
file(GLOB skydome_src
    ${CMAKE_CURRENT_SOURCE_DIR}/skydome/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/skydome/*.cpp)
set(skydome_config ${CMAKE_CURRENT_SOURCE_DIR}/skydome/config/skydomerendermodule.json)
file(GLOB skydome_shaders
    ${CMAKE_CURRENT_SOURCE_DIR}/skydome/shaders/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/skydome/shaders/*.hlsl)
set_source_files_properties(${skydome_shaders} PROPERTIES VS_TOOL_OVERRIDE "Text")
copyCommand("${skydome_shaders}" ${SHADER_OUTPUT})

# TAA
file(GLOB taa_src
    ${CMAKE_CURRENT_SOURCE_DIR}/taa/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/taa/*.cpp)
set(taa_config ${CMAKE_CURRENT_SOURCE_DIR}/taa/config/taarendermodule.json)
file(GLOB taa_shaders
    ${CMAKE_CURRENT_SOURCE_DIR}/taa/shaders/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/taa/shaders/*.hlsl)
set_source_files_properties(${taa_shaders} PROPERTIES VS_TOOL_OVERRIDE "Text")
copyCommand("${taa_shaders}" ${SHADER_OUTPUT})

# GPU particles
file(GLOB gpuparticle_src
    ${CMAKE_CURRENT_SOURCE_DIR}/gpuparticle/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gpuparticle/*.cpp)
file(GLOB gpuparticle_shaders
    ${CMAKE_CURRENT_SOURCE_DIR}/gpuparticle/shaders/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/gpuparticle/shaders/*.hlsl)
set_source_files_properties(${gpuparticle_shaders} PROPERTIES VS_TOOL_OVERRIDE "Text")
copyCommand("${gpuparticle_shaders}" ${SHADER_OUTPUT})

# Copy config files to output directory
file(GLOB configFiles
    ${gbuffer_config}
    ${translucency_config}
	${raster_shadow_config}
    ${skydome_config}
    ${taa_config}
)
copyCommand("${configFiles}" ${CONFIG_OUTPUT}/rm_configs)

add_library (RenderModules STATIC
                        ${registry_src}
						# Cauldron rendermodules
                        ${gbuffer_src} ${gbuffer_config} ${gbuffer_shaders}
						${animatedtextures_src} ${animatedtextures_config} ${animatedtextures_shaders}
                        ${translucency_src} ${translucency_config} ${translucency_shaders}
                        ${lighting_src} ${lighting_shaders}
                        ${limiter_src} ${limiter_shaders}
                        ${raster_shadow_src} ${raster_shadow_config} ${raster_shadow_shaders}
                        ${skydome_src} ${skydome_config} ${skydome_shaders}
                        ${taa_src} ${taa_config} ${taa_shaders}
						${gpuparticle_src} ${gpuparticle_shaders})

target_link_libraries(RenderModules LINK_PUBLIC Framework)# ffx_core_${CMAKE_GENERATOR_PLATFORM})
add_dependencies(RenderModules Framework)

#add_dependencies(RenderModules ffx_core_${CMAKE_GENERATOR_PLATFORM})

source_group(""								FILES ${registry_src})
# Cauldron rendermodules
source_group("GBuffer"						FILES ${gbuffer_src})
source_group("GBuffer\\Config"				FILES ${gbuffer_config})
source_group("Animated Textures"			FILES ${animatedtextures_src})
source_group("Animated Textures\\Shaders"	FILES ${animatedtextures_shaders})

source_group("GBuffer\\Shaders"				FILES ${gbuffer_shaders})
source_group("GPUParticle"					FILES ${gpuparticle_src})
source_group("GPUParticle\\Shaders"			FILES ${gpuparticle_shaders})
source_group("Translucency"					FILES ${translucency_src})
source_group("Translucency\\Config"			FILES ${translucency_config})
source_group("Translucency\\Shaders" 		FILES ${translucency_shaders})
source_group("Lighting"						FILES ${lighting_src})
source_group("Lighting\\Shaders"			FILES ${lighting_shaders})
source_group("FPSLimiter"					FILES ${limiter_src})
source_group("FPSLimiter\\Shaders"			FILES ${limiter_shaders})
source_group("RasterShadow"					FILES ${raster_shadow_src})
source_group("RasterShadow\\Config"			FILES ${raster_shadow_config})
source_group("RasterShadow\\Shaders"		FILES ${raster_shadow_shaders})
source_group("Skydome"						FILES ${skydome_src})
source_group("Skydome\\Config"				FILES ${skydome_config})
source_group("Skydome\\Shaders"				FILES ${skydome_shaders})
source_group("TAA"							FILES ${taa_src})
source_group("TAA\\Config"					FILES ${taa_config})
source_group("TAA\\Shaders"					FILES ${taa_shaders})
