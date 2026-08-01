#include <ffx_api/dx12/ffx_api_dx12.h>
#include <ffx_api/ffx_framegeneration.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <limits>
#include <vector>

#include "FSR4DriverUpgrade.h"
#include "FSR4FrameInterpolatorDX.h"
#include "NGX/NvNGX.h"

namespace
{
	using CreateContextFn = PfnFfxCreateContext;
	using DestroyContextFn = PfnFfxDestroyContext;
	using ConfigureFn = PfnFfxConfigure;
	using QueryFn = PfnFfxQuery;
	using DispatchFn = PfnFfxDispatch;

	HMODULE LoadFSR4Runtime()
	{
		wchar_t path[MAX_PATH] = {};
		HMODULE implementation = nullptr;
		if (!GetModuleHandleExW(
				GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
				reinterpret_cast<LPCWSTR>(&LoadFSR4Runtime),
				&implementation) ||
			!GetModuleFileNameW(implementation, path, MAX_PATH))
			return nullptr;

		std::filesystem::path loader(path);
		loader = loader.parent_path() / L"amd_fidelityfx_loader_dx12.dll";
		return LoadLibraryW(loader.c_str());
	}

	FfxApiResource GetResource(ID3D12Resource *resource, uint32_t state)
	{
		return ffxApiGetResourceDX12(resource, state);
	}

	struct CameraCalibration
	{
		float nearPlane = 0.0f;
		float farPlane = 0.0f;
		float verticalFov = 1.5707963f;
		bool infiniteDepth = false;
		bool usedProjectionMatrix = false;
	};

	CameraCalibration GetCameraCalibration(NGXInstanceParameters *parameters)
	{
		CameraCalibration calibration{};
		if (!parameters)
			return calibration;

		const bool depthInverted = parameters->GetUIntOrDefault("DLSSG.DepthInverted", 0) != 0;
		bool matrixWasUsable = false;
		float(*viewToClip)[4] = nullptr;
		parameters->GetVoidPointer("DLSSG.CameraViewToClip", reinterpret_cast<void **>(&viewToClip));
		if (viewToClip && !parameters->GetUIntOrDefault("DLSSG.OrthoProjection", 0))
		{
			float matrix[4][4] = {};
			memcpy(matrix, viewToClip, sizeof(matrix));
			float zero[4][4] = {};
			float identity[4][4] = {};
			identity[0][0] = identity[1][1] = identity[2][2] = identity[3][3] = 1.0f;
			if (memcmp(matrix, identity, sizeof(matrix)) != 0 &&
				memcmp(matrix, zero, sizeof(matrix)) != 0)
			{
				const double b = matrix[1][1];
				const double c = matrix[2][2];
				const double d = matrix[3][2];
				const double e = matrix[2][3];
				if (std::isfinite(b) && std::isfinite(c) && std::isfinite(d) && std::isfinite(e) && b != 0.0)
				{
					if (e < 0.0)
					{
						calibration.nearPlane = static_cast<float>(c == 0.0 ? 0.0 : d / c);
						calibration.farPlane = static_cast<float>(d / (c + 1.0));
					}
					else
					{
						calibration.nearPlane = static_cast<float>(c == 0.0 ? 0.0 : -d / c);
						calibration.farPlane = static_cast<float>(-d / (c - 1.0));
					}
					if (depthInverted)
						std::swap(calibration.nearPlane, calibration.farPlane);
					calibration.verticalFov = static_cast<float>(2.0 * std::atan(1.0 / b));
					matrixWasUsable = std::isfinite(calibration.nearPlane) && std::isfinite(calibration.farPlane) &&
						std::isfinite(calibration.verticalFov) && calibration.verticalFov > 0.0f;
				}
			}
		}

		if (!matrixWasUsable)
		{
			calibration.nearPlane = parameters->GetFloatOrDefault("DLSSG.CameraNear", 0.0f);
			calibration.farPlane = parameters->GetFloatOrDefault("DLSSG.CameraFar", 0.0f);
			calibration.verticalFov = parameters->GetFloatOrDefault("DLSSG.CameraFOV", 0.0f);
			if (calibration.verticalFov == 0.0f)
				calibration.verticalFov = 90.0f;
			if (calibration.verticalFov > 10.0f)
				calibration.verticalFov *= 3.14159265358979323846f / 180.0f;
		}
		calibration.usedProjectionMatrix = matrixWasUsable;

		if (calibration.nearPlane != 0.0f && calibration.farPlane == 0.0f)
		{
			calibration.infiniteDepth = true;
			calibration.farPlane = calibration.nearPlane + 1.0f;
		}
		return calibration;
	}

