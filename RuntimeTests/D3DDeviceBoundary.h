#pragma once

#include <d3d11_1.h>
#include <wrl/client.h>

// Pass through the D3D device boundary while recording creation descriptors.
// WARP normalizes non-structured strides, so querying the finished buffer cannot
// reveal an invalid descriptor submitted by the runtime. All domain code stays real.
class D3DDeviceBoundary final : public ID3D11Device1
{
	Microsoft::WRL::ComPtr<ID3D11Device1> device;
	LONG references = 1;

  public:
	D3D11_BUFFER_DESC last_buffer_description{};
	unsigned buffer_creations = 0;
	bool fail_staging = false;

	explicit D3DDeviceBoundary(ID3D11Device1 *original) : device(original) {}
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void **result) override
	{
		if (!result)
			return E_POINTER;
		if (iid == __uuidof(IUnknown) || iid == __uuidof(ID3D11Device) || iid == __uuidof(ID3D11Device1))
		{
			*result = static_cast<ID3D11Device1 *>(this);
			AddRef();
			return S_OK;
		}
		*result = nullptr;
		return E_NOINTERFACE;
	}
	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return static_cast<ULONG>(InterlockedIncrement(&references));
	}
	ULONG STDMETHODCALLTYPE Release() override
	{
		const ULONG remaining = static_cast<ULONG>(InterlockedDecrement(&references));
		if (!remaining)
			delete this;
		return remaining;
	}
	HRESULT STDMETHODCALLTYPE CreateBuffer(const D3D11_BUFFER_DESC *pDesc, const D3D11_SUBRESOURCE_DATA *pInitialData,
	                                       ID3D11Buffer **ppBuffer) override
	{
		last_buffer_description = *pDesc;
		++buffer_creations;
		if (fail_staging && pDesc->Usage == D3D11_USAGE_STAGING)
		{
			*ppBuffer = nullptr;
			return E_OUTOFMEMORY;
		}
		return device->CreateBuffer(pDesc, pInitialData, ppBuffer);
	}

	HRESULT STDMETHODCALLTYPE CreateTexture1D(const D3D11_TEXTURE1D_DESC *pDesc,
	                                          const D3D11_SUBRESOURCE_DATA *pInitialData,
	                                          ID3D11Texture1D **ppTexture1D) override
	{
		return device->CreateTexture1D(pDesc, pInitialData, ppTexture1D);
	}

	HRESULT STDMETHODCALLTYPE CreateTexture2D(const D3D11_TEXTURE2D_DESC *pDesc,
	                                          const D3D11_SUBRESOURCE_DATA *pInitialData,
	                                          ID3D11Texture2D **ppTexture2D) override
	{
		return device->CreateTexture2D(pDesc, pInitialData, ppTexture2D);
	}

	HRESULT STDMETHODCALLTYPE CreateTexture3D(const D3D11_TEXTURE3D_DESC *pDesc,
	                                          const D3D11_SUBRESOURCE_DATA *pInitialData,
	                                          ID3D11Texture3D **ppTexture3D) override
	{
		return device->CreateTexture3D(pDesc, pInitialData, ppTexture3D);
	}

	HRESULT STDMETHODCALLTYPE CreateShaderResourceView(ID3D11Resource *pResource,
	                                                   const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
	                                                   ID3D11ShaderResourceView **ppSRView) override
	{
		return device->CreateShaderResourceView(pResource, pDesc, ppSRView);
	}

	HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView(ID3D11Resource *pResource,
	                                                    const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
	                                                    ID3D11UnorderedAccessView **ppUAView) override
	{
		return device->CreateUnorderedAccessView(pResource, pDesc, ppUAView);
	}

	HRESULT STDMETHODCALLTYPE CreateRenderTargetView(ID3D11Resource *pResource,
	                                                 const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
	                                                 ID3D11RenderTargetView **ppRTView) override
	{
		return device->CreateRenderTargetView(pResource, pDesc, ppRTView);
	}

	HRESULT STDMETHODCALLTYPE CreateDepthStencilView(ID3D11Resource *pResource,
	                                                 const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
	                                                 ID3D11DepthStencilView **ppDepthStencilView) override
	{
		return device->CreateDepthStencilView(pResource, pDesc, ppDepthStencilView);
	}

	HRESULT STDMETHODCALLTYPE CreateInputLayout(const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs, UINT NumElements,
	                                            const void *pShaderBytecodeWithInputSignature, SIZE_T BytecodeLength,
	                                            ID3D11InputLayout **ppInputLayout) override
	{
		return device->CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature,
		                                 BytecodeLength, ppInputLayout);
	}

	HRESULT STDMETHODCALLTYPE CreateVertexShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
	                                             ID3D11ClassLinkage *pClassLinkage,
	                                             ID3D11VertexShader **ppVertexShader) override
	{
		return device->CreateVertexShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
	}

	HRESULT STDMETHODCALLTYPE CreateGeometryShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
	                                               ID3D11ClassLinkage *pClassLinkage,
	                                               ID3D11GeometryShader **ppGeometryShader) override
	{
		return device->CreateGeometryShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppGeometryShader);
	}

	HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput(const void *pShaderBytecode, SIZE_T BytecodeLength,
	                                                               const D3D11_SO_DECLARATION_ENTRY *pSODeclaration,
	                                                               UINT NumEntries, const UINT *pBufferStrides,
	                                                               UINT NumStrides, UINT RasterizedStream,
	                                                               ID3D11ClassLinkage *pClassLinkage,
	                                                               ID3D11GeometryShader **ppGeometryShader) override
	{
		return device->CreateGeometryShaderWithStreamOutput(pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries,
		                                                    pBufferStrides, NumStrides, RasterizedStream, pClassLinkage,
		                                                    ppGeometryShader);
	}

	HRESULT STDMETHODCALLTYPE CreatePixelShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
	                                            ID3D11ClassLinkage *pClassLinkage,
	                                            ID3D11PixelShader **ppPixelShader) override
	{
		return device->CreatePixelShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
	}

	HRESULT STDMETHODCALLTYPE CreateHullShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
	                                           ID3D11ClassLinkage *pClassLinkage,
	                                           ID3D11HullShader **ppHullShader) override
	{
		return device->CreateHullShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppHullShader);
	}

	HRESULT STDMETHODCALLTYPE CreateDomainShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
	                                             ID3D11ClassLinkage *pClassLinkage,
	                                             ID3D11DomainShader **ppDomainShader) override
	{
		return device->CreateDomainShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppDomainShader);
	}

	HRESULT STDMETHODCALLTYPE CreateComputeShader(const void *pShaderBytecode, SIZE_T BytecodeLength,
	                                              ID3D11ClassLinkage *pClassLinkage,
	                                              ID3D11ComputeShader **ppComputeShader) override
	{
		return device->CreateComputeShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppComputeShader);
	}

	HRESULT STDMETHODCALLTYPE CreateClassLinkage(ID3D11ClassLinkage **ppLinkage) override
	{
		return device->CreateClassLinkage(ppLinkage);
	}

	HRESULT STDMETHODCALLTYPE CreateBlendState(const D3D11_BLEND_DESC *pBlendStateDesc,
	                                           ID3D11BlendState **ppBlendState) override
	{
		return device->CreateBlendState(pBlendStateDesc, ppBlendState);
	}

	HRESULT STDMETHODCALLTYPE CreateDepthStencilState(const D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc,
	                                                  ID3D11DepthStencilState **ppDepthStencilState) override
	{
		return device->CreateDepthStencilState(pDepthStencilDesc, ppDepthStencilState);
	}

	HRESULT STDMETHODCALLTYPE CreateRasterizerState(const D3D11_RASTERIZER_DESC *pRasterizerDesc,
	                                                ID3D11RasterizerState **ppRasterizerState) override
	{
		return device->CreateRasterizerState(pRasterizerDesc, ppRasterizerState);
	}

	HRESULT STDMETHODCALLTYPE CreateSamplerState(const D3D11_SAMPLER_DESC *pSamplerDesc,
	                                             ID3D11SamplerState **ppSamplerState) override
	{
		return device->CreateSamplerState(pSamplerDesc, ppSamplerState);
	}

	HRESULT STDMETHODCALLTYPE CreateQuery(const D3D11_QUERY_DESC *pQueryDesc, ID3D11Query **ppQuery) override
	{
		return device->CreateQuery(pQueryDesc, ppQuery);
	}

	HRESULT STDMETHODCALLTYPE CreatePredicate(const D3D11_QUERY_DESC *pPredicateDesc,
	                                          ID3D11Predicate **ppPredicate) override
	{
		return device->CreatePredicate(pPredicateDesc, ppPredicate);
	}

	HRESULT STDMETHODCALLTYPE CreateCounter(const D3D11_COUNTER_DESC *pCounterDesc, ID3D11Counter **ppCounter) override
	{
		return device->CreateCounter(pCounterDesc, ppCounter);
	}

	HRESULT STDMETHODCALLTYPE CreateDeferredContext(UINT ContextFlags, ID3D11DeviceContext **ppDeferredContext) override
	{
		return device->CreateDeferredContext(ContextFlags, ppDeferredContext);
	}

	HRESULT STDMETHODCALLTYPE OpenSharedResource(HANDLE hResource, REFIID ReturnedInterface, void **ppResource) override
	{
		return device->OpenSharedResource(hResource, ReturnedInterface, ppResource);
	}

	HRESULT STDMETHODCALLTYPE CheckFormatSupport(DXGI_FORMAT Format, UINT *pFormatSupport) override
	{
		return device->CheckFormatSupport(Format, pFormatSupport);
	}

	HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels(DXGI_FORMAT Format, UINT SampleCount,
	                                                        UINT *pNumQualityLevels) override
	{
		return device->CheckMultisampleQualityLevels(Format, SampleCount, pNumQualityLevels);
	}

	void STDMETHODCALLTYPE CheckCounterInfo(D3D11_COUNTER_INFO *pCounterInfo) override
	{
		device->CheckCounterInfo(pCounterInfo);
	}

	HRESULT STDMETHODCALLTYPE CheckCounter(const D3D11_COUNTER_DESC *pDesc, D3D11_COUNTER_TYPE *pType,
	                                       UINT *pActiveCounters, LPSTR szName, UINT *pNameLength, LPSTR szUnits,
	                                       UINT *pUnitsLength, LPSTR szDescription, UINT *pDescriptionLength) override
	{
		return device->CheckCounter(pDesc, pType, pActiveCounters, szName, pNameLength, szUnits, pUnitsLength,
		                            szDescription, pDescriptionLength);
	}

	HRESULT STDMETHODCALLTYPE CheckFeatureSupport(D3D11_FEATURE Feature, void *pFeatureSupportData,
	                                              UINT FeatureSupportDataSize) override
	{
		return device->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
	}

	HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT *pDataSize, void *pData) override
	{
		return device->GetPrivateData(guid, pDataSize, pData);
	}

	HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void *pData) override
	{
		return device->SetPrivateData(guid, DataSize, pData);
	}

	HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown *pData) override
	{
		return device->SetPrivateDataInterface(guid, pData);
	}

	D3D_FEATURE_LEVEL STDMETHODCALLTYPE GetFeatureLevel() override
	{
		return device->GetFeatureLevel();
	}

	UINT STDMETHODCALLTYPE GetCreationFlags() override
	{
		return device->GetCreationFlags();
	}

	HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason() override
	{
		return device->GetDeviceRemovedReason();
	}

	void STDMETHODCALLTYPE GetImmediateContext(ID3D11DeviceContext **ppImmediateContext) override
	{
		device->GetImmediateContext(ppImmediateContext);
	}

	HRESULT STDMETHODCALLTYPE SetExceptionMode(UINT RaiseFlags) override
	{
		return device->SetExceptionMode(RaiseFlags);
	}

	UINT STDMETHODCALLTYPE GetExceptionMode() override
	{
		return device->GetExceptionMode();
	}

	void STDMETHODCALLTYPE GetImmediateContext1(ID3D11DeviceContext1 **ppImmediateContext) override
	{
		device->GetImmediateContext1(ppImmediateContext);
	}

	HRESULT STDMETHODCALLTYPE CreateDeferredContext1(UINT ContextFlags,
	                                                 ID3D11DeviceContext1 **ppDeferredContext) override
	{
		return device->CreateDeferredContext1(ContextFlags, ppDeferredContext);
	}

	HRESULT STDMETHODCALLTYPE CreateBlendState1(const D3D11_BLEND_DESC1 *pBlendStateDesc,
	                                            ID3D11BlendState1 **ppBlendState) override
	{
		return device->CreateBlendState1(pBlendStateDesc, ppBlendState);
	}

	HRESULT STDMETHODCALLTYPE CreateRasterizerState1(const D3D11_RASTERIZER_DESC1 *pRasterizerDesc,
	                                                 ID3D11RasterizerState1 **ppRasterizerState) override
	{
		return device->CreateRasterizerState1(pRasterizerDesc, ppRasterizerState);
	}

	HRESULT STDMETHODCALLTYPE CreateDeviceContextState(UINT Flags, const D3D_FEATURE_LEVEL *pFeatureLevels,
	                                                   UINT FeatureLevels, UINT SDKVersion, REFIID EmulatedInterface,
	                                                   D3D_FEATURE_LEVEL *pChosenFeatureLevel,
	                                                   ID3DDeviceContextState **ppContextState) override
	{
		return device->CreateDeviceContextState(Flags, pFeatureLevels, FeatureLevels, SDKVersion, EmulatedInterface,
		                                        pChosenFeatureLevel, ppContextState);
	}

	HRESULT STDMETHODCALLTYPE OpenSharedResource1(HANDLE hResource, REFIID returnedInterface,
	                                              void **ppResource) override
	{
		return device->OpenSharedResource1(hResource, returnedInterface, ppResource);
	}

	HRESULT STDMETHODCALLTYPE OpenSharedResourceByName(LPCWSTR lpName, DWORD dwDesiredAccess, REFIID returnedInterface,
	                                                   void **ppResource) override
	{
		return device->OpenSharedResourceByName(lpName, dwDesiredAccess, returnedInterface, ppResource);
	}
};
