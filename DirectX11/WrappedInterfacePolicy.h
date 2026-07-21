#pragma once

#include <d3d11_4.h>

static inline bool IsUnsupportedWrappedDeviceInterface(REFIID riid)
{
	return riid == __uuidof(ID3D11Device2) || riid == __uuidof(ID3D11Device3) ||
		riid == __uuidof(ID3D11Device4) || riid == __uuidof(ID3D11Device5);
}

static inline bool IsUnsupportedWrappedContextInterface(REFIID riid)
{
	return riid == __uuidof(ID3D11DeviceContext2) || riid == __uuidof(ID3D11DeviceContext3) ||
		riid == __uuidof(ID3D11DeviceContext4);
}
