#include <dxgi1_6.h>
#include <numbers>
#include "NGX/NvNGX.h"
#include "FFExt.h"
#include "FFFrameInterpolator.h"
#include "Util.h"

FFFrameInterpolator::FFFrameInterpolator(
	uint32_t OutputWidth,
	uint32_t OutputHeight,
	NGXInstanceParameters *NGXParameters)
	: m_SwapchainWidth(OutputWidth),
	  m_SwapchainHeight(OutputHeight)
{
	Create(NGXParameters);
}

FFFrameInterpolator::~FFFrameInterpolator()
{
	Destroy();
}

FfxErrorCode FFFrameInterpolator::Dispatch(void *CommandList, NGXInstanceParameters *NGXParameters)
{
	FfxResource gameBackBufferResource = {};
	FfxResource gameRealOutputResource = {};

	const auto dispatchStatus = [&]() -> FfxErrorCode
	{
		const bool enableInterpolation = NGXParameters->GetUIntOrDefault("DLSSG.EnableInterp", 0) != 0;

		LoadTextureFromNGXParameters(NGXParameters, "DLSSG.Backbuffer", &gameBackBufferResource, FFX_RESOURCE_STATE_COMPUTE_READ);
		LoadTextureFromNGXParameters(NGXParameters, "DLSSG.OutputReal", &gameRealOutputResource, FFX_RESOURCE_STATE_UNORDERED_ACCESS);

		if (!enableInterpolation)
			return FFX_OK;

		if (!CalculateResourceDimensions(NGXParameters))
			return FFX_ERROR_INVALID_ARGUMENT;

		QueryHDRLuminanceRange(NGXParameters);

		// Parameter setup
		FFDilatorDispatchParameters fsrDilationDesc = {};
		FfxOpticalflowDispatchDescription fsrOfDispatchDesc = {};
		FFInterpolatorDispatchParameters fsrFiDispatchDesc = {};

		if (!BuildDilationParameters(&fsrDilationDesc, NGXParameters))
			return FFX_ERROR_INVALID_ARGUMENT;

		if (!BuildOpticalFlowParameters(&fsrOfDispatchDesc, NGXParameters))
			return FFX_ERROR_INVALID_ARGUMENT;

		if (!BuildFrameInterpolationParameters(&fsrFiDispatchDesc, NGXParameters))
			return FFX_ERROR_INVALID_ARGUMENT;

		const static bool doDebugOverlay = Util::GetSetting(L"Debug", L"EnableDebugOverlay", false);
		const static bool doDebugTearLines = Util::GetSetting(L"Debug", L"EnableDebugTearLines", false);
		const static bool doInterpolatedOnly = Util::GetSetting(L"Debug", L"EnableInterpolatedFramesOnly", false);

		fsrFiDispatchDesc.DebugView = doDebugOverlay;
		fsrFiDispatchDesc.DebugTearLines = doDebugTearLines;

		// Record commands
		auto status = m_DilationContext->Dispatch(fsrDilationDesc);
		FFX_RETURN_ON_FAIL(status);

		status = ffxOpticalflowContextDispatch(&m_OpticalFlowContext.value(), &fsrOfDispatchDesc);
		FFX_RETURN_ON_FAIL(status);

		status = m_FrameInterpolatorContext->Dispatch(fsrFiDispatchDesc);
		FFX_RETURN_ON_FAIL(status);

		if (fsrFiDispatchDesc.DebugView || doInterpolatedOnly)
			gameBackBufferResource = fsrFiDispatchDesc.OutputInterpolatedColorBuffer;

		return FFX_OK;
	}();

	if (dispatchStatus == FFX_OK && gameRealOutputResource.resource && gameBackBufferResource.resource)
		CopyTexture(GetActiveCommandList(), &gameRealOutputResource, &gameBackBufferResource);

	NGXParameters->Set4("DLSSG.FlushRequired", 0);
	return dispatchStatus;
}

