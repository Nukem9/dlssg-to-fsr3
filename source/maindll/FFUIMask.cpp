//
// This file is a highly modified derivation of ffx_fsr3upscaler.cpp from AMD's FidelityFX-FSR SDK,
// which is licensed under the MIT license. See /src/components/fsr3upscaler/ffx_fsr3upscaler.cpp for
// the original source code.
//
// This file is licensed under a compatible license found in LICENSE.md in the root directory of this
// project.
//
#include "FFExt.h"
#include "FFUIMask.h"

#if 0 // Experimental/debugging purposes

//FfxCreateResourceDescription FFFrameInterpolator::GetScratchHUDLessResourceDescription(const FfxResourceDescription& InputColorDesc)
//{
//	return FfxCreateResourceDescription
//	{
//		FFX_HEAP_TYPE_DEFAULT,
//		{
//			InputColorDesc.type, InputColorDesc.format,
//			InputColorDesc.width, InputColorDesc.height,
//			1, 1, FFX_RESOURCE_FLAGS_ALIASABLE,
//			(FfxResourceUsage)(FFX_RESOURCE_USAGE_UAV)
//		},
//		FFX_RESOURCE_STATE_UNORDERED_ACCESS,
//		0, nullptr,
//		L"FFFRAMEINTERPOLATOR_ScratchHUDLess",
//		0,
//	};
//}

FFUIMask::FFUIMask(const FfxInterface& BackendInterface)
	: m_BackendInterface(BackendInterface)
{
	// Initialize a private backend instance
	FFX_THROW_ON_FAIL(m_BackendInterface.fpCreateBackendContext(&m_BackendInterface, &m_EffectContextId));
}

FFUIMask::~FFUIMask()
{
	for (auto& pipeline : m_DispatchPipelineStates)
		m_BackendInterface.fpDestroyPipeline(&m_BackendInterface, &pipeline.second, m_EffectContextId);

	m_DispatchPipelineStates.clear();
	m_BackendInterface.fpDestroyBackendContext(&m_BackendInterface, m_EffectContextId);
}

