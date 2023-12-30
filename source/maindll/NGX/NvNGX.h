#pragma once

#define NGXDLLEXPORT extern "C" __declspec(dllexport)

using NGXResult = uint32_t;
constexpr NGXResult NGX_SUCCESS = 0x1;
constexpr NGXResult NGX_FEATURE_NOT_FOUND = 0xBAD00004;
constexpr NGXResult NGX_INVALID_PARAMETER = 0xBAD00005;

constexpr uint32_t NGXHardcodedArchitecture = 0xC0;

// See _nvngx.dll
struct NGXHandle
{
	uint32_t InternalId = 0;
	uint32_t InternalFeatureId = 0;

	static NGXHandle *Allocate(uint32_t FeatureId)
	{
		constinit static uint32_t nextId = 1;
		return new NGXHandle { nextId++, FeatureId };
	}

	static void Free(NGXHandle *Handle)
	{
		delete Handle;
	}
};

// See nvngx_dlss.dll and nvngx_dlssg.dll
#ifdef VK_HEADER_VERSION
struct NGXVulkanResourceHandle
{
	struct
	{
		VkImageView View;
		VkImage Image;
		VkImageSubresourceRange Subresource;
		VkFormat Format;
		uint32_t Width;
		uint32_t Height;
	} ImageMetadata;
	uint32_t Type;
};
#endif // VK_HEADER_VERSION

// See nvngx_dlssg.dll
struct NGXFeatureRequirementInfo
{
	uint32_t Flags;
	uint32_t RequiredGPUArchitecture;
	char RequiredOperatingSystemVersion[32];
};

// See sl.common.dll exports
struct NGXInstanceParameters
{
	virtual void SetVoidPointer(const char *Name, void *Value) = 0;		  // 0
	virtual void Set2(const char *Name, float Value) = 0;				  // 8
	virtual void Set3(const char *Name, void *Unknown) = 0;				  // 10
	virtual void Set4(const char *Name, uint32_t Value) = 0;			  // 18
	virtual void Set5(const char *Name, uint32_t Value) = 0;			  // 20
	virtual void Set6(const char *Name, void *Unknown) = 0;				  // 28
	virtual void Set7(const char *Name, struct ID3D12Resource *Value) = 0;// 30
	virtual void Set8(const char *Name, void *Value) = 0;				  // 38
	virtual NGXResult GetVoidPointer(const char *Name, void **Value) = 0; // 40
	virtual NGXResult Get2(const char *Name, float *Value) = 0;			  // 48
	virtual NGXResult Get3(const char *Name, void *Value) = 0;			  // 50
	virtual NGXResult Get4(const char *Name, uint32_t *Value) = 0;		  // 58
	virtual NGXResult Get5(const char *Name, uint32_t *Value) = 0;		  // 60
	virtual NGXResult Get6(const char *Name, void *Unknown) = 0;		  // 68
	virtual NGXResult Get7(const char *Name, float *Value) = 0;			  // 70
	virtual NGXResult Get8(const char *Name, void *Unknown) = 0;		  // 78
	virtual void Unknown() = 0;

	float GetFloatOrDefault(const char *Name, float Default)
	{
		Get7(Name, &Default);
		return Default;
	}

	uint32_t GetUIntOrDefault(const char *Name, uint32_t Default)
	{
		Get5(Name, &Default);
		return Default;
	}
};