bool FFFrameInterpolator::CalculateResourceDimensions(NGXInstanceParameters *NGXParameters)
{
	// NGX doesn't provide a direct method to query current gbuffer dimensions so we'll grab them
	// from the depth buffer instead. Depth is suitable because it's the one resource guaranteed to
	// be the same size as the gbuffer. Hopefully.
	FfxResource temp = {};
	{
		auto width = NGXParameters->GetUIntOrDefault("DLSSG.DepthSubrectWidth", 0);
		auto height = NGXParameters->GetUIntOrDefault("DLSSG.DepthSubrectHeight", 0);

		if (width == 0 || height == 0)
		{
			LoadTextureFromNGXParameters(NGXParameters, "DLSSG.Depth", &temp, FFX_RESOURCE_STATE_COPY_DEST);
			width = temp.description.width;
			height = temp.description.height;
		}

		m_PreUpscaleRenderWidth = width;
		m_PreUpscaleRenderHeight = height;
	}

	if (m_PreUpscaleRenderWidth <= 32 || m_PreUpscaleRenderHeight <= 32)
		return false;

	// HUD-less dimensions are the "ground truth" final render resolution. These aren't necessarily
	// equal to back buffer dimensions. Letterboxing in The Witcher 3 is a good test case.
#if 0
	if (LoadTextureFromNGXParameters(NGXParameters, "DLSSG.HUDLess", &temp, FFX_RESOURCE_STATE_COPY_DEST))
	{
		auto width = NGXParameters->GetUIntOrDefault("DLSSG.HUDLessSubrectWidth", 0);
		auto height = NGXParameters->GetUIntOrDefault("DLSSG.HUDLessSubrectHeight", 0);

		if (width == 0 || height == 0)
		{
			width = temp.description.width;
			height = temp.description.height;
		}

		m_PostUpscaleRenderWidth = width;
		m_PostUpscaleRenderHeight = height;
	}
	else
#endif
	{
		// No HUD-less resource. Default to back buffer resolution.
		m_PostUpscaleRenderWidth = m_SwapchainWidth;
		m_PostUpscaleRenderHeight = m_SwapchainHeight;
	}

	if (m_PostUpscaleRenderWidth <= 32 || m_PostUpscaleRenderHeight <= 32)
		return false;

	// I've no better place to put this. Dying Light 2 is beyond screwed up.
	//
	// 1. They take a D24 depth buffer and explicitly convert it to RGBA8. The GBA channels are unused. This
	//    is passed to Streamline as the "depth" buffer.
	//
	// 2. They take the same RGBA8 "depth" buffer above and pass it to Streamline as the "HUD-less" color
	//    buffer.
	//
	// How's it possible to be this incompetent? Have these people used a debugger? The debug visualizer? For
	// all I know the native DLSS-G implementation doesn't even work. It could just be duplicating frames.
	const static auto isDyingLight2 = GetModuleHandleW(L"DyingLightGame_x64_rwdi.exe") != nullptr;

	if (isDyingLight2)
	{
		NGXParameters->SetVoidPointer("DLSSG.HUDLess", nullptr);
		m_PostUpscaleRenderWidth = m_SwapchainWidth;
		m_PostUpscaleRenderHeight = m_SwapchainHeight;
	}

	return true;
}

void FFFrameInterpolator::QueryHDRLuminanceRange(NGXInstanceParameters *NGXParameters)
{
	if (NGXParameters->GetUIntOrDefault("DLSSG.ColorBuffersHDR", 0) == 0)
		return;

	if (m_HDRLuminanceRangeSet)
		return;

	// Microsoft DirectX 12 HDR sample
	// https://github.com/microsoft/DirectX-Graphics-Samples/blob/b5f92e2251ee83db4d4c795b3cba5d470c52eaf8/Samples/Desktop/D3D12HDR/src/D3D12HDR.cpp#L1064
	const auto luid = GetActiveAdapterLUID();

	IDXGIFactory1 *factory = nullptr;
	IDXGIAdapter1 *adapter = nullptr;
	IDXGIOutput *output = nullptr;

	if (CreateDXGIFactory1(IID_PPV_ARGS(&factory)) == S_OK)
	{
		// Match the active DXGI adapter
		for (uint32_t i = 0; factory->EnumAdapters1(i, &adapter) == S_OK; i++)
		{
			DXGI_ADAPTER_DESC desc = {};
			adapter->GetDesc(&desc);

			static_assert(luid.size() == sizeof(desc.AdapterLuid));

			if (memcmp(&desc.AdapterLuid, luid.data(), luid.size()) == 0)
			{
				// Then check the first HDR-capable output
				for (uint32_t j = 0; adapter->EnumOutputs(j, &output) == S_OK; j++)
				{
					if (IDXGIOutput6 *output6; output->QueryInterface(IID_PPV_ARGS(&output6)) == S_OK)
					{
						DXGI_OUTPUT_DESC1 outputDesc = {};
						output6->GetDesc1(&outputDesc);
						output6->Release();

						if (outputDesc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 && !m_HDRLuminanceRangeSet)
						{
							m_HDRLuminanceRange = { outputDesc.MinLuminance, outputDesc.MaxLuminance };
							m_HDRLuminanceRangeSet = true;
						}
					}

					output->Release();
				}
			}

			adapter->Release();
		}

		factory->Release();
	}

	// Keep using hardcoded defaults even if we didn't find a valid output
	m_HDRLuminanceRangeSet = true;

	spdlog::info("Using assumed HDR luminance range: {} to {} nits", m_HDRLuminanceRange.x, m_HDRLuminanceRange.y);
}

