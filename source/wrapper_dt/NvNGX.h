#pragma once

#include <cstdint>

// dlssg_to_fsr3_amd_is_better.dll is the meat & bones. I'm using a bootstrapper/forwarder DLL in
// order to give us a chance to log errors at startup. Otherwise games will silently fail if they
// can't load the implementation.
#define NGXDLLEXPORT					 extern "C" __declspec(dllexport)
#define CALL_NGX_EXPORT_IMPL(ExportName) return reinterpret_cast<decltype(&ExportName)>(GetOriginalExportCached(#ExportName))

using NGXResult = uint32_t;
struct NGXHandle;				  // See _nvngx.dll
struct NGXFeatureRequirementInfo; // See nvngx_dlssg.dll
class NGXInstanceParameters;	  // See sl.common.dll exports

auto GetImplementationDll();
auto GetOriginalExport(const char *name);

template<typename = decltype([]{})>
auto GetOriginalExportCached(const char *name)
{
	static auto impl = GetOriginalExport(name);
	return impl;
}
