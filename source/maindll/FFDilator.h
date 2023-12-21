//
// This file is a highly modified derivation of ffx_fsr3upscaler.h from AMD's FidelityFX-FSR SDK,
// which is licensed under the MIT license. See /include/FidelityFX/host/fsr3upscaler/ffx_fsr3upscaler.h
// for the original source code.
//
// This file is licensed under a compatible license found in LICENSE.md in the root directory of this
// project.
//
#pragma once

#include <FidelityFX/host/ffx_fsr3upscaler.h>
#include <FidelityFX/components/fsr3upscaler/ffx_fsr3upscaler_private.h>

struct FFDilatorDispatchParameters
{
	FfxCommandList CommandList;

	FfxDimensions2D RenderSize;
	FfxDimensions2D OutputSize;

	FfxResource InputDepth;
	FfxResource InputMotionVectors;

	FfxResource OutputDilatedDepth;
	FfxResource OutputDilatedMotionVectors;
	FfxResource OutputReconstructedPrevNearestDepth;

	bool HDR;
	bool DepthInverted;
	bool MotionVectorsFullResolution;
	bool MotionVectorJitterCancellation;
	FfxFloatCoords2D MotionVectorScale;
	FfxFloatCoords2D MotionVectorJitterOffsets;
};

class FFDilator
{
private:
	struct ResourceIndex
	{
		enum
		{
			InputDepth = 0,
			InputMotionVectors = 1,
			OutputDilatedDepth = 2,
			OutputDilatedMotionVectors = 3,
			OutputReconstructedPrevNearestDepth = 4,

			Count,
		};
	};

	struct ResourceBinding
	{
		uint32_t Index;
		wchar_t Name[64];
	};

	constexpr static ResourceBinding SRVTextureBindingTable[] =
	{
		{ ResourceIndex::InputDepth, L"r_input_depth" },
		{ ResourceIndex::InputMotionVectors, L"r_input_motion_vectors" },
		{ ResourceIndex::OutputDilatedDepth, L"r_dilated_depth" },
		{ ResourceIndex::OutputDilatedMotionVectors, L"r_dilated_motion_vectors" },
		{ ResourceIndex::OutputReconstructedPrevNearestDepth, L"r_reconstructed_previous_nearest_depth" },
	};

	constexpr static ResourceBinding UAVTextureBindingTable[] =
	{
		{ ResourceIndex::OutputReconstructedPrevNearestDepth, L"rw_reconstructed_previous_nearest_depth" },
		{ ResourceIndex::OutputDilatedMotionVectors, L"rw_dilated_motion_vectors" },
		{ ResourceIndex::OutputDilatedDepth, L"rw_dilated_depth" },
	};

	constexpr static ResourceBinding CBufferBindingTable[] =
	{
		{ 0, L"cbFSR3Upscaler" },
	};

	const uint32_t m_MaxRenderWidth;
	const uint32_t m_MaxRenderHeight;

	FfxInterface m_BackendInterface = {};
	uint32_t m_EffectContextId = 0;

	std::unordered_map<uint32_t, FfxPipelineState> m_DispatchPipelineStates = {};
	
	Fsr3UpscalerConstants m_CurrentConstants = {};
	Fsr3UpscalerConstants m_PreviousConstants = {};
	FfxConstantBuffer m_DispatchConstantBuffer = { { sizeof(Fsr3UpscalerConstants) / sizeof(uint32_t) } };

    FfxResourceInternal m_SRVResources[ResourceIndex::Count] = {};
	FfxResourceInternal m_UAVResources[ResourceIndex::Count] = {};

public:
	FFDilator(const FfxInterface& BackendInterface, uint32_t m_MaxRenderWidth, uint32_t m_MaxRenderHeight);
	FFDilator(const FFDilator&) = delete;
	FFDilator& operator=(const FFDilator&) = delete;
	~FFDilator();

	FfxFsr3UpscalerSharedResourceDescriptions GetSharedResourceDescriptions() const;
	FfxErrorCode Dispatch(const FFDilatorDispatchParameters& Parameters);

private:
	void UpdateConstantBuffers(const FFDilatorDispatchParameters& Parameters, Fsr3UpscalerConstants& Constants);
	FfxErrorCode ScheduleComputeDispatch(const FfxPipelineState& Pipeline, uint32_t DispatchX, uint32_t DispatchY, uint32_t DispatchZ);
	FfxPipelineState& GetPipelineStateForParameters(const FFDilatorDispatchParameters& Parameters);
	FfxPipelineState& InternalCreatePipelineState(uint32_t PassFlags);
	FfxErrorCode RemapResourceBindings(FfxPipelineState& InOutPipeline);

	static uint32_t GetPipelinePermutationFlags(uint32_t ContextFlags, bool Fp16, bool Force64);
};