bool FFFrameInterpolator::BuildDilationParameters(FFDilatorDispatchParameters *OutParameters, NGXInstanceParameters *NGXParameters)
{
	auto& desc = *OutParameters;
	desc.CommandList = GetActiveCommandList();

	if (!LoadTextureFromNGXParameters(NGXParameters, "DLSSG.Depth", &desc.InputDepth, FFX_RESOURCE_STATE_COPY_DEST))
		return false;

	if (!LoadTextureFromNGXParameters(NGXParameters, "DLSSG.MVecs", &desc.InputMotionVectors, FFX_RESOURCE_STATE_COPY_DEST))
		return false;

	desc.OutputDilatedDepth = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_TexSharedDilatedDepth);
	desc.OutputDilatedMotionVectors = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_TexSharedDilatedMotionVectors);
	desc.OutputReconstructedPrevNearestDepth = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_TexSharedPreviousNearestDepth);

	desc.RenderSize = { m_PreUpscaleRenderWidth, m_PreUpscaleRenderHeight };
	desc.OutputSize = { m_PostUpscaleRenderWidth, m_PostUpscaleRenderHeight };

	desc.HDR = NGXParameters->GetUIntOrDefault("DLSSG.ColorBuffersHDR", 0) != 0;
	desc.DepthInverted = NGXParameters->GetUIntOrDefault("DLSSG.DepthInverted", 0) != 0;

	const FfxDimensions2D mvecExtents = {
		NGXParameters->GetUIntOrDefault("DLSSG.MVecsSubrectWidth", desc.InputMotionVectors.description.width),
		NGXParameters->GetUIntOrDefault("DLSSG.MVecsSubrectHeight", desc.InputMotionVectors.description.height),
	};

	desc.MotionVectorScale = {
		NGXParameters->GetFloatOrDefault("DLSSG.MvecScaleX", 1.0f),
		NGXParameters->GetFloatOrDefault("DLSSG.MvecScaleY", 1.0f),
	};

	desc.MotionVectorJitterOffsets = {
		NGXParameters->GetFloatOrDefault("DLSSG.JitterOffsetX", 0),
		NGXParameters->GetFloatOrDefault("DLSSG.JitterOffsetY", 0),
	};

	desc.MotionVectorsDilated = NGXParameters->GetUIntOrDefault("DLSSG.MvecDilated", 0) != 0;
	desc.MotionVectorsFullResolution = m_PostUpscaleRenderWidth == mvecExtents.width && m_PostUpscaleRenderHeight == mvecExtents.height;
	desc.MotionVectorJitterCancellation = NGXParameters->GetUIntOrDefault("DLSSG.MvecJittered", 0) != 0;

	return true;
}

bool FFFrameInterpolator::BuildOpticalFlowParameters(
	FfxOpticalflowDispatchDescription *OutParameters,
	NGXInstanceParameters *NGXParameters)
{
	auto& desc = *OutParameters;
	desc.commandList = GetActiveCommandList();

	if (!LoadTextureFromNGXParameters(NGXParameters, "DLSSG.HUDLess", &desc.color, FFX_RESOURCE_STATE_COPY_DEST) &&
		!LoadTextureFromNGXParameters(NGXParameters, "DLSSG.Backbuffer", &desc.color, FFX_RESOURCE_STATE_COMPUTE_READ))
		return false;

	desc.color.description.width = m_PostUpscaleRenderWidth; // Explicit override
	desc.color.description.height = m_PostUpscaleRenderHeight;

	desc.opticalFlowVector = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_TexSharedOpticalFlowVector);
	desc.opticalFlowSCD = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_TexSharedOpticalFlowSCD);

	desc.reset = NGXParameters->GetUIntOrDefault("DLSSG.Reset", 0) != 0;

	if (NGXParameters->GetUIntOrDefault("DLSSG.ColorBuffersHDR", 0) == 0)
		desc.backbufferTransferFunction = FFX_BACKBUFFER_TRANSFER_FUNCTION_SRGB;
	else
		desc.backbufferTransferFunction = FFX_BACKBUFFER_TRANSFER_FUNCTION_PQ;

	desc.minMaxLuminance = m_HDRLuminanceRange;

	return true;
}

