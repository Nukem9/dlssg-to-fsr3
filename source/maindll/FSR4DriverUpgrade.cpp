#define WIN32_NO_STATUS
#include <Windows.h>
#undef WIN32_NO_STATUS
#include <d3d12.h>
#include <detours/detours.h>
#include <dxgi1_6.h>

#include <atomic>
#include <filesystem>
#include <mutex>

#include <ffx_api/ffx_framegeneration.h>

#include "FSR4DriverUpgrade.h"

namespace
{
	struct MagicData
	{
		uint32_t values[4];
		MagicData *next;
	};

	using AmdExtD3DCreateInterfaceFn = HRESULT(__cdecl *)(IUnknown *outer, REFIID iid, void **object);
	using UpdateFfxApiProviderFn = HRESULT(STDMETHODCALLTYPE *)(void *data, uint32_t size);
	using UpdateFfxApiProviderExFn = HRESULT(STDMETHODCALLTYPE *)(void *data, uint32_t size, MagicData *magic);

	MIDL_INTERFACE("b58d6601-7401-4234-8180-6febfc0e484c")
	IAmdExtFfxApi : public IUnknown
	{
		virtual HRESULT STDMETHODCALLTYPE UpdateFfxApiProvider(void *data, uint32_t size) = 0;
	};

	class AmdExtFfxApiProxy final : public IAmdExtFfxApi
	{
	public:
		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **object) override;
		ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
		ULONG STDMETHODCALLTYPE Release() override { return 1; }
		HRESULT STDMETHODCALLTYPE UpdateFfxApiProvider(void *data, uint32_t size) override;
		void SetOriginal(IAmdExtFfxApi *original);

