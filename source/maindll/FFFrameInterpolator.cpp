#include <dxgi1_6.h>
#include <numbers>
#include "NGX/NvNGX.h"
#include "FFFrameInterpolator.h"
#include "Util.h"

bool g_EnableDebugOverlay = false;
bool g_EnableDebugTearLines = false;
bool g_EnableInterpolatedFramesOnly = false;

extern "C" void __declspec(dllexport) RefreshGlobalConfiguration()
{
	g_EnableDebugOverlay = Util::GetSetting(L"EnableDebugOverlay", false);
	g_EnableDebugTearLines = Util::GetSetting(L"EnableDebugTearLines", false);
	g_EnableInterpolatedFramesOnly = Util::GetSetting(L"EnableInterpolatedFramesOnly", false);
}

FFFrameInterpolator::FFFrameInterpolator(uint32_t OutputWidth, uint32_t OutputHeight)
	: m_SwapchainWidth(OutputWidth),
	  m_SwapchainHeight(OutputHeight)
{
	RefreshGlobalConfiguration();
}

FFFrameInterpolator::~FFFrameInterpolator()
{
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
		FfxOpticalflowDispatchDescription fsrOfDispatchDesc = {};
		FFInterpolatorDispatchParameters fsrFiDispatchDesc = {};

		if (!BuildOpticalFlowParameters(&fsrOfDispatchDesc, NGXParameters))
			return FFX_ERROR_INVALID_ARGUMENT;

		if (!BuildFrameInterpolationParameters(&fsrFiDispatchDesc, NGXParameters))
			return FFX_ERROR_INVALID_ARGUMENT;

		fsrFiDispatchDesc.DebugView = g_EnableDebugOverlay;
		fsrFiDispatchDesc.DebugTearLines = g_EnableDebugTearLines;

		// Record commands
		if (auto status = ffxOpticalflowContextDispatch(&m_OpticalFlowContext.value(), &fsrOfDispatchDesc); status != FFX_OK)
			return status;

		if (auto status = m_FrameInterpolatorContext->Dispatch(fsrFiDispatchDesc); status != FFX_OK)
			return status;

		if (fsrFiDispatchDesc.DebugView || g_EnableInterpolatedFramesOnly)
			gameBackBufferResource = fsrFiDispatchDesc.OutputInterpolatedColorBuffer;

		return FFX_OK;
	}();

	if (dispatchStatus == FFX_OK && gameRealOutputResource.resource && gameBackBufferResource.resource)
		CopyTexture(GetActiveCommandList(), &gameRealOutputResource, &gameBackBufferResource);

 	return dispatchStatus;
}

void FFFrameInterpolator::Create(NGXInstanceParameters *NGXParameters)
{
	if (CreateBackend(NGXParameters) != FFX_OK)
		throw std::runtime_error("Failed to create backend context.");

	if (CreateOpticalFlowContext() != FFX_OK)
		throw std::runtime_error("Failed to create optical flow context.");

	m_FrameInterpolatorContext.emplace(
		m_FrameInterpolationBackendInterface,
		m_SharedBackendInterface,
		*m_SharedEffectContextId,
		m_SwapchainWidth,
		m_SwapchainHeight);
}

