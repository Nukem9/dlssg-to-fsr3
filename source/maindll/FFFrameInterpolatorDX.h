#pragma once

#include "FFFrameInterpolator.h"

class FSR4FrameInterpolatorDX;

struct ID3D12Device;

class FFFrameInterpolatorDX final : public FFFrameInterpolator
{
private:
	ID3D12Device *const m_Device;

	// Transient
	FfxCommandList m_ActiveCommandList = {};
	std::unique_ptr<FSR4FrameInterpolatorDX> m_FSR4;

public:
	FFFrameInterpolatorDX(ID3D12Device *Device, uint32_t OutputWidth, uint32_t OutputHeight, NGXInstanceParameters *NGXParameters);
	FFFrameInterpolatorDX(const FFFrameInterpolatorDX&) = delete;
	FFFrameInterpolatorDX& operator=(const FFFrameInterpolatorDX&) = delete;
	~FFFrameInterpolatorDX();

	FfxErrorCode Dispatch(void *CommandList, NGXInstanceParameters *NGXParameters) override;

private:
	FfxErrorCode InitializeBackendInterface(
		FFInterfaceWrapper *BackendInterface,
		uint32_t MaxContexts,
		NGXInstanceParameters *NGXParameters) override;

	std::array<uint8_t, 8> GetActiveAdapterLUID() const override;
	FfxCommandList GetActiveCommandList() const override;

	void CopyTexture(FfxCommandList CommandList, const FfxResource *Destination, const FfxResource *Source) override;

	bool LoadTextureFromNGXParameters(
		NGXInstanceParameters *NGXParameters,
		const char *Name,
		FfxResource *OutFfxResource,
		FfxResourceStates State) override;
};
