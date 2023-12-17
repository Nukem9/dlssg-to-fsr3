#include "nvngx.h"

NGXDLLEXPORT uint32_t NVSDK_NGX_GetAPIVersion()
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_GetAPIVersion)();
}

NGXDLLEXPORT uint32_t NVSDK_NGX_GetApplicationId()
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_GetApplicationId)();
}

NGXDLLEXPORT uint32_t NVSDK_NGX_GetDriverVersion()
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_GetDriverVersion)();
}

NGXDLLEXPORT NGXResult NVSDK_NGX_GetDriverVersionEx(uint32_t *Versions, uint32_t InputVersionCount, uint32_t *TotalDriverVersionCount)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_GetDriverVersionEx)(Versions, InputVersionCount, TotalDriverVersionCount);
}

NGXDLLEXPORT uint32_t NVSDK_NGX_GetGPUArchitecture()
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_GetGPUArchitecture)();
}

NGXDLLEXPORT uint32_t NVSDK_NGX_GetSnippetVersion()
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_GetSnippetVersion)();
}

NGXDLLEXPORT NGXResult NVSDK_NGX_ProcessCommand(const char *Command, const char *Value, void *Unknown)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_ProcessCommand)(Command, Value, Unknown);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_SetInfoCallback(void *Callback)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_SetInfoCallback)(Callback);
}

NGXDLLEXPORT NGXResult NVSDK_NGX_SetTelemetryEvaluateCallback(void *Callback)
{
	CALL_NGX_EXPORT_IMPL(NVSDK_NGX_SetTelemetryEvaluateCallback)(Callback);
}
