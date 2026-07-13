#pragma once

#include <d3d11.h>

#include <atomic>
#include <string>
#include <vector>

class HackerInputLayout final : public ID3D11InputLayout
{
public:
	HackerInputLayout(ID3D11InputLayout* orig, const D3D11_INPUT_ELEMENT_DESC* pElements, UINT numElements);
	~HackerInputLayout();

	ID3D11InputLayout* GetOrigInputLayout() const;

	UINT GetElementCount() const;
	const D3D11_INPUT_ELEMENT_DESC* GetElements() const;
	uint32_t GetLayoutHash() const;

#pragma region IUnknown
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override;
	ULONG STDMETHODCALLTYPE AddRef() override;
	ULONG STDMETHODCALLTYPE Release() override;
#pragma endregion

#pragma region ID3D11DeviceChild
	void STDMETHODCALLTYPE GetDevice(ID3D11Device** ppDevice) override;
	HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData) override;
	HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void* pData) override;
	HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown* pData) override;
#pragma endregion

private:
	uint32_t CalculateLayoutHash() const;

private:
	std::atomic<ULONG> mRefCount = 1;
	ID3D11InputLayout* mOrigLayout = nullptr;
	uint32_t mLayoutHash = 0;

	std::vector<D3D11_INPUT_ELEMENT_DESC> mElements;
	std::vector<std::string> mSemanticNames;
};
