//
// This file is a highly modified derivation of ffx_fsr3upscaler.cpp from AMD's FidelityFX-FSR SDK,
// which is licensed under the MIT license. See /src/components/fsr3upscaler/ffx_fsr3upscaler.cpp for
// the original source code.
//
// This file is licensed under a compatible license found in LICENSE.md in the root directory of this
// project.
//
#include "FFExt.h"
#include "FFDilator.h"

FFDilator::FFDilator(const FfxInterface& BackendInterface, uint32_t m_MaxRenderWidth, uint32_t m_MaxRenderHeight)
	: m_BackendInterface(BackendInterface),
	  m_MaxRenderWidth(m_MaxRenderWidth),
	  m_MaxRenderHeight(m_MaxRenderHeight)
{
	const auto status = m_BackendInterface.fpCreateBackendContext(&m_BackendInterface, &m_EffectContextId.emplace());

	if (status != FFX_OK)
	{
		m_EffectContextId.reset();
		throw std::runtime_error("FFDilator: Failed to create backend context.");
	}
}

FFDilator::~FFDilator()
{
	if (m_EffectContextId)
	{
		for (auto& pipeline : m_DispatchPipelineStates)
			m_BackendInterface.fpDestroyPipeline(&m_BackendInterface, &pipeline.second, *m_EffectContextId);

		m_BackendInterface.fpDestroyBackendContext(&m_BackendInterface, *m_EffectContextId);
	}
}

// clang-format off
FfxFsr3UpscalerSharedResourceDescriptions FFDilator::GetSharedResourceDescriptions() const
{
	FfxFsr3UpscalerSharedResourceDescriptions descs = {};

	descs.dilatedDepth =
	{
		FFX_HEAP_TYPE_DEFAULT,
		{
			FFX_RESOURCE_TYPE_TEXTURE2D, FFX_SURFACE_FORMAT_R32_FLOAT,
			m_MaxRenderWidth, m_MaxRenderHeight,
			1, 1, FFX_RESOURCE_FLAGS_ALIASABLE,
			(FfxResourceUsage)(FFX_RESOURCE_USAGE_RENDERTARGET | FFX_RESOURCE_USAGE_UAV)
		},
		FFX_RESOURCE_STATE_UNORDERED_ACCESS,
		0, nullptr,
		L"FFXDILATION_DilatedDepth",
		FFX_FSR3UPSCALER_RESOURCE_IDENTIFIER_DILATED_DEPTH,
	};

	descs.dilatedMotionVectors =
	{
		FFX_HEAP_TYPE_DEFAULT,
		{
			FFX_RESOURCE_TYPE_TEXTURE2D, FFX_SURFACE_FORMAT_R16G16_FLOAT,
			m_MaxRenderWidth, m_MaxRenderHeight,
			1, 1, FFX_RESOURCE_FLAGS_ALIASABLE,
			(FfxResourceUsage)(FFX_RESOURCE_USAGE_RENDERTARGET | FFX_RESOURCE_USAGE_UAV)
		},
		FFX_RESOURCE_STATE_UNORDERED_ACCESS,
		0, nullptr,
		L"FFXDILATION_DilatedVelocity",
		FFX_FSR3UPSCALER_RESOURCE_IDENTIFIER_DILATED_MOTION_VECTORS,
	};

	descs.reconstructedPrevNearestDepth =
	{
		FFX_HEAP_TYPE_DEFAULT,
		{
			FFX_RESOURCE_TYPE_TEXTURE2D, FFX_SURFACE_FORMAT_R32_UINT,
			m_MaxRenderWidth, m_MaxRenderHeight,
			1, 1, FFX_RESOURCE_FLAGS_ALIASABLE,
			(FfxResourceUsage)(FFX_RESOURCE_USAGE_UAV)
		},
		FFX_RESOURCE_STATE_UNORDERED_ACCESS,
		0, nullptr,
		L"FFXDILATION_ReconstructedPrevNearestDepth",
		FFX_FSR3UPSCALER_RESOURCE_IDENTIFIER_RECONSTRUCTED_PREVIOUS_NEAREST_DEPTH,
	};

	return descs;
}

