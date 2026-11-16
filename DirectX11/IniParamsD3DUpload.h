#pragma once

#include <d3d11.h>

#include <cstddef>
#include <cstring>

// Concrete D3D adapter for one complete WRITE_DISCARD publication. It consumes
// the CPU image synchronously and never unmaps a resource whose Map failed.
inline HRESULT UploadIniParams(ID3D11DeviceContext *context, ID3D11Texture1D *texture, const void *rows,
                               std::size_t row_count)
{
	if (!context || !texture || !rows)
		return E_POINTER;
	D3D11_TEXTURE1D_DESC desc{};
	texture->GetDesc(&desc);
	if (desc.Width != row_count || desc.Format != DXGI_FORMAT_R32G32B32A32_FLOAT)
		return E_INVALIDARG;

	D3D11_MAPPED_SUBRESOURCE mapped{};
	HRESULT result = context->Map(texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
	if (FAILED(result))
		return result;
	std::memcpy(mapped.pData, rows, row_count * sizeof(float) * 4);
	context->Unmap(texture, 0);
	return S_OK;
}