	float GetViewSpaceToMetersFactor(NGXInstanceParameters *parameters)
	{
		const auto missing = std::numeric_limits<float>::quiet_NaN();
		const auto dlssgValue = parameters->GetFloatOrDefault("DLSSG.ViewSpaceToMetersFactor", missing);
		if (std::isfinite(dlssgValue) && dlssgValue > 0.0f)
			return dlssgValue;
		const auto fsrValue = parameters->GetFloatOrDefault("FSR.viewSpaceToMetersFactor", missing);
		return std::isfinite(fsrValue) && fsrValue > 0.0f ? fsrValue : 1.0f;
	}

}

FSR4FrameInterpolatorDX::FSR4FrameInterpolatorDX(ID3D12Device *device, uint32_t outputWidth, uint32_t outputHeight)
	: m_Device(device), m_OutputWidth(outputWidth), m_OutputHeight(outputHeight)
{
	FSR4DriverUpgrade::Initialize(m_Device);
	m_Runtime = LoadFSR4Runtime();
	if (!m_Runtime)
	{
		spdlog::info("FSR4 FG runtime loader was not found; using FSR3 FG.");
		return;
	}

	m_CreateContext = GetProcAddress(static_cast<HMODULE>(m_Runtime), "ffxCreateContext");
	m_DestroyContext = GetProcAddress(static_cast<HMODULE>(m_Runtime), "ffxDestroyContext");
	m_Configure = GetProcAddress(static_cast<HMODULE>(m_Runtime), "ffxConfigure");
	m_Query = GetProcAddress(static_cast<HMODULE>(m_Runtime), "ffxQuery");
	m_Dispatch = GetProcAddress(static_cast<HMODULE>(m_Runtime), "ffxDispatch");
	if (!IsAvailable())
	{
		spdlog::warn("FSR4 FG runtime loader is missing a required API export; using FSR3 FG.");
		FreeLibrary(static_cast<HMODULE>(m_Runtime));
		m_Runtime = nullptr;
	}
}

FSR4FrameInterpolatorDX::~FSR4FrameInterpolatorDX()
{
	DestroyContext();
	if (m_Runtime)
		FreeLibrary(static_cast<HMODULE>(m_Runtime));
}

bool FSR4FrameInterpolatorDX::IsAvailable() const
{
	return m_Runtime && m_CreateContext && m_DestroyContext && m_Configure && m_Query && m_Dispatch;
}