FfxErrorCode FFDilator::Dispatch(const FFDilatorDispatchParameters& Parameters)
{
	auto registerResource = [&](const FfxResource& Resource, uint32_t Index, bool UAV)
	{
		m_BackendInterface.fpRegisterResource(&m_BackendInterface, &Resource, *m_EffectContextId, UAV ? &m_UAVResources[Index] : &m_SRVResources[Index]);
	};

	registerResource(Parameters.InputDepth, ResourceIndex::InputDepth, false);
	registerResource(Parameters.InputMotionVectors, ResourceIndex::InputMotionVectors, false);
	registerResource(Parameters.OutputDilatedDepth, ResourceIndex::OutputDilatedDepth, false);
	registerResource(Parameters.OutputDilatedDepth, ResourceIndex::OutputDilatedDepth, true);
	registerResource(Parameters.OutputDilatedMotionVectors, ResourceIndex::OutputDilatedMotionVectors, false);
	registerResource(Parameters.OutputDilatedMotionVectors, ResourceIndex::OutputDilatedMotionVectors, true);
	registerResource(Parameters.OutputReconstructedPrevNearestDepth, ResourceIndex::OutputReconstructedPrevNearestDepth, false);
	registerResource(Parameters.OutputReconstructedPrevNearestDepth, ResourceIndex::OutputReconstructedPrevNearestDepth, true);

	// Update constants. Many can be skipped since it's only reconstruction
	Fsr3UpscalerConstants constants = {};
	UpdateConstantBuffers(Parameters, constants);

	// Clear the reconstructed previous nearest depth buffer to maximum depth, then dispatch the shader
	{
		FfxGpuJobDescription job = { FFX_GPU_JOB_CLEAR_FLOAT };

		job.clearJobDescriptor.color[0] = Parameters.DepthInverted ? 0.0f : 1.0f;
		job.clearJobDescriptor.color[1] = job.clearJobDescriptor.color[0];
		job.clearJobDescriptor.color[2] = job.clearJobDescriptor.color[0];
		job.clearJobDescriptor.color[3] = job.clearJobDescriptor.color[0];
		job.clearJobDescriptor.target = m_SRVResources[ResourceIndex::OutputReconstructedPrevNearestDepth];

		FFX_RETURN_ON_FAIL(m_BackendInterface.fpScheduleGpuJob(&m_BackendInterface, &job));
	}

	// Determine hardcoded dispatch dimensions
	const uint32_t threadGroupWorkRegionDim = 8;
	const uint32_t dispatchSrcX = (constants.renderSize[0] + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;
	const uint32_t dispatchSrcY = (constants.renderSize[1] + (threadGroupWorkRegionDim - 1)) / threadGroupWorkRegionDim;

	FfxPipelineState *pipelineState = nullptr;
	FFX_RETURN_ON_FAIL(GetPipelineStateForParameters(Parameters, pipelineState));
	FFX_RETURN_ON_FAIL(ScheduleComputeDispatch(*pipelineState, dispatchSrcX, dispatchSrcY, 1));

	// Finally append our calls to the D3D12 command list and release
	// TODO: Need early fpUnregisterResources call to avoid resource leak on error
	FFX_RETURN_ON_FAIL(m_BackendInterface.fpExecuteGpuJobs(&m_BackendInterface, Parameters.CommandList));
	FFX_RETURN_ON_FAIL(m_BackendInterface.fpUnregisterResources(&m_BackendInterface, Parameters.CommandList, *m_EffectContextId));

	return FFX_OK;
}

void FFDilator::UpdateConstantBuffers(const FFDilatorDispatchParameters& Parameters, Fsr3UpscalerConstants& Constants)
{
	Constants.renderSize[0] = Parameters.RenderSize.width;
	Constants.renderSize[1] = Parameters.RenderSize.height;
	Constants.maxRenderSize[0] = m_MaxRenderWidth;
	Constants.maxRenderSize[1] = m_MaxRenderHeight;
	Constants.displaySize[0] = Parameters.OutputSize.width;
	Constants.displaySize[1] = Parameters.OutputSize.height;

	if (Parameters.MotionVectorsFullResolution)
	{
		Constants.renderSize[0] = Constants.displaySize[0];
		Constants.renderSize[1] = Constants.displaySize[1];
	}

	Constants.inputColorResourceDimensions[0] = Constants.renderSize[0];
	Constants.inputColorResourceDimensions[1] = Constants.renderSize[1];

	Constants.jitterOffset[0] = Parameters.MotionVectorJitterOffsets.x;
	Constants.jitterOffset[1] = Parameters.MotionVectorJitterOffsets.y;
	Constants.motionVectorScale[0] = Parameters.MotionVectorScale.x / Constants.renderSize[0];
	Constants.motionVectorScale[1] = Parameters.MotionVectorScale.y / Constants.renderSize[1];

	if (Parameters.MotionVectorJitterCancellation)
	{
		Constants.motionVectorJitterCancellation[0] = (m_PreviousConstants.jitterOffset[0] - Constants.jitterOffset[0]) / Constants.renderSize[0];
		Constants.motionVectorJitterCancellation[1] = (m_PreviousConstants.jitterOffset[1] - Constants.jitterOffset[1]) / Constants.renderSize[1];
	}

	// Placeholders. Not used.
	Constants.preExposure = 1.0f;
	Constants.previousFramePreExposure = 1.0f;

	m_PreviousConstants = m_CurrentConstants;
	m_CurrentConstants = Constants;
	memcpy(&m_DispatchConstantBuffer.data, &m_CurrentConstants, m_DispatchConstantBuffer.num32BitEntries * sizeof(uint32_t));
}

FfxErrorCode FFDilator::ScheduleComputeDispatch(const FfxPipelineState& Pipeline, uint32_t DispatchX, uint32_t DispatchY, uint32_t DispatchZ)
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

		if (Pipeline.constCount != 1)
			__debugbreak(); // Build-time issue

		wcscpy_s(jobDescriptor.cbNames[0], Pipeline.constantBufferBindings[0].name);
		jobDescriptor.cbs[0] = m_DispatchConstantBuffer;
	}

	FfxGpuJobDescription job = { FFX_GPU_JOB_COMPUTE };
	job.computeJobDescriptor = jobDescriptor;

	return m_BackendInterface.fpScheduleGpuJob(&m_BackendInterface, &job);
}

