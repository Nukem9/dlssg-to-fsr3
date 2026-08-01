#pragma once

struct ID3D12Device;

// Bridges the FFX loader's AMD extension callback to the driver FSR4 provider.
class FSR4DriverUpgrade
{
public:
	static void Initialize(ID3D12Device *device);
	static bool IsFGProviderUpgraded();
};
