#include <directx/d3dx12.h>
#include <FidelityFX/host/backends/dx12/ffx_dx12.h>
#include "NGX/NvNGX.h"
#include "FFInterfaceWrapper.h"

D3D12_RESOURCE_FLAGS ffxGetDX12ResourceFlags(FfxResourceUsage flags);
D3D12_RESOURCE_STATES ffxGetDX12StateFromResourceState(FfxResourceStates state);
DXGI_FORMAT ffxGetDX12FormatFromSurfaceFormat(FfxSurfaceFormat surfaceFormat);
ID3D12Resource *getDX12ResourcePtr(BackendContext_DX12 *backendContext, int32_t resourceIndex);

static DXGI_FORMAT convertFormatUav(DXGI_FORMAT format);
static DXGI_FORMAT convertFormatSrv(DXGI_FORMAT format);

FFInterfaceWrapper::FFInterfaceWrapper()
{
	memset(this, 0, sizeof(*this));
}

FFInterfaceWrapper::~FFInterfaceWrapper()
{
	delete[] reinterpret_cast<uint8_t *>(GetUserData());
}

FfxErrorCode FFInterfaceWrapper::Initialize(ID3D12Device *Device, uint32_t MaxContexts, NGXInstanceParameters *NGXParameters)
{
	const auto fsrDevice = ffxGetDeviceDX12(Device);
	const auto scratchSize = ffxGetScratchMemorySizeDX12(MaxContexts);

	// FFX provides zero means to store user data in backend interfaces. Stuff it before the actual scratch buffer memory.
	auto scratchMemory = new uint8_t[sizeof(UserDataHack) + scratchSize];
	auto userData = new (scratchMemory) UserDataHack;

	// Adjust the allocation offset
	auto ffxScratchMemory = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(scratchMemory) + sizeof(UserDataHack));
	memset(ffxScratchMemory, 0, scratchSize);

	auto result = ffxGetInterfaceDX12(this, fsrDevice, ffxScratchMemory, scratchSize, MaxContexts);

	if (result == FFX_OK && NGXParameters)
	{
		NGXParameters->GetVoidPointer("ResourceAllocCallback", reinterpret_cast<void **>(&userData->m_NGXAllocCallback));
		NGXParameters->GetVoidPointer("ResourceReleaseCallback", reinterpret_cast<void **>(&userData->m_NGXFreeCallback));

		if (userData->m_NGXAllocCallback && userData->m_NGXFreeCallback)
		{
			fpCreateResource = CustomCreateResourceDX12;
			fpDestroyResource = CustomDestroyResourceDX12;
		}
	}

	return result;
}

FFInterfaceWrapper::UserDataHack *FFInterfaceWrapper::GetUserData()
{
	if (!scratchBuffer)
		return nullptr;

	return reinterpret_cast<UserDataHack *>(reinterpret_cast<uintptr_t>(scratchBuffer) - sizeof(UserDataHack));
}