FfxErrorCode FFDilator::GetPipelineStateForParameters(const FFDilatorDispatchParameters& Parameters, FfxPipelineState*& PipelineState)
{
	uint32_t flags = 0;

	if (Parameters.HDR)
		flags |= FFX_FSR3UPSCALER_ENABLE_HIGH_DYNAMIC_RANGE;

	if (Parameters.DepthInverted)
		flags |= FFX_FSR3UPSCALER_ENABLE_DEPTH_INVERTED;

	if (Parameters.MotionVectorsFullResolution)
		flags |= FFX_FSR3UPSCALER_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS;

	if (Parameters.MotionVectorJitterCancellation)
		flags |= FFX_FSR3UPSCALER_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION;

	// Try to find an existing pipeline state
	if (!m_DispatchPipelineStates.contains(flags))
		FFX_RETURN_ON_FAIL(InternalCreatePipelineState(flags));

	PipelineState = &m_DispatchPipelineStates.at(flags);
	return FFX_OK;
}

FfxErrorCode FFDilator::InternalCreatePipelineState(uint32_t PassFlags)
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

	wcscpy_s(pipelineDescription.name, L"FFXDILATION_ReconstructPreviousDepth");
	auto permutationFlags = GetPipelinePermutationFlags(
			pipelineDescription.contextFlags,
			supportedFP16,
			canForceWave64);

	FFX_RETURN_ON_FAIL(m_BackendInterface.fpCreatePipeline(
		&m_BackendInterface,
		FFX_EFFECT_FSR3UPSCALER,
		FFX_FSR3UPSCALER_PASS_RECONSTRUCT_PREVIOUS_DEPTH,
		permutationFlags,
		&pipelineDescription,
		*m_EffectContextId,
		&pipelineState));

	return RemapResourceBindings(pipelineState);
}

FfxErrorCode FFDilator::RemapResourceBindings(FfxPipelineState& InOutPipeline)
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

uint32_t FFDilator::GetPipelinePermutationFlags(uint32_t ContextFlags, bool Fp16, bool Force64)
{
	uint32_t flags = 0;
	flags |= (ContextFlags & FFX_FSR3UPSCALER_ENABLE_HIGH_DYNAMIC_RANGE) ? FSR3UPSCALER_SHADER_PERMUTATION_HDR_COLOR_INPUT : 0;
	flags |= (ContextFlags & FFX_FSR3UPSCALER_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS) ? 0 : FSR3UPSCALER_SHADER_PERMUTATION_LOW_RES_MOTION_VECTORS;
	flags |= (ContextFlags & FFX_FSR3UPSCALER_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION) ? FSR3UPSCALER_SHADER_PERMUTATION_JITTER_MOTION_VECTORS : 0;
	flags |= (ContextFlags & FFX_FSR3UPSCALER_ENABLE_DEPTH_INVERTED) ? FSR3UPSCALER_SHADER_PERMUTATION_DEPTH_INVERTED : 0;
	flags |= (Force64) ? FSR3UPSCALER_SHADER_PERMUTATION_FORCE_WAVE64 : 0;
	flags |= (Fp16) ? FSR3UPSCALER_SHADER_PERMUTATION_ALLOW_FP16 : 0;

	flags |= FSR3UPSCALER_SHADER_PERMUTATION_FORKEDCUSTOMIZATIONS; // FORK HACK

	return flags;
}
// clang-format on
