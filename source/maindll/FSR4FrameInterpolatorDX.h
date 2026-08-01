#pragma once

struct ID3D12Device;
struct ID3D12GraphicsCommandList;
struct ID3D12Resource;
struct NGXInstanceParameters;

class FSR4FrameInterpolatorDX
{
public:
	FSR4FrameInterpolatorDX(ID3D12Device *device, uint32_t outputWidth, uint32_t outputHeight);
	FSR4FrameInterpolatorDX(const FSR4FrameInterpolatorDX&) = delete;
	FSR4FrameInterpolatorDX& operator=(const FSR4FrameInterpolatorDX&) = delete;
	~FSR4FrameInterpolatorDX();

	bool IsAvailable() const;
	bool Dispatch(ID3D12GraphicsCommandList *commandList, NGXInstanceParameters *parameters);

private:
	bool CreateContext(ID3D12Resource *color, ID3D12Resource *depth, ID3D12Resource *motionVectors, NGXInstanceParameters *parameters);
	void DestroyContext();

	ID3D12Device *m_Device = nullptr;
	void *m_Runtime = nullptr;
	void *m_Context = nullptr;
	void *m_CreateContext = nullptr;
	void *m_DestroyContext = nullptr;
	void *m_Configure = nullptr;
	void *m_Query = nullptr;
	void *m_Dispatch = nullptr;
	uint32_t m_OutputWidth = 0;
	uint32_t m_OutputHeight = 0;
	uint64_t m_FrameId = 0;
	bool m_FSR4ProviderUnavailable = false;
	bool m_FSR4DispatchUnavailable = false;
	bool m_LoggedFSR4Active = false;
};
