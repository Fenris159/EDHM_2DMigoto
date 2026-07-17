#pragma once

#include <d3d11.h>

#include <atomic>
#include <string>
#include <vector>

// Private data on the original layout holding an IUnknown ref to our side-car.
// {A7C3E1B0-4D2F-4E8A-9C11-E01111A70101}
static const GUID GUID_HackerInputLayout =
{ 0xa7c3e1b0, 0x4d2f, 0x4e8a, { 0x9c, 0x11, 0xe0, 0x11, 0x11, 0xa7, 0x01, 0x01 } };

class HackerInputLayout final : public ID3D11InputLayout
{
public:
	HackerInputLayout(ID3D11InputLayout* orig, const D3D11_INPUT_ELEMENT_DESC* pElements, UINT numElements);
	~HackerInputLayout();

	// Resolve a game-supplied layout pointer to our side-car (or nullptr if foreign).
	// Returns an AddRef'd pointer; caller must Release().
	static HackerInputLayout* FromLayout(ID3D11InputLayout* layout);

	ID3D11InputLayout* GetOrigInputLayout() const;
	HRESULT GetAttachResult() const;

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
	HRESULT mAttachResult = E_FAIL;
	uint32_t mLayoutHash = 0;

	std::vector<D3D11_INPUT_ELEMENT_DESC> mElements;
	std::vector<std::string> mSemanticNames;
};
