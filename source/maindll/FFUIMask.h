//
// This file is a highly modified derivation of ffx_fsr3upscaler.h from AMD's FidelityFX-FSR SDK,
// which is licensed under the MIT license. See /include/FidelityFX/host/fsr3upscaler/ffx_fsr3upscaler.h
// for the original source code.
//
// This file is licensed under a compatible license found in LICENSE.md in the root directory of this
// project.
//
#pragma once

#if 0 // Experimental/debugging purposes only

#include <FidelityFX/host/ffx_frameinterpolation.h>
#include <FidelityFX/components/frameinterpolation/ffx_frameinterpolation_private.h>

struct FFUIMaskDispatchParameters
{
	FfxCommandList CommandList;

	FfxDimensions2D RenderSize;

	FfxResource InputColor;
	FfxResource InputUIMask;

	FfxResource OutputHUDLessColor;

	bool HDR;
	FfxFloatCoords2D MinMaxLuminance;
};

class FFUIMask
{
private:
	struct ResourceIndex
	{
		enum
		{
			InputColor = 0,
			InputUIMask = 1,
			OutputHUDLessColor = 2,

			Count,
		};
	};

	struct ResourceBinding
	{
		uint32_t Index;
		wchar_t Name[64];
	};

	constexpr static ResourceBinding SRVTextureBindingTable[] = {
		{ ResourceIndex::InputColor, L"r_present_backbuffer" }, // Hijacked inputs
		{ ResourceIndex::InputUIMask, L"r_current_interpolation_source" },
	};

	constexpr static ResourceBinding UAVTextureBindingTable[] = {
		{ ResourceIndex::OutputHUDLessColor, L"rw_output" },
	};

	constexpr static ResourceBinding CBufferBindingTable[] = {
		{ 0, L"cbFI" },
	};

	FfxInterface m_BackendInterface = {};
	uint32_t m_EffectContextId = 0;

	std::unordered_map<uint32_t, FfxPipelineState> m_DispatchPipelineStates = {};

	FrameInterpolationConstants m_CurrentConstants = {};
	FfxConstantBuffer m_DispatchConstantBuffer = { { sizeof(FrameInterpolationConstants) / sizeof(uint32_t) } };

	FfxResourceInternal m_SRVResources[ResourceIndex::Count] = {};
	FfxResourceInternal m_UAVResources[ResourceIndex::Count] = {};

public:
	FFUIMask(const FfxInterface& BackendInterface);
	FFUIMask(const FFUIMask&) = delete;
	FFUIMask& operator=(const FFUIMask&) = delete;
	~FFUIMask();

	FfxErrorCode Dispatch(const FFUIMaskDispatchParameters& Parameters);

private:
	void UpdateConstantBuffers(const FFUIMaskDispatchParameters& Parameters, FrameInterpolationConstants& Constants);
	FfxErrorCode ScheduleComputeDispatch(const FfxPipelineState& Pipeline, uint32_t DispatchX, uint32_t DispatchY, uint32_t DispatchZ);
	FfxPipelineState& GetPipelineStateForParameters(const FFUIMaskDispatchParameters& Parameters);
	FfxPipelineState& InternalCreatePipelineState(uint32_t PassFlags);
	FfxErrorCode RemapResourceBindings(FfxPipelineState& InOutPipeline);

	static uint32_t GetPipelinePermutationFlags(uint32_t ContextFlags, bool Fp16, bool Force64);
};

#endif