bool FFFrameInterpolator::BuildFrameInterpolationParameters(
	FFInterpolatorDispatchParameters *OutParameters,
	NGXInstanceParameters *NGXParameters)
{
	auto& desc = *OutParameters;
	desc.CommandList = GetActiveCommandList();

	LoadTextureFromNGXParameters(NGXParameters, "DLSSG.HUDLess", &desc.InputHUDLessColorBuffer, FFX_RESOURCE_STATE_COPY_DEST);

	if (!LoadTextureFromNGXParameters(NGXParameters, "DLSSG.Backbuffer", &desc.InputColorBuffer, FFX_RESOURCE_STATE_COMPUTE_READ) &&
		!desc.InputHUDLessColorBuffer.resource)
		return false;

	if (!LoadTextureFromNGXParameters(
			NGXParameters,
			"DLSSG.OutputInterpolated",
			&desc.OutputInterpolatedColorBuffer,
			FFX_RESOURCE_STATE_UNORDERED_ACCESS))
		return false;

	desc.InputDilatedDepth = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_TexSharedDilatedDepth);
	desc.InputDilatedMotionVectors = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_TexSharedDilatedMotionVectors);
	desc.InputReconstructedPreviousNearDepth = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_TexSharedPreviousNearestDepth);
	desc.InputOpticalFlowVector = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_TexSharedOpticalFlowVector);
	desc.InputOpticalFlowSceneChangeDetection = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_TexSharedOpticalFlowSCD);

	desc.RenderSize = { m_PreUpscaleRenderWidth, m_PreUpscaleRenderHeight };
	desc.OutputSize = { m_SwapchainWidth, m_SwapchainHeight };

	desc.OpticalFlowBufferSize = { desc.InputOpticalFlowVector.description.width, desc.InputOpticalFlowVector.description.height };
	desc.OpticalFlowBlockSize = 8;
	desc.OpticalFlowScale = { 1.0f / m_PostUpscaleRenderWidth, 1.0f / m_PostUpscaleRenderHeight };

	// Games require a deg2rad fixup because...reasons
	desc.CameraFovAngleVertical = NGXParameters->GetFloatOrDefault("DLSSG.CameraFOV", 0);

	if (desc.CameraFovAngleVertical > 10.0f)
		desc.CameraFovAngleVertical *= std::numbers::pi_v<float> / 180.0f;

	desc.CameraNear = NGXParameters->GetFloatOrDefault("DLSSG.CameraNear", 0);
	desc.CameraFar = NGXParameters->GetFloatOrDefault("DLSSG.CameraFar", 0);
	desc.ViewSpaceToMetersFactor = 0.0f; // TODO

	desc.HDR = NGXParameters->GetUIntOrDefault("DLSSG.ColorBuffersHDR", 0) != 0;
	desc.DepthInverted = NGXParameters->GetUIntOrDefault("DLSSG.DepthInverted", 0) != 0;
	desc.Reset = NGXParameters->GetUIntOrDefault("DLSSG.Reset", 0) != 0;

	desc.MinMaxLuminance = m_HDRLuminanceRange;

	return true;
}

void FFFrameInterpolator::Create(NGXInstanceParameters *NGXParameters)
{
	if (CreateBackend(NGXParameters) != FFX_OK)
		throw std::runtime_error("Failed to create backend context.");

	if (CreateDilationContext() != FFX_OK)
		throw std::runtime_error("Failed to create dilation context.");

	if (CreateOpticalFlowContext() != FFX_OK)
		throw std::runtime_error("Failed to create optical flow context.");

	m_FrameInterpolatorContext.emplace(m_FrameInterpolationBackendInterface, m_SwapchainWidth, m_SwapchainHeight);
}

void FFFrameInterpolator::Destroy()
{
	m_FrameInterpolatorContext.reset();
	DestroyOpticalFlowContext();
	DestroyDilationContext();
	DestroyBackend();
}

FfxErrorCode FFFrameInterpolator::CreateBackend(NGXInstanceParameters *NGXParameters)
{
	const uint32_t maxContexts = 3; // Assume 3 contexts per interface
	FFX_RETURN_ON_FAIL(InitializeBackendInterface(&m_SharedBackendInterface, maxContexts, NGXParameters));
	FFX_RETURN_ON_FAIL(InitializeBackendInterface(&m_FrameInterpolationBackendInterface, maxContexts, NGXParameters));

	const auto status = m_SharedBackendInterface.fpCreateBackendContext(&m_SharedBackendInterface, &m_SharedEffectContextId.emplace());

	if (status != FFX_OK)
	{
		m_SharedEffectContextId.reset();
		return status;
	}

	return FFX_OK;
}

void FFFrameInterpolator::DestroyBackend()
{
	if (m_SharedEffectContextId)
		m_SharedBackendInterface.fpDestroyBackendContext(&m_SharedBackendInterface, *m_SharedEffectContextId);
}