//
// Everything after this point is lifted verbatim from /sdk/src/backends/dx12/ffx_dx12.cpp.
//
// There's simply no other way to implement custom resource creation and destruction callbacks.
//
FfxErrorCode FFInterfaceWrapper::CustomCreateResourceDX12(
	FfxInterface *backendInterface,
	const FfxCreateResourceDescription *createResourceDescription,
	FfxUInt32 effectContextId,
	FfxResourceInternal *outTexture)
{
	FFX_ASSERT(NULL != backendInterface);
	FFX_ASSERT(NULL != createResourceDescription);
	FFX_ASSERT(NULL != outTexture);

	BackendContext_DX12 *backendContext = (BackendContext_DX12 *)backendInterface->scratchBuffer;
	BackendContext_DX12::EffectContext& effectContext = backendContext->pEffectContexts[effectContextId];
	ID3D12Device *dx12Device = backendContext->device;

	FFX_ASSERT(NULL != dx12Device);

	D3D12_HEAP_PROPERTIES dx12HeapProperties = {};
	dx12HeapProperties.Type = (createResourceDescription->heapType == FFX_HEAP_TYPE_DEFAULT) ? D3D12_HEAP_TYPE_DEFAULT
																							 : D3D12_HEAP_TYPE_UPLOAD;

	FFX_ASSERT(effectContext.nextStaticResource + 1 < effectContext.nextDynamicResource);

	outTexture->internalIndex = effectContext.nextStaticResource++;
	BackendContext_DX12::Resource *backendResource = &backendContext->pResources[outTexture->internalIndex];
	backendResource->resourceDescription = createResourceDescription->resourceDescription;

	D3D12_RESOURCE_DESC dx12ResourceDescription = {};
	dx12ResourceDescription.Format = DXGI_FORMAT_UNKNOWN;
	dx12ResourceDescription.Width = 1;
	dx12ResourceDescription.Height = 1;
	dx12ResourceDescription.MipLevels = 1;
	dx12ResourceDescription.DepthOrArraySize = 1;
	dx12ResourceDescription.SampleDesc.Count = 1;
	dx12ResourceDescription.Flags = ffxGetDX12ResourceFlags(backendResource->resourceDescription.usage);

	switch (createResourceDescription->resourceDescription.type)
	{

	case FFX_RESOURCE_TYPE_BUFFER:
		dx12ResourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		dx12ResourceDescription.Width = createResourceDescription->resourceDescription.width;
		dx12ResourceDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		break;

	case FFX_RESOURCE_TYPE_TEXTURE1D:
		dx12ResourceDescription.Format = ffxGetDX12FormatFromSurfaceFormat(createResourceDescription->resourceDescription.format);
		dx12ResourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D;
		dx12ResourceDescription.Width = createResourceDescription->resourceDescription.width;
		dx12ResourceDescription.DepthOrArraySize = static_cast<uint16_t>(createResourceDescription->resourceDescription.depth);
		dx12ResourceDescription.MipLevels = static_cast<uint16_t>(createResourceDescription->resourceDescription.mipCount);
		break;

	case FFX_RESOURCE_TYPE_TEXTURE2D:
		dx12ResourceDescription.Format = ffxGetDX12FormatFromSurfaceFormat(createResourceDescription->resourceDescription.format);
		dx12ResourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
		dx12ResourceDescription.Width = createResourceDescription->resourceDescription.width;
		dx12ResourceDescription.Height = createResourceDescription->resourceDescription.height;
		dx12ResourceDescription.DepthOrArraySize = static_cast<uint16_t>(createResourceDescription->resourceDescription.depth);
		dx12ResourceDescription.MipLevels = static_cast<uint16_t>(createResourceDescription->resourceDescription.mipCount);
		break;

	case FFX_RESOURCE_TYPE_TEXTURE_CUBE:
	case FFX_RESOURCE_TYPE_TEXTURE3D:
		dx12ResourceDescription.Format = ffxGetDX12FormatFromSurfaceFormat(createResourceDescription->resourceDescription.format);
		dx12ResourceDescription.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
		dx12ResourceDescription.Width = createResourceDescription->resourceDescription.width;
		dx12ResourceDescription.Height = createResourceDescription->resourceDescription.height;
		dx12ResourceDescription.DepthOrArraySize = static_cast<uint16_t>(createResourceDescription->resourceDescription.depth);
		dx12ResourceDescription.MipLevels = static_cast<uint16_t>(createResourceDescription->resourceDescription.mipCount);
		break;

	default:
		break;
	}

	ID3D12Resource *dx12Resource = nullptr;
	if (createResourceDescription->heapType == FFX_HEAP_TYPE_UPLOAD)
	{

		D3D12_PLACED_SUBRESOURCE_FOOTPRINT dx12Footprint = {};

		UINT rowCount;
		UINT64 rowSizeInBytes;
		UINT64 totalBytes;

		dx12Device->GetCopyableFootprints(&dx12ResourceDescription, 0, 1, 0, &dx12Footprint, &rowCount, &rowSizeInBytes, &totalBytes);

		D3D12_HEAP_PROPERTIES dx12UploadHeapProperties = {};
		dx12UploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC dx12UploadBufferDescription = {};

		dx12UploadBufferDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		dx12UploadBufferDescription.Width = totalBytes;
		dx12UploadBufferDescription.Height = 1;
		dx12UploadBufferDescription.DepthOrArraySize = 1;
		dx12UploadBufferDescription.MipLevels = 1;
		dx12UploadBufferDescription.Format = DXGI_FORMAT_UNKNOWN;
		dx12UploadBufferDescription.SampleDesc.Count = 1;
		dx12UploadBufferDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

#if 0 // REPLACED
		if (FAILED(dx12Device->CreateCommittedResource(
			&dx12HeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&dx12UploadBufferDescription,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&dx12Resource))))
			return FFX_ERROR_OUT_OF_MEMORY;
#else
		CD3DX12_HEAP_PROPERTIES temp(dx12UploadHeapProperties);

		static_cast<FFInterfaceWrapper *>(backendInterface)->GetUserData()->m_NGXAllocCallback(
			&dx12UploadBufferDescription,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			&temp,
			&dx12Resource);

		if (!dx12Resource)
			return FFX_ERROR_OUT_OF_MEMORY;
#endif

		backendResource->initialState = FFX_RESOURCE_STATE_GENERIC_READ;
		backendResource->currentState = FFX_RESOURCE_STATE_GENERIC_READ;

		D3D12_RANGE dx12EmptyRange = {};
		void *uploadBufferData = nullptr;
		if (FAILED(dx12Resource->Map(0, &dx12EmptyRange, &uploadBufferData)))
			return FFX_ERROR_BACKEND_API_ERROR;

		const uint8_t *src = static_cast<uint8_t *>(createResourceDescription->initData);
		uint8_t *dst = static_cast<uint8_t *>(uploadBufferData);
		for (uint32_t currentRowIndex = 0; currentRowIndex < createResourceDescription->resourceDescription.height; ++currentRowIndex)
		{

			memcpy(dst, src, (size_t)rowSizeInBytes);
			src += rowSizeInBytes;
			dst += dx12Footprint.Footprint.RowPitch;
		}

		dx12Resource->Unmap(0, nullptr);
		dx12Resource->SetName(createResourceDescription->name);
		backendResource->resourcePtr = dx12Resource;

#ifdef _DEBUG
		wcscpy_s(backendResource->resourceName, createResourceDescription->name);
#endif
		return FFX_OK;
	}
	else
	{

		const FfxResourceStates resourceStates = (createResourceDescription->initData &&
												  (createResourceDescription->heapType != FFX_HEAP_TYPE_UPLOAD))
													 ? FFX_RESOURCE_STATE_COPY_DEST
													 : createResourceDescription->initalState;
		// Buffers ignore any input state and create in common (but issue a warning)
		const D3D12_RESOURCE_STATES dx12ResourceStates = dx12ResourceDescription.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER
															 ? D3D12_RESOURCE_STATE_COMMON
															 : ffxGetDX12StateFromResourceState(resourceStates);

#if 0 // REPLACED
		if (FAILED(dx12Device->CreateCommittedResource(
			&dx12HeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&dx12ResourceDescription,
			dx12ResourceStates,
			nullptr,
			IID_PPV_ARGS(&dx12Resource))))
			return FFX_ERROR_OUT_OF_MEMORY;
#else
		CD3DX12_HEAP_PROPERTIES temp(dx12HeapProperties);

		static_cast<FFInterfaceWrapper *>(backendInterface)->GetUserData()->m_NGXAllocCallback(
			&dx12ResourceDescription,
			dx12ResourceStates,
			&temp,
			&dx12Resource);

		if (!dx12Resource)
			return FFX_ERROR_OUT_OF_MEMORY;
#endif

		backendResource->initialState = resourceStates;
		backendResource->currentState = resourceStates;

		dx12Resource->SetName(createResourceDescription->name);
		backendResource->resourcePtr = dx12Resource;

#ifdef _DEBUG
		wcscpy_s(backendResource->resourceName, createResourceDescription->name);
#endif

		// Create SRVs and UAVs
		{
			D3D12_UNORDERED_ACCESS_VIEW_DESC dx12UavDescription = {};
			D3D12_SHADER_RESOURCE_VIEW_DESC dx12SrvDescription = {};
			D3D12_RESOURCE_DESC dx12Desc = dx12Resource->GetDesc();
			dx12UavDescription.Format = convertFormatUav(dx12Desc.Format);
			dx12SrvDescription.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			dx12SrvDescription.Format = convertFormatSrv(dx12Desc.Format);

			bool requestArrayView = FFX_CONTAINS_FLAG(createResourceDescription->resourceDescription.usage, FFX_RESOURCE_USAGE_ARRAYVIEW);

			switch (dx12Desc.Dimension)
			{

			case D3D12_RESOURCE_DIMENSION_BUFFER:
				dx12UavDescription.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
				dx12SrvDescription.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
				break;

			case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
				if (dx12Desc.DepthOrArraySize > 1 || requestArrayView)
				{
					dx12UavDescription.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
					dx12UavDescription.Texture1DArray.ArraySize = dx12Desc.DepthOrArraySize;
					dx12UavDescription.Texture1DArray.FirstArraySlice = 0;
					dx12UavDescription.Texture1DArray.MipSlice = 0;

					dx12SrvDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
					dx12SrvDescription.Texture1DArray.ArraySize = dx12Desc.DepthOrArraySize;
					dx12SrvDescription.Texture1DArray.FirstArraySlice = 0;
					dx12SrvDescription.Texture1DArray.MipLevels = dx12Desc.MipLevels;
					dx12SrvDescription.Texture1DArray.MostDetailedMip = 0;
				}
				else
				{
					dx12UavDescription.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
					dx12UavDescription.Texture1D.MipSlice = 0;

					dx12SrvDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
					dx12SrvDescription.Texture1D.MipLevels = dx12Desc.MipLevels;
					dx12SrvDescription.Texture1D.MostDetailedMip = 0;
				}
				break;

			case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
			{
				if (dx12Desc.DepthOrArraySize > 1 || requestArrayView)
				{
					dx12UavDescription.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
					dx12UavDescription.Texture2DArray.ArraySize = dx12Desc.DepthOrArraySize;
					dx12UavDescription.Texture2DArray.FirstArraySlice = 0;
					dx12UavDescription.Texture2DArray.MipSlice = 0;
					dx12UavDescription.Texture2DArray.PlaneSlice = 0;

					dx12SrvDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
					dx12SrvDescription.Texture2DArray.ArraySize = dx12Desc.DepthOrArraySize;
					dx12SrvDescription.Texture2DArray.FirstArraySlice = 0;
					dx12SrvDescription.Texture2DArray.MipLevels = dx12Desc.MipLevels;
					dx12SrvDescription.Texture2DArray.MostDetailedMip = 0;
					dx12SrvDescription.Texture2DArray.PlaneSlice = 0;
				}
				else
				{
					dx12UavDescription.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
					dx12UavDescription.Texture2D.MipSlice = 0;
					dx12UavDescription.Texture2D.PlaneSlice = 0;

					dx12SrvDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					dx12SrvDescription.Texture2D.MipLevels = dx12Desc.MipLevels;
					dx12SrvDescription.Texture2D.MostDetailedMip = 0;
					dx12SrvDescription.Texture2D.PlaneSlice = 0;
				}
				break;
			}

			case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
				dx12UavDescription.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
				dx12SrvDescription.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
				dx12SrvDescription.Texture3D.MipLevels = dx12Resource->GetDesc().MipLevels;
				dx12SrvDescription.Texture3D.MostDetailedMip = 0;
				break;

			default:
				break;
			}

			if (dx12Desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
			{

				// UAV
				if (dx12Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
				{

					FFX_ASSERT(effectContext.nextStaticUavDescriptor + 1 < effectContext.nextDynamicUavDescriptor);
					backendResource->uavDescCount = 1;
					backendResource->uavDescIndex = effectContext.nextStaticUavDescriptor++;

					dx12UavDescription.Buffer.FirstElement = 0;
					dx12UavDescription.Buffer.StructureByteStride = backendResource->resourceDescription.stride;
					dx12UavDescription.Buffer.NumElements = backendResource->resourceDescription.size /
															backendResource->resourceDescription.stride;
					dx12UavDescription.Buffer.CounterOffsetInBytes = 0;

					D3D12_CPU_DESCRIPTOR_HANDLE dx12CpuHandle = backendContext->descHeapUavGpu->GetCPUDescriptorHandleForHeapStart();
					dx12CpuHandle.ptr += (backendResource->uavDescIndex) *
										 dx12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					dx12Device->CreateUnorderedAccessView(dx12Resource, 0, &dx12UavDescription, dx12CpuHandle);

					dx12CpuHandle = backendContext->descHeapUavCpu->GetCPUDescriptorHandleForHeapStart();
					dx12CpuHandle.ptr += (backendResource->uavDescIndex) *
										 dx12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					dx12Device->CreateUnorderedAccessView(dx12Resource, 0, &dx12UavDescription, dx12CpuHandle);

					effectContext.nextStaticUavDescriptor++;
				}
				else
				{
					dx12SrvDescription.Buffer.FirstElement = 0;
					dx12SrvDescription.Buffer.StructureByteStride = backendResource->resourceDescription.stride;
					dx12SrvDescription.Buffer.NumElements = backendResource->resourceDescription.size /
															backendResource->resourceDescription.stride;
					D3D12_CPU_DESCRIPTOR_HANDLE dx12CpuHandle = backendContext->descHeapSrvCpu->GetCPUDescriptorHandleForHeapStart();
					dx12CpuHandle.ptr += outTexture->internalIndex *
										 dx12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
					dx12Device->CreateShaderResourceView(dx12Resource, &dx12SrvDescription, dx12CpuHandle);
				}
			}
			else
			{
				// CPU readable
				D3D12_CPU_DESCRIPTOR_HANDLE dx12CpuHandle = backendContext->descHeapSrvCpu->GetCPUDescriptorHandleForHeapStart();
				dx12CpuHandle.ptr += outTexture->internalIndex *
									 dx12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
				dx12Device->CreateShaderResourceView(dx12Resource, &dx12SrvDescription, dx12CpuHandle);

				// UAV
				if (dx12Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
				{

					const int32_t uavDescriptorCount = (dx12Desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) ? dx12Desc.MipLevels
																													 : 1;
					FFX_ASSERT(effectContext.nextStaticUavDescriptor + uavDescriptorCount < effectContext.nextDynamicUavDescriptor);

					backendResource->uavDescCount = uavDescriptorCount;
					backendResource->uavDescIndex = effectContext.nextStaticUavDescriptor;

					for (int32_t currentMipIndex = 0; currentMipIndex < uavDescriptorCount; ++currentMipIndex)
					{

						dx12UavDescription.Texture2D.MipSlice = currentMipIndex;

						dx12CpuHandle = backendContext->descHeapUavGpu->GetCPUDescriptorHandleForHeapStart();
						dx12CpuHandle.ptr += (backendResource->uavDescIndex + currentMipIndex) *
											 dx12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
						dx12Device->CreateUnorderedAccessView(dx12Resource, 0, &dx12UavDescription, dx12CpuHandle);

						dx12CpuHandle = backendContext->descHeapUavCpu->GetCPUDescriptorHandleForHeapStart();
						dx12CpuHandle.ptr += (backendResource->uavDescIndex + currentMipIndex) *
											 dx12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
						dx12Device->CreateUnorderedAccessView(dx12Resource, 0, &dx12UavDescription, dx12CpuHandle);
					}

					effectContext.nextStaticUavDescriptor += uavDescriptorCount;
				}
			}
		}

		// create upload resource and upload job
		if (createResourceDescription->initData)
		{

			FfxResourceInternal copySrc;
			FfxCreateResourceDescription uploadDescription = { *createResourceDescription };
			uploadDescription.heapType = FFX_HEAP_TYPE_UPLOAD;
			uploadDescription.resourceDescription.usage = FFX_RESOURCE_USAGE_READ_ONLY;
			uploadDescription.initalState = FFX_RESOURCE_STATE_GENERIC_READ;

			backendInterface->fpCreateResource(backendInterface, &uploadDescription, effectContextId, &copySrc);

			// setup the upload job
			FfxGpuJobDescription copyJob = {

				FFX_GPU_JOB_COPY
			};
			copyJob.copyJobDescriptor.src = copySrc;
			copyJob.copyJobDescriptor.dst = *outTexture;

			backendInterface->fpScheduleGpuJob(backendInterface, &copyJob);
		}
	}

	return FFX_OK;
}

FfxErrorCode FFInterfaceWrapper::CustomDestroyResourceDX12(
	FfxInterface *backendInterface,
	FfxResourceInternal resource,
	FfxUInt32 effectContextId)
{
	FFX_ASSERT(NULL != backendInterface);

	BackendContext_DX12 *backendContext = (BackendContext_DX12 *)backendInterface->scratchBuffer;
	BackendContext_DX12::EffectContext& effectContext = backendContext->pEffectContexts[effectContextId];
	if ((resource.internalIndex >= int32_t(effectContextId * FFX_MAX_RESOURCE_COUNT)) &&
		(resource.internalIndex < int32_t(effectContext.nextStaticResource)))
	{
		ID3D12Resource *dx12Resource = getDX12ResourcePtr(backendContext, resource.internalIndex);

		if (dx12Resource)
		{
#if 0 // REPLACED
			dx12Resource->Release();
#else
			static_cast<FFInterfaceWrapper *>(backendInterface)->GetUserData()->m_NGXFreeCallback(dx12Resource);
#endif

			backendContext->pResources[resource.internalIndex].resourcePtr = nullptr;
		}

		return FFX_OK;
	}

	return FFX_ERROR_OUT_OF_RANGE;
}

DXGI_FORMAT convertFormatUav(DXGI_FORMAT format)
{
	switch (format)
	{
	// Handle Depth
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
	case DXGI_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	case DXGI_FORMAT_D16_UNORM:
		return DXGI_FORMAT_R16_UNORM;

	// Handle color: assume FLOAT for 16 and 32 bit channels, else UNORM
	case DXGI_FORMAT_R32G32B32A32_TYPELESS:
		return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case DXGI_FORMAT_R32G32B32_TYPELESS:
		return DXGI_FORMAT_R32G32B32_FLOAT;
	case DXGI_FORMAT_R16G16B16A16_TYPELESS:
		return DXGI_FORMAT_R16G16B16A16_FLOAT;
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	case DXGI_FORMAT_R32G32_TYPELESS:
		return DXGI_FORMAT_R32G32_FLOAT;
	case DXGI_FORMAT_R16G16_TYPELESS:
		return DXGI_FORMAT_R16G16_FLOAT;
	case DXGI_FORMAT_R10G10B10A2_TYPELESS:
		return DXGI_FORMAT_R10G10B10A2_UNORM;
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	case DXGI_FORMAT_B8G8R8X8_TYPELESS:
		return DXGI_FORMAT_B8G8R8X8_UNORM_SRGB;
	case DXGI_FORMAT_R32_TYPELESS:
		return DXGI_FORMAT_R32_FLOAT;
	case DXGI_FORMAT_R8G8_TYPELESS:
		return DXGI_FORMAT_R8G8_UNORM;
	case DXGI_FORMAT_R16_TYPELESS:
		return DXGI_FORMAT_R16_FLOAT;
	case DXGI_FORMAT_R8_TYPELESS:
		return DXGI_FORMAT_R8_UNORM;
	default:
		return format;
	}
}

DXGI_FORMAT convertFormatSrv(DXGI_FORMAT format)
{
	switch (format)
	{
		// Handle Depth
	case DXGI_FORMAT_R32G8X24_TYPELESS:
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
	case DXGI_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_R32_FLOAT;
	case DXGI_FORMAT_R24G8_TYPELESS:
	case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	case DXGI_FORMAT_D16_UNORM:
		return DXGI_FORMAT_R16_UNORM;

		// Handle Color
	case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		return DXGI_FORMAT_B8G8R8A8_UNORM;
	case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		return DXGI_FORMAT_R8G8B8A8_UNORM;

		// Others can map as is
	default:

		return format;
	}
}
