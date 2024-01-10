#include <cstdint>
#include <string.h>

constinit const char *TargetFunctionNames[] = {
	"EOS_Overlay_ApplicationWillShutdown",
	"EOS_Overlay_CloseBrowser",
	"EOS_Overlay_EjectInstance",
	"EOS_Overlay_EvaluateJS",
	"EOS_Overlay_GetDisplaySettings",
	"EOS_Overlay_Initialize",
	"EOS_Overlay_InvokeJavascriptCallback",
	"EOS_Overlay_LoadURL",
	"EOS_Overlay_ObserveBrowserStatus",
	"EOS_Overlay_RegisterGamepadListener",
	"EOS_Overlay_RegisterJSBindings",
	"EOS_Overlay_RegisterKeyListener",
	"EOS_Overlay_SetAnalyticsEventHandler",
	"EOS_Overlay_SetDisplaySettings",
	"EOS_Overlay_SetLogMessageHandler",
	"EOS_Overlay_UnregisterGamepadListener",
	"EOS_Overlay_UnregisterKeyListener",
};

uint32_t __fastcall HookedEOS_Overlay_Stub(void *a1, void *a2, void *a3)
{
	return 0xFFFFFFFF;
}

void TryInterceptEOSFunction(void *ModuleHandle, const void *FunctionName, void **FunctionPointer)
{
	if (!FunctionName || !*FunctionPointer || reinterpret_cast<uintptr_t>(FunctionName) < 0x10000)
		return;

	// Each export from EOSOVH-Win64-Shipping.dll requires interception. They're all guilty of calling the
	// overlay initialization function, which causes other hooks to be removed.
	for (auto name : TargetFunctionNames)
	{
		if (strcmp(static_cast<const char *>(FunctionName), name) != 0)
			continue;

		*FunctionPointer = &HookedEOS_Overlay_Stub;
		break;
	}
}