	private:
		IAmdExtFfxApi *GetOriginal();
		std::mutex m_OriginalLock;
		IAmdExtFfxApi *m_Original = nullptr;
	};

	std::once_flag g_InitializeOnce;
	std::mutex g_DriverApiLock;
	AmdExtD3DCreateInterfaceFn g_OriginalCreateInterface = nullptr;
	UpdateFfxApiProviderFn g_UpdateProvider = nullptr;
	UpdateFfxApiProviderExFn g_UpdateProviderEx = nullptr;
	HMODULE g_DriverProviderModule = nullptr;
	std::atomic_bool g_Enabled = false;
	std::atomic_bool g_Hooked = false;
	std::atomic_bool g_FGProviderUpgraded = false;
	AmdExtFfxApiProxy g_AmdExtFfxApiProxy;

	bool IsAmdAdapter(ID3D12Device *device)
	{
		if (!device)
			return false;

		const auto luid = device->GetAdapterLuid();
		IDXGIFactory1 *factory = nullptr;
		if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
			return false;

		bool isAmd = false;
		for (uint32_t index = 0; !isAmd; ++index)
		{
			IDXGIAdapter1 *adapter = nullptr;
			if (factory->EnumAdapters1(index, &adapter) != S_OK)
				break;

			DXGI_ADAPTER_DESC1 desc = {};
			if (SUCCEEDED(adapter->GetDesc1(&desc)) &&
				desc.AdapterLuid.LowPart == luid.LowPart && desc.AdapterLuid.HighPart == luid.HighPart)
			{
				isAmd = desc.VendorId == 0x1002;
				spdlog::info("D3D12 adapter for FSR4 FG driver upgrade: vendor {:04X}, device {:04X}.",
					desc.VendorId, desc.DeviceId);
			}
			adapter->Release();
		}

		factory->Release();
		return isAmd;
	}

	HMODULE LoadActiveDriverProvider()
	{
		if (const auto loaded = GetModuleHandleW(L"amdxcffx64.dll"))
			return loaded;

		wchar_t systemDirectory[MAX_PATH] = {};
		if (!GetSystemDirectoryW(systemDirectory, MAX_PATH))
			return nullptr;

		const auto driverStore = std::filesystem::path(systemDirectory).parent_path() /
			L"System32\\DriverStore\\FileRepository";
		std::filesystem::path newestProvider;
		std::filesystem::file_time_type newestWriteTime = {};
		std::error_code error;
		for (std::filesystem::recursive_directory_iterator entry(
			driverStore, std::filesystem::directory_options::skip_permission_denied, error);
			entry != std::filesystem::recursive_directory_iterator(); entry.increment(error))
		{
			if (error)
			{
				error.clear();
				continue;
			}
			if (!entry->is_regular_file(error) || entry->path().filename() != L"amdxcffx64.dll")
				continue;
			const auto writeTime = entry->last_write_time(error);
			if (!error && (newestProvider.empty() || writeTime > newestWriteTime))
			{
				newestProvider = entry->path();
				newestWriteTime = writeTime;
			}
		}

		if (!newestProvider.empty())
		{
			const auto module = LoadLibraryExW(newestProvider.c_str(), nullptr,
				LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
			if (module)
				spdlog::info("Loaded newest installed FSR4 FG driver provider from {}.", newestProvider.string());
			return module;
		}

		return nullptr;
	}

	bool LoadDriverUpgradeApi()
	{
		std::scoped_lock lock(g_DriverApiLock);
		if (g_UpdateProvider || g_UpdateProviderEx)
			return true;

		g_DriverProviderModule = LoadActiveDriverProvider();
		if (!g_DriverProviderModule)
		{
			spdlog::warn("amdxcffx64.dll was not found for the active adapter; using FSR3 FG.");
			return false;
		}

		g_UpdateProvider = reinterpret_cast<UpdateFfxApiProviderFn>(GetProcAddress(g_DriverProviderModule, "UpdateFfxApiProvider"));
		g_UpdateProviderEx = reinterpret_cast<UpdateFfxApiProviderExFn>(GetProcAddress(g_DriverProviderModule, "UpdateFfxApiProviderEx"));
		if (!g_UpdateProvider && !g_UpdateProviderEx)
		{
			spdlog::warn("amdxcffx64.dll has no FFX provider upgrade export; using FSR3 FG.");
			return false;
		}
		return true;
	}

	HRESULT STDMETHODCALLTYPE HookedAmdExtD3DCreateInterface(IUnknown *outer, REFIID iid, void **object)
	{
		if (g_Enabled && iid == __uuidof(IAmdExtFfxApi))
		{
			// FSR4 upscaling uses this same interface. Keep the driver's implementation
			// behind our proxy so non-FG provider upgrades retain their native path.
			HRESULT originalResult = E_NOINTERFACE;
			if (g_OriginalCreateInterface)
			{
				void *original = nullptr;
				originalResult = g_OriginalCreateInterface(outer, iid, &original);
				if (SUCCEEDED(originalResult) && original)
				{
					g_AmdExtFfxApiProxy.SetOriginal(static_cast<IAmdExtFfxApi *>(original));
					static_cast<IAmdExtFfxApi *>(original)->Release();
				}
			}
			if (FAILED(originalResult))
				spdlog::warn("Failed to obtain the original AMD FFX provider interface ({:08X}); non-FG FSR4 upgrades may be unavailable.",
					static_cast<uint32_t>(originalResult));
			*object = &g_AmdExtFfxApiProxy;
			spdlog::info("AMD FFX loader requested the FSR4 FG provider upgrade interface.");
			return S_OK;
		}
		return g_OriginalCreateInterface ? g_OriginalCreateInterface(outer, iid, object) : E_NOINTERFACE;
	}

	HRESULT AmdExtFfxApiProxy::QueryInterface(REFIID iid, void **object)
	{
		if (!object)
			return E_POINTER;
		if (iid == IID_IUnknown || iid == __uuidof(IAmdExtFfxApi))
		{
			*object = this;
			return S_OK;
		}
		*object = nullptr;
		return E_NOINTERFACE;
	}

	void AmdExtFfxApiProxy::SetOriginal(IAmdExtFfxApi *original)
	{
		if (!original)
			return;

		std::scoped_lock lock(m_OriginalLock);
		if (m_Original)
			return;

		original->AddRef();
		m_Original = original;
	}

	IAmdExtFfxApi *AmdExtFfxApiProxy::GetOriginal()
	{
		std::scoped_lock lock(m_OriginalLock);
		if (m_Original)
			m_Original->AddRef();
		return m_Original;
	}

	HRESULT AmdExtFfxApiProxy::UpdateFfxApiProvider(void *data, uint32_t size)
	{
		if (!data || size < sizeof(uint32_t) + sizeof(uint32_t) + sizeof(uint64_t))
			return E_INVALIDARG;

		const auto descType = *reinterpret_cast<const uint64_t *>(static_cast<const uint8_t *>(data) + 8);
		if (descType != FFX_API_CREATE_CONTEXT_DESC_TYPE_FRAMEGENERATION)
		{
			auto *original = GetOriginal();
			if (!original)
				return E_NOINTERFACE;
			const auto result = original->UpdateFfxApiProvider(data, size);
			original->Release();
			return result;
		}
		if (!LoadDriverUpgradeApi())
			return E_NOINTERFACE;

		HRESULT result = E_NOINTERFACE;
		if (g_UpdateProviderEx)
		{
			MagicData magic = { { 0, 1, 1, 0 }, nullptr };
			result = g_UpdateProviderEx(data, size, &magic);
		}
		else if (g_UpdateProvider)
			result = g_UpdateProvider(data, size);

		if (SUCCEEDED(result))
		{
			g_FGProviderUpgraded = true;
			spdlog::info("FSR4 FG driver provider upgrade accepted (0x{:08X}).", static_cast<uint32_t>(result));
		}
		else
			spdlog::warn("FSR4 FG driver provider upgrade declined (0x{:08X}); using FSR3 FG.", static_cast<uint32_t>(result));
		return result;
	}
}

void FSR4DriverUpgrade::Initialize(ID3D12Device *device)
{
	std::call_once(g_InitializeOnce, [device]()
	{
		if (!IsAmdAdapter(device))
			spdlog::info("D3D12 adapter is exposed as non-AMD; continuing because DLSS proxy layers can spoof the adapter identity.");

		const auto amdxc64 = GetModuleHandleW(L"amdxc64.dll");
		if (!amdxc64)
		{
			spdlog::warn("amdxc64.dll is not loaded by this D3D12 runtime; using FSR3 FG.");
			return;
		}

		g_OriginalCreateInterface = reinterpret_cast<AmdExtD3DCreateInterfaceFn>(
			GetProcAddress(amdxc64, "AmdExtD3DCreateInterface"));
		if (!g_OriginalCreateInterface)
		{
			spdlog::warn("amdxc64.dll has no AmdExtD3DCreateInterface export; using FSR3 FG.");
			return;
		}

		g_Enabled = true;
		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(reinterpret_cast<PVOID *>(&g_OriginalCreateInterface), HookedAmdExtD3DCreateInterface);
		const auto result = DetourTransactionCommit();
		if (result != NO_ERROR)
		{
			g_Enabled = false;
			spdlog::warn("Failed to hook AmdExtD3DCreateInterface ({:X}); using FSR3 FG.", result);
			return;
		}

		g_Hooked = true;
		spdlog::info("FSR4 FG AMD driver upgrade hook installed.");
	});
}

bool FSR4DriverUpgrade::IsFGProviderUpgraded()
{
	return g_Hooked && g_FGProviderUpgraded;
}