void FFFrameInterpolator::Destroy()
{
	m_FrameInterpolatorContext.reset();
	DestroyOpticalFlowContext();
	DestroyBackend();
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

	//
	// At some point in time a Dying Light 2 patch fixed its depth resource issue. The game now passes
	// the correct resource to Streamline. Prior to this, depth was being converted to RGBA8.
	//
	// On the other hand DL2's HUD-less resource is still screwed up. There appears to be two separate
	// typos (copy-paste?) resulting in depth being bound as HUD-less. Manually patching game code
	// (x86 instructions) is an effective workaround but comes with a catch: DL2's actual "HUDLESS"
	// resource is untonemapped and therefore unusable in FSR FG.
	//
	const static bool isDyingLight2 = GetModuleHandleW(L"DyingLightGame_x64_rwdi.exe") != nullptr;

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

	desc.RenderSize = { m_PreUpscaleRenderWidth, m_PreUpscaleRenderHeight };
	desc.OutputSize = { m_SwapchainWidth, m_SwapchainHeight };

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

	if (!LoadTextureFromNGXParameters(NGXParameters, "DLSSG.Depth", &desc.InputDepth, FFX_RESOURCE_STATE_COPY_DEST))
		return false;

	if (!LoadTextureFromNGXParameters(NGXParameters, "DLSSG.MVecs", &desc.InputMotionVectors, FFX_RESOURCE_STATE_COPY_DEST))
		return false;

	desc.InputOpticalFlowVector = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_TexSharedOpticalFlowVector);
	desc.InputOpticalFlowSceneChangeDetection = m_SharedBackendInterface.fpGetResource(&m_SharedBackendInterface, *m_TexSharedOpticalFlowSCD);

	desc.OpticalFlowScale = { 1.0f / m_PostUpscaleRenderWidth, 1.0f / m_PostUpscaleRenderHeight };
	desc.OpticalFlowBlockSize = 8;

	const FfxDimensions2D mvecExtents = {
		NGXParameters->GetUIntOrDefault("DLSSG.MVecsSubrectWidth", desc.InputMotionVectors.description.width),
		NGXParameters->GetUIntOrDefault("DLSSG.MVecsSubrectHeight", desc.InputMotionVectors.description.height),
	};

	desc.MotionVectorsFullResolution = m_PostUpscaleRenderWidth == mvecExtents.width && m_PostUpscaleRenderHeight == mvecExtents.height;
	desc.MotionVectorJitterCancellation = NGXParameters->GetUIntOrDefault("DLSSG.MvecJittered", 0) != 0;
	desc.MotionVectorsDilated = NGXParameters->GetUIntOrDefault("DLSSG.MvecDilated", 0) != 0;

	desc.MotionVectorScale = {
		NGXParameters->GetFloatOrDefault("DLSSG.MvecScaleX", 1.0f),
		NGXParameters->GetFloatOrDefault("DLSSG.MvecScaleY", 1.0f),
	};

	desc.MotionVectorJitterOffsets = {
		NGXParameters->GetFloatOrDefault("DLSSG.JitterOffsetX", 0.0f),
		NGXParameters->GetFloatOrDefault("DLSSG.JitterOffsetY", 0.0f),
	};

	desc.HDR = NGXParameters->GetUIntOrDefault("DLSSG.ColorBuffersHDR", 0) != 0;
	desc.DepthInverted = NGXParameters->GetUIntOrDefault("DLSSG.DepthInverted", 0) != 0;
	desc.Reset = NGXParameters->GetUIntOrDefault("DLSSG.Reset", 0) != 0;

	auto loadCameraMatrix = [&]()
	{
		const bool isOrthographicProjection = NGXParameters->GetUIntOrDefault("DLSSG.OrthoProjection", 0) != 0;

		if (isOrthographicProjection)
			return false;

		float(*cameraViewToClip)[4] = nullptr;
		NGXParameters->GetVoidPointer("DLSSG.CameraViewToClip", reinterpret_cast<void **>(&cameraViewToClip));

		if (!cameraViewToClip)
			return false;

		float projMatrix[4][4];
		memcpy(projMatrix, cameraViewToClip, sizeof(projMatrix));

		// BUG: Various RTX Remix-based games pass in an identity matrix which is completely useless. No
		// idea why.
		const bool isEmptyOrIdentityMatrix = [&]()
		{
			float m[4][4] = {};
			if (memcmp(projMatrix, m, sizeof(m)) == 0)
				return true;

			m[0][0] = m[1][1] = m[2][2] = m[3][3] = 1.0f;
			return memcmp(projMatrix, m, sizeof(m)) == 0;
		}();

		if (isEmptyOrIdentityMatrix)
			return false;

		// BUG: Indiana Jones and the Great Circle passes in what appears to be column-major matrices.
		// Streamline expects row-major and so do we.
		const static bool isTheGreatCircle = GetModuleHandleW(L"TheGreatCircle.exe") != nullptr;

		for (int i = 0; i < 4 && isTheGreatCircle; i++)
		{
			for (int j = i + 1; j < 4; j++)
				std::swap(projMatrix[i][j], projMatrix[j][i]);
		}

		// a 0 0 0
		// 0 b 0 0
		// 0 0 c e
		// 0 0 d 0
		const double b = projMatrix[1][1];
		const double c = projMatrix[2][2];
		const double d = projMatrix[3][2];
		const double e = projMatrix[2][3];

		if (e < 0.0)
		{
			desc.CameraNear = static_cast<float>((c == 0.0) ? 0.0 : (d / c));
			desc.CameraFar = static_cast<float>(d / (c + 1.0));
		}
		else
		{
			desc.CameraNear = static_cast<float>((c == 0.0) ? 0.0 : (-d / c));
			desc.CameraFar = static_cast<float>(-d / (c - 1.0));
		}

		if (desc.DepthInverted)
			std::swap(desc.CameraNear, desc.CameraFar);

		desc.CameraFovAngleVertical = static_cast<float>(2.0 * std::atan(1.0 / b));
		desc.ViewSpaceToMetersFactor = 1.0f;
		return true;
	};

	if (!loadCameraMatrix())
	{
		// Some games pass in CameraFOV as degrees. Some games pass in CameraFOV as radians. Which is
		// correct? Who knows. I sure as hell don't.
		desc.CameraFovAngleVertical = NGXParameters->GetFloatOrDefault("DLSSG.CameraFOV", 0.0f);

		// BUG: RTX Remix-based games pass in a FOV of 0. This is a kludge.
		if (desc.CameraFovAngleVertical == 0.0f)
			desc.CameraFovAngleVertical = 90.0f;

		if (desc.CameraFovAngleVertical > 10.0f)
			desc.CameraFovAngleVertical *= std::numbers::pi_v<float> / 180.0f;

		desc.CameraNear = NGXParameters->GetFloatOrDefault("DLSSG.CameraNear", 0.0f);
		desc.CameraFar = NGXParameters->GetFloatOrDefault("DLSSG.CameraFar", 0.0f);
		desc.ViewSpaceToMetersFactor = 1.0f;
	}

	if (desc.CameraNear != 0.0f && desc.CameraFar == 0.0f)
	{
		// A CameraFar value of zero indicates an infinite far plane. Due to a bug in FSR's
		// setupDeviceDepthToViewSpaceDepthParams function, CameraFar must always be greater than
		// CameraNear when in use.
		desc.DepthPlaneInfinite = true;
		desc.CameraFar = desc.CameraNear + 1.0f;
	}

	desc.MinMaxLuminance = m_HDRLuminanceRange;

	return true;
}