FfxErrorCode FFFrameInterpolator::CreateDilationContext()
{
	m_DilationContext.emplace(m_SharedBackendInterface, m_SwapchainWidth, m_SwapchainHeight);
	const auto resourceDescs = m_DilationContext->GetSharedResourceDescriptions();

	auto status = m_SharedBackendInterface.fpCreateResource(
		&m_SharedBackendInterface,
		&resourceDescs.dilatedDepth,
		*m_SharedEffectContextId,
		&m_TexSharedDilatedDepth.emplace());

	if (status != FFX_OK)
	{
		m_TexSharedDilatedDepth.reset();
		return status;
	}

	status = m_SharedBackendInterface.fpCreateResource(
		&m_SharedBackendInterface,
		&resourceDescs.dilatedMotionVectors,
		*m_SharedEffectContextId,
		&m_TexSharedDilatedMotionVectors.emplace());

	if (status != FFX_OK)
	{
		m_TexSharedDilatedMotionVectors.reset();
		return status;
	}

	status = m_SharedBackendInterface.fpCreateResource(
		&m_SharedBackendInterface,
		&resourceDescs.reconstructedPrevNearestDepth,
		*m_SharedEffectContextId,
		&m_TexSharedPreviousNearestDepth.emplace());

	if (status != FFX_OK)
	{
		m_TexSharedPreviousNearestDepth.reset();
		return status;
	}

	return FFX_OK;
}

void FFFrameInterpolator::DestroyDilationContext()
{
	m_DilationContext.reset();

	if (m_TexSharedDilatedDepth)
		m_SharedBackendInterface.fpDestroyResource(&m_SharedBackendInterface, *m_TexSharedDilatedDepth, *m_SharedEffectContextId);

	if (m_TexSharedDilatedMotionVectors)
		m_SharedBackendInterface.fpDestroyResource(&m_SharedBackendInterface, *m_TexSharedDilatedMotionVectors, *m_SharedEffectContextId);

	if (m_TexSharedPreviousNearestDepth)
		m_SharedBackendInterface.fpDestroyResource(&m_SharedBackendInterface, *m_TexSharedPreviousNearestDepth, *m_SharedEffectContextId);
}

FfxErrorCode FFFrameInterpolator::CreateOpticalFlowContext()
{
	// Set up configuration for optical flow
	FfxOpticalflowContextDescription fsrOfDescription = {};
	fsrOfDescription.backendInterface = m_FrameInterpolationBackendInterface;
	fsrOfDescription.flags = 0;
	fsrOfDescription.resolution = { m_SwapchainWidth, m_SwapchainHeight };
	auto status = ffxOpticalflowContextCreate(&m_OpticalFlowContext.emplace(), &fsrOfDescription);

	if (status != FFX_OK)
	{
		m_OpticalFlowContext.reset();
		return status;
	}

	FfxOpticalflowSharedResourceDescriptions fsrOfSharedDescriptions = {};
	FFX_RETURN_ON_FAIL(ffxOpticalflowGetSharedResourceDescriptions(&m_OpticalFlowContext.value(), &fsrOfSharedDescriptions));

	status = m_SharedBackendInterface.fpCreateResource(
		&m_SharedBackendInterface,
		&fsrOfSharedDescriptions.opticalFlowVector,
		*m_SharedEffectContextId,
		&m_TexSharedOpticalFlowVector.emplace());

	if (status != FFX_OK)
	{
		m_TexSharedOpticalFlowVector.reset();
		return status;
	}

	status = m_SharedBackendInterface.fpCreateResource(
		&m_SharedBackendInterface,
		&fsrOfSharedDescriptions.opticalFlowSCD,
		*m_SharedEffectContextId,
		&m_TexSharedOpticalFlowSCD.emplace());

	if (status != FFX_OK)
	{
		m_TexSharedOpticalFlowSCD.reset();
		return status;
	}

	return FFX_OK;
}

void FFFrameInterpolator::DestroyOpticalFlowContext()
{
	if (m_OpticalFlowContext)
		ffxOpticalflowContextDestroy(&m_OpticalFlowContext.value());

	if (m_TexSharedOpticalFlowVector)
		m_SharedBackendInterface.fpDestroyResource(&m_SharedBackendInterface, *m_TexSharedOpticalFlowVector, *m_SharedEffectContextId);

	if (m_TexSharedOpticalFlowSCD)
		m_SharedBackendInterface.fpDestroyResource(&m_SharedBackendInterface, *m_TexSharedOpticalFlowSCD, *m_SharedEffectContextId);
}