bool FSR4FrameInterpolatorDX::CreateContext(
	ID3D12Resource *color,
	ID3D12Resource *depth,
	ID3D12Resource *motionVectors,
	NGXInstanceParameters *parameters)
{
	if (m_Context || !color || !depth || !motionVectors)
		return m_Context != nullptr;
	if (m_FSR4ProviderUnavailable)
		return false;

	ffxQueryDescGetVersions versions{};
	versions.header.type = FFX_API_QUERY_DESC_TYPE_GET_VERSIONS;
	versions.createDescType = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION;
	versions.device = m_Device;
	uint64_t count = 0;
	versions.outputCount = &count;
	const auto queryResult = static_cast<QueryFn>(m_Query)(nullptr, &versions.header);
	if (queryResult != FFX_API_RETURN_OK || count == 0)
	{
		m_FSR4ProviderUnavailable = true;
		spdlog::warn("FSR4 FG provider query failed ({:X}); using FSR3 FG.", static_cast<uint32_t>(queryResult));
		return false;
	}

	std::vector<uint64_t> ids(count);
	std::vector<const char *> names(count);
	versions.versionIds = ids.data();
	versions.versionNames = names.data();
	const auto versionsResult = static_cast<QueryFn>(m_Query)(nullptr, &versions.header);
	if (versionsResult != FFX_API_RETURN_OK)
	{
		m_FSR4ProviderUnavailable = true;
		spdlog::warn("FSR4 FG provider enumeration failed ({:X}); using FSR3 FG.", static_cast<uint32_t>(versionsResult));
		return false;
	}

	for (const auto *name : names)
		spdlog::info("FSR frame generation provider detected: {}", name ? name : "(unnamed)");

	if (!FSR4DriverUpgrade::IsFGProviderUpgraded())
	{
		m_FSR4ProviderUnavailable = true;
		spdlog::info("FSR4 FG driver provider was not activated; using FSR3 FG.");
		return false;
	}

	// The upgraded provider intentionally keeps the public FSR 3.1.6 API version.
	const auto index = size_t{ 0 };
	ffxCreateBackendDX12Desc backend{};
	backend.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12;
	backend.device = m_Device;

	ffxOverrideVersion override{};
	override.header.type = FFX_API_DESC_TYPE_OVERRIDE_VERSION;
	override.versionId = ids[index];
	backend.header.pNext = &override.header;

	ffxCreateContextDescFrameGeneration create{};
	create.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION;
	create.header.pNext = &backend.header;
	create.displaySize = { m_OutputWidth, m_OutputHeight };
	create.maxRenderSize = { static_cast<uint32_t>(depth->GetDesc().Width), depth->GetDesc().Height };
	create.backBufferFormat = ffxApiGetSurfaceFormatDX12(color->GetDesc().Format);
	const auto cameraCalibration = GetCameraCalibration(parameters);
	if (parameters->GetUIntOrDefault("DLSSG.DepthInverted", 0))
		create.flags |= FFX_FRAMEGENERATION_ENABLE_DEPTH_INVERTED;
	if (cameraCalibration.infiniteDepth)
		create.flags |= FFX_FRAMEGENERATION_ENABLE_DEPTH_INFINITE;
	spdlog::info(
		"FSR4 FG camera calibration: source={}, near={}, far={}, verticalFov={}, infiniteDepth={}, meters={}",
		cameraCalibration.usedProjectionMatrix ? "projection matrix" : "DLSSG parameters",
		cameraCalibration.nearPlane,
		cameraCalibration.farPlane,
		cameraCalibration.verticalFov,
		cameraCalibration.infiniteDepth,
		GetViewSpaceToMetersFactor(parameters));
	if (parameters->GetUIntOrDefault("DLSSG.MvecJittered", 0))
		create.flags |= FFX_FRAMEGENERATION_ENABLE_MOTION_VECTORS_JITTER_CANCELLATION;

	// DLSSG can provide motion vectors at either render or display resolution.
	// FSR4 needs this declared when the context is created; otherwise it scales
	// display-resolution vectors as render-resolution vectors, producing inverse
	// camera-motion trails and history flashes.
	uint32_t mvecWidth = parameters->GetUIntOrDefault("DLSSG.MVecsSubrectWidth", 0);
	uint32_t mvecHeight = parameters->GetUIntOrDefault("DLSSG.MVecsSubrectHeight", 0);
	if (mvecWidth == 0 || mvecHeight == 0)
	{
		const auto mvecDesc = motionVectors->GetDesc();
		mvecWidth = static_cast<uint32_t>(mvecDesc.Width);
		mvecHeight = mvecDesc.Height;
	}
	if (mvecWidth == m_OutputWidth && mvecHeight == m_OutputHeight)
	{
		create.flags |= FFX_FRAMEGENERATION_ENABLE_DISPLAY_RESOLUTION_MOTION_VECTORS;
		spdlog::info("FSR4 FG is using display-resolution DLSSG motion vectors ({}x{}).", mvecWidth, mvecHeight);
	}
	else
		spdlog::info("FSR4 FG is using render-resolution DLSSG motion vectors ({}x{}).", mvecWidth, mvecHeight);

	if (parameters->GetUIntOrDefault("DLSSG.ColorBuffersHDR", 0))
		create.flags |= FFX_FRAMEGENERATION_ENABLE_HIGH_DYNAMIC_RANGE;

	const auto result = static_cast<CreateContextFn>(m_CreateContext)(&m_Context, &create.header, nullptr);
	if (result != FFX_API_RETURN_OK)
	{
		m_FSR4ProviderUnavailable = true;
		spdlog::warn("FSR4 FG context creation failed ({:X}); using FSR3 FG.", static_cast<uint32_t>(result));
		m_Context = nullptr;
	}
	else
		spdlog::info("FSR4 FG context created through the upgraded FSR3 API provider {}.", names[index]);
	return m_Context != nullptr;
}