FfxErrorCode FFFrameInterpolator::CreateBackend(NGXInstanceParameters *NGXParameters)
{
	const uint32_t maxContexts = 3; // Assume 3 contexts per interface

	auto status = InitializeBackendInterface(&m_SharedBackendInterface, maxContexts, NGXParameters);

	if (status != FFX_OK)
		return status;

	status = InitializeBackendInterface(&m_FrameInterpolationBackendInterface, maxContexts, NGXParameters);

	if (status != FFX_OK)
		return status;

	status = m_SharedBackendInterface.fpCreateBackendContext(
		&m_SharedBackendInterface,
		FFX_EFFECT_FRAMEINTERPOLATION,
		nullptr,
		&m_SharedEffectContextId.emplace());

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

FfxErrorCode FFFrameInterpolator::CreateOpticalFlowContext()
{
	// Set up configuration for optical flow
	FfxOpticalflowContextDescription fsrOfDescription = {
		.backendInterface = m_FrameInterpolationBackendInterface,
		.flags = 0,
		.resolution = { m_SwapchainWidth, m_SwapchainHeight },
	};

	auto status = ffxOpticalflowContextCreate(&m_OpticalFlowContext.emplace(), &fsrOfDescription);

	if (status != FFX_OK)
	{
		m_OpticalFlowContext.reset();
		return status;
	}

	FfxOpticalflowSharedResourceDescriptions fsrOfSharedDescriptions = {};
	status = ffxOpticalflowGetSharedResourceDescriptions(&m_OpticalFlowContext.value(), &fsrOfSharedDescriptions);

	if (status != FFX_OK)
		return status;

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
