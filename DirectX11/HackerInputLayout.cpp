#include "util.h"

#include "HackerInputLayout.h"

HackerInputLayout::HackerInputLayout(ID3D11InputLayout* orig, const D3D11_INPUT_ELEMENT_DESC* pElements, UINT numElements) : mOrigLayout(orig)
{
	mElements.resize(numElements);
	mSemanticNames.resize(numElements);

	for (UINT i = 0; i < numElements; ++i)
	{
		mElements[i] = pElements[i];

		if (pElements[i].SemanticName)
		{
			mSemanticNames[i] = pElements[i].SemanticName;
			mElements[i].SemanticName = mSemanticNames[i].c_str();
		}
		else
		{
			mElements[i].SemanticName = nullptr;
		}
	}

	mLayoutHash = CalculateLayoutHash();

	// Side-car only: game keeps the real ID3D11InputLayout. Mark the original
	// so FromLayout() can recover this cache for hunting / frame analysis.
	// The original layout owns this side-car through SetPrivateDataInterface.
	if (mOrigLayout)
		mAttachResult = mOrigLayout->SetPrivateDataInterface(GUID_HackerInputLayout, this);
}

HackerInputLayout* HackerInputLayout::FromLayout(ID3D11InputLayout* layout)
{
	if (!layout)
		return nullptr;

	IUnknown* private_data = nullptr;
	UINT size = sizeof(private_data);
	// Works whether `layout` is our wrapper (GetPrivateData forwards to orig)
	// or the original layout we created (private data set in constructor).
	if (SUCCEEDED(layout->GetPrivateData(GUID_HackerInputLayout, &size, &private_data)) && private_data) {
		HackerInputLayout* hacker = nullptr;
		HRESULT hr = private_data->QueryInterface(GUID_HackerInputLayout, reinterpret_cast<void**>(&hacker));
		private_data->Release();
		if (SUCCEEDED(hr))
			return hacker;
	}

	return nullptr;
}

HackerInputLayout::~HackerInputLayout() = default;

ID3D11InputLayout* HackerInputLayout::GetOrigInputLayout() const
{
	return mOrigLayout;
}

HRESULT HackerInputLayout::GetAttachResult() const
{
	return mAttachResult;
}

UINT HackerInputLayout::GetElementCount() const
{
	return static_cast<UINT>(mElements.size());
}

const D3D11_INPUT_ELEMENT_DESC* HackerInputLayout::GetElements() const
{
	return mElements.data();
}

uint32_t HackerInputLayout::GetLayoutHash() const
{
	return mLayoutHash;
}

uint32_t HackerInputLayout::CalculateLayoutHash() const
{
	uint32_t hash = 0;

	for (size_t i = 0; i < mElements.size(); ++i)
	{
		const D3D11_INPUT_ELEMENT_DESC& element = mElements[i];

		// Do not hash element.SemanticName pointer.
		if (element.SemanticName)
			hash = crc32c_hw(hash, element.SemanticName, strlen(element.SemanticName));

		hash = crc32c_hw(hash, &element.SemanticIndex, sizeof(element.SemanticIndex));
		hash = crc32c_hw(hash, &element.Format, sizeof(element.Format));
		hash = crc32c_hw(hash, &element.InputSlot, sizeof(element.InputSlot));
		hash = crc32c_hw(hash, &element.AlignedByteOffset, sizeof(element.AlignedByteOffset));
		hash = crc32c_hw(hash, &element.InputSlotClass, sizeof(element.InputSlotClass));
		hash = crc32c_hw(hash, &element.InstanceDataStepRate, sizeof(element.InstanceDataStepRate));
	}

	return hash;
}

#pragma region IUnknown
HRESULT STDMETHODCALLTYPE HackerInputLayout::QueryInterface(REFIID riid, void** ppvObject)
{
	if (!ppvObject)
		return E_POINTER;

	if (riid == __uuidof(ID3D11InputLayout) ||
		riid == __uuidof(ID3D11DeviceChild) ||
		riid == __uuidof(IUnknown) ||
		IsEqualIID(riid, GUID_HackerInputLayout))
	{
		*ppvObject = this;
		AddRef();
		return S_OK;
	}

	*ppvObject = nullptr;
	return mOrigLayout->QueryInterface(riid, ppvObject);
}

ULONG STDMETHODCALLTYPE HackerInputLayout::AddRef()
{
	return ++mRefCount;
}

ULONG STDMETHODCALLTYPE HackerInputLayout::Release()
{
	ULONG ref = --mRefCount;

	if (ref == 0)
	{
		// Do not Release mOrigLayout — side-car never owned the game's Create ref.
		// Releasing it here double-freed the real layout and corrupted D3D state.
		delete this;
	}

	return ref;
}
#pragma endregion IUnknown

#pragma region ID3D11DeviceChild
void STDMETHODCALLTYPE HackerInputLayout::GetDevice(ID3D11Device** ppDevice)
{
	mOrigLayout->GetDevice(ppDevice);
}

HRESULT STDMETHODCALLTYPE HackerInputLayout::GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData)
{
	return mOrigLayout->GetPrivateData(guid, pDataSize, pData);
}

HRESULT STDMETHODCALLTYPE HackerInputLayout::SetPrivateData(REFGUID guid, UINT DataSize, const void* pData)
{
	return mOrigLayout->SetPrivateData(guid, DataSize, pData);
}

HRESULT STDMETHODCALLTYPE HackerInputLayout::SetPrivateDataInterface(REFGUID guid, const IUnknown* pData)
{
	return mOrigLayout->SetPrivateDataInterface(guid, pData);
}
#pragma endregion ID3D11DeviceChild