void FSR4FrameInterpolatorDX::DestroyContext()
{
	if (m_Context)
		static_cast<DestroyContextFn>(m_DestroyContext)(&m_Context, nullptr);
	m_Context = nullptr;
}

bool FSR4FrameInterpolatorDX::Dispatch(ID3D12GraphicsCommandList *commandList, NGXInstanceParameters *parameters)
{
	if (!IsAvailable() || !commandList || !parameters || m_FSR4DispatchUnavailable)
		return false;

	ID3D12Resource *color = nullptr;
	ID3D12Resource *hudLess = nullptr;
	ID3D12Resource *depth = nullptr;
	ID3D12Resource *motionVectors = nullptr;
	ID3D12Resource *output = nullptr;
	// FSR4's explicit dispatch path has no separate HUD composition input. Use the
	// final backbuffer so generated frames retain the game's UI. HUDLess remains a
	// fallback for games that do not expose the final color resource.
	parameters->GetVoidPointer("DLSSG.Backbuffer", reinterpret_cast<void **>(&color));
	parameters->GetVoidPointer("DLSSG.HUDLess", reinterpret_cast<void **>(&hudLess));
	if (!color)
		color = hudLess;
	parameters->GetVoidPointer("DLSSG.Depth", reinterpret_cast<void **>(&depth));
	parameters->GetVoidPointer("DLSSG.MVecs", reinterpret_cast<void **>(&motionVectors));
	parameters->GetVoidPointer("DLSSG.OutputInterpolated", reinterpret_cast<void **>(&output));
	if (!color || !depth || !motionVectors || !output)
	{
		m_FSR4DispatchUnavailable = true;
		spdlog::warn("FSR4 FG is missing a required DLSSG resource; using FSR3 FG.");
		return false;
	}
	if (!CreateContext(color, depth, motionVectors, parameters))
		return false;

	const auto frameId = ++m_FrameId;
	ffxConfigureDescFrameGeneration configure{};
	configure.header.type = FFX_API_CONFIGURE_DESC_TYPE_FRAMEGENERATION;
	configure.frameGenerationEnabled = true;
	configure.flags = FFX_FRAMEGENERATION_FLAG_NO_SWAPCHAIN_CONTEXT_NOTIFY;
	configure.frameID = frameId;
	// The API uses this companion surface to separate UI from the final backbuffer.
	// Without it games that provide HUDLess (such as FF16) can feed mismatched UI and
	// scene histories to FSR4 during camera motion.
	if (hudLess && hudLess != color)
		configure.HUDLessColor = GetResource(hudLess, FFX_API_RESOURCE_STATE_COPY_DEST);
	const auto configureResult = static_cast<ConfigureFn>(m_Configure)(&m_Context, &configure.header);
	if (configureResult != FFX_API_RETURN_OK)
	{
		m_FSR4DispatchUnavailable = true;
		spdlog::warn("FSR4 FG configuration failed ({:X}); using FSR3 FG.", static_cast<uint32_t>(configureResult));
		return false;
	}

	ffxCreateBackendDX12Desc prepareBackend{};
	prepareBackend.header.type = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12;
	prepareBackend.device = m_Device;

#pragma warning(push)
#pragma warning(disable: 4996)
	ffxDispatchDescFrameGenerationPrepare prepare{};
	prepare.header.type = FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION_PREPARE;
	prepare.header.pNext = &prepareBackend.header;
	prepare.commandList = commandList;
	prepare.frameID = frameId;
	prepare.renderSize = { static_cast<uint32_t>(depth->GetDesc().Width), depth->GetDesc().Height };
	prepare.jitterOffset = {
		parameters->GetFloatOrDefault("DLSSG.JitterOffsetX", 0.0f),
		parameters->GetFloatOrDefault("DLSSG.JitterOffsetY", 0.0f),
	};
	prepare.motionVectorScale = {
		parameters->GetFloatOrDefault("DLSSG.MvecScaleX", 1.0f),
		parameters->GetFloatOrDefault("DLSSG.MvecScaleY", 1.0f),
	};
	prepare.unused_reset = parameters->GetUIntOrDefault("DLSSG.Reset", 0) != 0;
	const auto cameraCalibration = GetCameraCalibration(parameters);
	prepare.cameraNear = cameraCalibration.nearPlane;
	prepare.cameraFar = cameraCalibration.farPlane;
	prepare.cameraFovAngleVertical = cameraCalibration.verticalFov;
	prepare.viewSpaceToMetersFactor = GetViewSpaceToMetersFactor(parameters);
	prepare.frameTimeDelta = parameters->GetFloatOrDefault("DLSSG.FrameTimeDeltaInMsec", 16.6667f);
	// DLSSG supplies these surfaces in COPY_DEST. Declaring the real state lets the
	// FidelityFX backend synchronize them before the FG compute pass reads them.
	prepare.depth = GetResource(depth, FFX_API_RESOURCE_STATE_COPY_DEST);
	prepare.motionVectors = GetResource(motionVectors, FFX_API_RESOURCE_STATE_COPY_DEST);
	const auto prepareResult = static_cast<DispatchFn>(m_Dispatch)(&m_Context, &prepare.header);
	if (prepareResult != FFX_API_RETURN_OK)
	{
		m_FSR4DispatchUnavailable = true;
		spdlog::warn("FSR4 FG prepare failed ({:X}); using FSR3 FG.", static_cast<uint32_t>(prepareResult));
		return false;
	}

	ffxDispatchDescFrameGeneration dispatch{};
	dispatch.header.type = FFX_API_DISPATCH_DESC_TYPE_FRAMEGENERATION;
	dispatch.commandList = commandList;
	dispatch.presentColor = GetResource(color, FFX_API_RESOURCE_STATE_COMPUTE_READ);
	dispatch.outputs[0] = GetResource(output, FFX_API_RESOURCE_STATE_UNORDERED_ACCESS);
	dispatch.numGeneratedFrames = 1;
	dispatch.reset = prepare.unused_reset;
#pragma warning(pop)
	dispatch.backbufferTransferFunction = parameters->GetUIntOrDefault("DLSSG.ColorBuffersHDR", 0) ? FFX_API_BACKBUFFER_TRANSFER_FUNCTION_PQ : FFX_API_BACKBUFFER_TRANSFER_FUNCTION_SRGB;
	dispatch.minMaxLuminance[0] = 0.0001f;
	dispatch.minMaxLuminance[1] = 1000.0f;
	dispatch.generationRect = { 0, 0, static_cast<int32_t>(m_OutputWidth), static_cast<int32_t>(m_OutputHeight) };
	dispatch.frameID = frameId;
	const auto dispatchResult = static_cast<DispatchFn>(m_Dispatch)(&m_Context, &dispatch.header);
	if (dispatchResult != FFX_API_RETURN_OK)
	{
		m_FSR4DispatchUnavailable = true;
		spdlog::warn("FSR4 FG dispatch failed ({:X}); using FSR3 FG.", static_cast<uint32_t>(dispatchResult));
		return false;
	}

	if (!m_LoggedFSR4Active)
	{
		spdlog::info("FSR4 FG dispatch is active.");
		m_LoggedFSR4Active = true;
	}
	return true;
}