// clang-format off
FfxErrorCode FFUIMask::Dispatch(const FFUIMaskDispatchParameters& Parameters)
{
	auto registerResource = [&](const FfxResource& Resource, uint32_t Index, bool UAV)
	{
		m_BackendInterface.fpRegisterResource(&m_BackendInterface, &Resource, m_EffectContextId, UAV ? &m_UAVResources[Index] : &m_SRVResources[Index]);
	};

	registerResource(Parameters.InputColor, ResourceIndex::InputColor, false);
	registerResource(Parameters.InputUIMask, ResourceIndex::InputUIMask, false);
	registerResource(Parameters.OutputHUDLessColor, ResourceIndex::OutputHUDLessColor, true);

	// Update constants. Many can be skipped since it's a custom shader
	FrameInterpolationConstants constants = {};
	UpdateConstantBuffers(Parameters, constants);

	// Determine hardcoded dispatch dimensions
	const uint32_t threadGroupWorkRegionDim = 8;
	const uint32_t dispatchSrcX = (constants.renderSize[0] + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
	const uint32_t dispatchSrcY = (constants.renderSize[1] + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;

	FFX_RETURN_ON_FAIL(ScheduleComputeDispatch(GetPipelineStateForParameters(Parameters), dispatchSrcX, dispatchSrcY, 1));

	// Finally append our calls to the D3D12 command list and release 
	FFX_RETURN_ON_FAIL(m_BackendInterface.fpExecuteGpuJobs(&m_BackendInterface, Parameters.CommandList));
	FFX_RETURN_ON_FAIL(m_BackendInterface.fpUnregisterResources(&m_BackendInterface, Parameters.CommandList, m_EffectContextId));

	return FFX_OK;
}

void FFUIMask::UpdateConstantBuffers(const FFUIMaskDispatchParameters& Parameters, FrameInterpolationConstants& Constants)
{
	Constants.renderSize[0] = Parameters.RenderSize.width;
	Constants.renderSize[1] = Parameters.RenderSize.height;
	Constants.displaySize[0] = Parameters.RenderSize.width;
	Constants.displaySize[1] = Parameters.RenderSize.height;
	Constants.displaySizeRcp[0] = 1.0f / Parameters.RenderSize.width;
	Constants.displaySizeRcp[1] = 1.0f / Parameters.RenderSize.height;
	Constants.upscalerTargetSize[0] = Parameters.RenderSize.width;
	Constants.upscalerTargetSize[1] = Parameters.RenderSize.height;
	Constants.maxRenderSize[0] = Parameters.RenderSize.width;
	Constants.maxRenderSize[1] = Parameters.RenderSize.height;

	Constants.backBufferTransferFunction = Parameters.HDR ? FFX_BACKBUFFER_TRANSFER_FUNCTION_PQ : FFX_BACKBUFFER_TRANSFER_FUNCTION_SRGB;
	Constants.minMaxLuminance[0] = Parameters.MinMaxLuminance.x;
	Constants.minMaxLuminance[1] = Parameters.MinMaxLuminance.y;

	m_CurrentConstants = Constants;
	memcpy(&m_DispatchConstantBuffer.data, &m_CurrentConstants, m_DispatchConstantBuffer.num32BitEntries * sizeof(uint32_t));
}

FfxErrorCode FFUIMask::ScheduleComputeDispatch(const FfxPipelineState& Pipeline, uint32_t DispatchX, uint32_t DispatchY, uint32_t DispatchZ)
{
	FfxComputeJobDescription jobDescriptor = {};
	{
		jobDescriptor.dimensions[0] = DispatchX;
		jobDescriptor.dimensions[1] = DispatchY;
		jobDescriptor.dimensions[2] = DispatchZ;
		jobDescriptor.pipeline = Pipeline;

		for (uint32_t i = 0; i < Pipeline.srvTextureCount; ++i)
		{
			const uint32_t currentResourceId = Pipeline.srvTextureBindings[i].resourceIdentifier;
			wcscpy_s(jobDescriptor.srvTextureNames[i], Pipeline.srvTextureBindings[i].name);

			jobDescriptor.srvTextures[i] = m_SRVResources[currentResourceId];
		}

		for (uint32_t i = 0; i < Pipeline.uavTextureCount; ++i)
		{
			const uint32_t currentResourceId = Pipeline.uavTextureBindings[i].resourceIdentifier;
			wcscpy_s(jobDescriptor.uavTextureNames[i], Pipeline.uavTextureBindings[i].name);

			jobDescriptor.uavTextures[i] = m_UAVResources[currentResourceId];
			jobDescriptor.uavTextureMips[i] = 0;
		}

		if (Pipeline.constCount > 1)
			FFX_THROW_ON_FAIL(FFX_ERROR_INVALID_ARGUMENT); // Build-time issue

		if (Pipeline.constCount == 1)
		{
			wcscpy_s(jobDescriptor.cbNames[0], Pipeline.constantBufferBindings[0].name);
			jobDescriptor.cbs[0] = m_DispatchConstantBuffer;
		}
	}

	FfxGpuJobDescription job = { FFX_GPU_JOB_COMPUTE };
	job.computeJobDescriptor = jobDescriptor;

	return m_BackendInterface.fpScheduleGpuJob(&m_BackendInterface, &job);
}

FfxPipelineState& FFUIMask::GetPipelineStateForParameters(const FFUIMaskDispatchParameters& Parameters)
{
	uint32_t flags = 0;

	if (Parameters.HDR)
		flags |= FFX_FRAMEINTERPOLATION_ENABLE_HDR_COLOR_INPUT;

	// Try to find an existing pipeline state
	auto pipelineState = m_DispatchPipelineStates.find(flags);

	if (pipelineState != m_DispatchPipelineStates.end())
		return pipelineState->second;

	// Otherwise create a new one
	return InternalCreatePipelineState(flags);
}

FfxPipelineState& FFUIMask::InternalCreatePipelineState(uint32_t PassFlags)
{
	FfxPipelineDescription pipelineDescription =
	{
		.contextFlags = PassFlags,
		.stage = FFX_BIND_COMPUTE_SHADER_STAGE,
	};

	// Pipeline state - samplers
	const FfxSamplerDescription samplerDescs[] =
	{
		{
			FFX_FILTER_TYPE_MINMAGMIP_POINT,
			FFX_ADDRESS_MODE_CLAMP,
			FFX_ADDRESS_MODE_CLAMP,
			FFX_ADDRESS_MODE_CLAMP,
			FFX_BIND_COMPUTE_SHADER_STAGE,
		},
		{
			FFX_FILTER_TYPE_MINMAGMIP_LINEAR,
			FFX_ADDRESS_MODE_CLAMP,
			FFX_ADDRESS_MODE_CLAMP,
			FFX_ADDRESS_MODE_CLAMP,
			FFX_BIND_COMPUTE_SHADER_STAGE,
		},
	};

	pipelineDescription.samplers = samplerDescs;
	pipelineDescription.samplerCount = std::size(samplerDescs);

	// Pipeline state - root constants
	const FfxRootConstantDescription rootConstantDescs[] =
	{
		{ m_DispatchConstantBuffer.num32BitEntries, FFX_BIND_COMPUTE_SHADER_STAGE },
	};

	pipelineDescription.rootConstants = rootConstantDescs;
	pipelineDescription.rootConstantBufferCount = static_cast<uint32_t>(std::size(rootConstantDescs));

	// Query and assign device capabilities
	FfxDeviceCapabilities capabilities = {};
	m_BackendInterface.fpGetDeviceCapabilities(&m_BackendInterface, &capabilities);

	bool supportedFP16 = capabilities.fp16Supported;
	bool canForceWave64 = false;

	if (capabilities.waveLaneCountMin == 32 && capabilities.waveLaneCountMax == 64)
		canForceWave64 = capabilities.minimumSupportedShaderModel >= FFX_SHADER_MODEL_6_6;
	else
		canForceWave64 = false;

	// Determine permutation configuration for ReconstructPreviousDepth
	auto& pipelineState = m_DispatchPipelineStates.emplace(PassFlags, FfxPipelineState{}).first->second;

	wcscpy_s(pipelineDescription.name, L"FFXUIMASK_GenerateHUDLess");
	auto permutationFlags = GetPipelinePermutationFlags(
			pipelineDescription.contextFlags,
			supportedFP16,
			canForceWave64);

	FFX_THROW_ON_FAIL(m_BackendInterface.fpCreatePipeline(
		&m_BackendInterface,
		FFX_EFFECT_FRAMEINTERPOLATION,
		FFX_FRAMEINTERPOLATION_PASS_UIMASK,
		permutationFlags,
		&pipelineDescription,
		m_EffectContextId,
		&pipelineState));

	FFX_THROW_ON_FAIL(RemapResourceBindings(pipelineState));
	return pipelineState;
}

FfxErrorCode FFUIMask::RemapResourceBindings(FfxPipelineState& InOutPipeline)
{
	auto doRemap = []<typename T, typename U>(T *PipelineBindings, size_t PipelineBindCount, const U *NameTable, size_t NameTableCount)
	{
		for (size_t bindIndex = 0; bindIndex < PipelineBindCount; ++bindIndex)
		{
			if (PipelineBindings[bindIndex].bindCount == 0)
				continue;

			auto mapped = std::find_if(NameTable, NameTable + NameTableCount, [&](const auto& NameMap)
			{
				return wcscmp(NameMap.Name, PipelineBindings[bindIndex].name) == 0;
			});

			if (mapped == (NameTable + NameTableCount))
				return FFX_ERROR_INVALID_ARGUMENT;

			PipelineBindings[bindIndex].resourceIdentifier = mapped->Index;
		}

		return FFX_OK;
	};

	FfxErrorCode error = FFX_OK;
	error |= doRemap(InOutPipeline.srvTextureBindings, InOutPipeline.srvTextureCount, SRVTextureBindingTable, std::size(SRVTextureBindingTable));
	error |= doRemap(InOutPipeline.uavTextureBindings, InOutPipeline.uavTextureCount, UAVTextureBindingTable, std::size(UAVTextureBindingTable));
	error |= doRemap(InOutPipeline.constantBufferBindings, InOutPipeline.constCount, CBufferBindingTable, std::size(CBufferBindingTable));

	return error;
}

uint32_t FFUIMask::GetPipelinePermutationFlags(uint32_t ContextFlags, bool Fp16, bool Force64)
{
	uint32_t flags = 0;
    flags |= (ContextFlags & FFX_FRAMEINTERPOLATION_ENABLE_DEPTH_INVERTED) ? FRAMEINTERPOLATION_SHADER_PERMUTATION_DEPTH_INVERTED : 0;
    flags |= (Force64) ? FRAMEINTERPOLATION_SHADER_PERMUTATION_FORCE_WAVE64 : 0;
    flags |= (Fp16) ? FRAMEINTERPOLATION_SHADER_PERMUTATION_ALLOW_FP16 : 0;
    return flags;
}
// clang-format on

#endif
