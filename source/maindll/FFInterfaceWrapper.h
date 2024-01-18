#pragma once

struct CD3DX12_HEAP_PROPERTIES;
struct D3D12_RESOURCE_DESC;
struct ID3D12Device;
struct ID3D12Resource;
struct NGXInstanceParameters;

#include <FidelityFX/host/ffx_interface.h>

class FFInterfaceWrapper : public FfxInterface
{
private:
	using NGXAllocCallback = void(D3D12_RESOURCE_DESC *Desc, uint32_t State, CD3DX12_HEAP_PROPERTIES *Heap, ID3D12Resource **Resource);
	using NGXFreeCallback = void(ID3D12Resource *Resource);

	struct UserDataHack
	{
		NGXAllocCallback *m_NGXAllocCallback = nullptr;
		NGXFreeCallback *m_NGXFreeCallback = nullptr;
	};
	static_assert(sizeof(UserDataHack) == 0x10);

public:
	FFInterfaceWrapper();
	FFInterfaceWrapper(const FFInterfaceWrapper&) = delete;
	FFInterfaceWrapper& operator=(const FFInterfaceWrapper&) = delete;
	~FFInterfaceWrapper();

	FfxErrorCode Initialize(ID3D12Device *Device, uint32_t MaxContexts, NGXInstanceParameters *NGXParameters);

private:
	UserDataHack *GetUserData();

	static FfxErrorCode CustomCreateResourceDX12(
		FfxInterface *backendInterface,
		const FfxCreateResourceDescription *createResourceDescription,
		FfxUInt32 effectContextId,
		FfxResourceInternal *outTexture);

	static FfxErrorCode CustomDestroyResourceDX12(FfxInterface *backendInterface, FfxResourceInternal resource, FfxUInt32 effectContextId);
};
static_assert(sizeof(FFInterfaceWrapper) == sizeof(FfxInterface));
