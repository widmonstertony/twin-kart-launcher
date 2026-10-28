#define WIN32_LEAN_AND_MEAN
#include "SharedTextureBridge.h"
#include <dxgi1_4.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
namespace TwinKartGraphics {
struct SharedTextureBridge::Impl {
    ComPtr<ID3D11Device> device11; ComPtr<ID3D11Device5> device5; ComPtr<ID3D11DeviceContext4> context;
    ComPtr<ID3D12Device> device12; ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D11Texture2D> texture11; ComPtr<ID3D12Resource> texture12;
    ComPtr<ID3D11Fence> fence11; ComPtr<ID3D12Fence> fence12;
    UINT64 next=0; DWORD thread=0; bool in12=false; unsigned width=0,height=0; DXGI_FORMAT format=DXGI_FORMAT_UNKNOWN;
    bool Texture(ID3D11Texture2D* texture) const {
        if(!texture) return false; D3D11_TEXTURE2D_DESC desc{}; texture->GetDesc(&desc); ComPtr<ID3D11Device> parent; texture->GetDevice(&parent);
        return parent.Get()==device11.Get() && desc.Width==width && desc.Height==height && desc.MipLevels==1 && desc.ArraySize==1 && desc.SampleDesc.Count==1 && desc.Format==format;
    }
    bool Owner() const { return thread==GetCurrentThreadId(); }
};
SharedTextureBridge::SharedTextureBridge():impl(new Impl) {}
SharedTextureBridge::~SharedTextureBridge() { if(impl->context && impl->Owner()) Drain(); delete impl; }
HRESULT SharedTextureBridge::Initialize(ID3D11Device* d11,ID3D12Device* d12,ID3D12CommandQueue* queue,unsigned width,unsigned height,DXGI_FORMAT format) {
    if(impl->device11 || !d11 || !d12 || !queue || width<1 || height<1 || width>8192 || height>8192 ||
       (format!=DXGI_FORMAT_R10G10B10A2_UNORM && format!=DXGI_FORMAT_R16G16B16A16_FLOAT && format!=DXGI_FORMAT_R32_FLOAT && format!=DXGI_FORMAT_R16G16_FLOAT && format!=DXGI_FORMAT_R8G8B8A8_UNORM && format!=DXGI_FORMAT_R8_UNORM)) return E_INVALIDARG;
    ComPtr<IDXGIDevice> dxgi; ComPtr<IDXGIAdapter> adapter; DXGI_ADAPTER_DESC adapterDesc{};
    HRESULT hr=d11->QueryInterface(IID_PPV_ARGS(&dxgi)); if(FAILED(hr)) return hr;
    if(FAILED(hr=dxgi->GetAdapter(&adapter)) || FAILED(hr=adapter->GetDesc(&adapterDesc))) return hr;
    auto luid=d12->GetAdapterLuid(); if(luid.HighPart!=adapterDesc.AdapterLuid.HighPart || luid.LowPart!=adapterDesc.AdapterLuid.LowPart) return E_INVALIDARG;
    ComPtr<ID3D12Device> queueDevice; if(FAILED(hr=queue->GetDevice(IID_PPV_ARGS(&queueDevice)))) return hr;
    if(queueDevice.Get()!=d12 || queue->GetDesc().Type!=D3D12_COMMAND_LIST_TYPE_DIRECT) return E_INVALIDARG;
    impl->device11=d11; impl->device12=d12; impl->queue=queue; impl->thread=GetCurrentThreadId(); impl->width=width; impl->height=height; impl->format=format;
    if(FAILED(hr=d11->QueryInterface(IID_PPV_ARGS(&impl->device5)))) return hr;
    ComPtr<ID3D11DeviceContext> immediate; d11->GetImmediateContext(&immediate);
    if(FAILED(hr=immediate.As(&impl->context))) return hr;
    D3D12_HEAP_PROPERTIES properties{}; properties.Type=D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc{}; desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D; desc.Width=width; desc.Height=height;
    desc.DepthOrArraySize=1; desc.MipLevels=1; desc.Format=format; desc.SampleDesc.Count=1;
    desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_SIMULTANEOUS_ACCESS|D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS|D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    if(FAILED(hr=d12->CreateCommittedResource(&properties,D3D12_HEAP_FLAG_SHARED,&desc,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&impl->texture12)))) return hr;
    HANDLE textureHandle=nullptr;
    if(FAILED(hr=d12->CreateSharedHandle(impl->texture12.Get(),nullptr,GENERIC_ALL,nullptr,&textureHandle))) return hr;
    hr=impl->device5->OpenSharedResource1(textureHandle,IID_PPV_ARGS(&impl->texture11)); CloseHandle(textureHandle); if(FAILED(hr)) return hr;
    if(FAILED(hr=d12->CreateFence(0,D3D12_FENCE_FLAG_SHARED,IID_PPV_ARGS(&impl->fence12)))) return hr;
    HANDLE fenceHandle=nullptr;
    if(FAILED(hr=d12->CreateSharedHandle(impl->fence12.Get(),nullptr,GENERIC_ALL,nullptr,&fenceHandle))) return hr;
    hr=impl->device5->OpenSharedFence(fenceHandle,IID_PPV_ARGS(&impl->fence11)); CloseHandle(fenceHandle); return hr;
}
ID3D12Resource* SharedTextureBridge::Texture12() const { return impl->texture12.Get(); }
HRESULT SharedTextureBridge::CopyTo12(ID3D11Texture2D* source) {
    if(!impl->Owner() || !impl->fence11 || impl->in12 || !impl->Texture(source)) return E_INVALIDARG;
    impl->context->CopyResource(impl->texture11.Get(),source);
    UINT64 value=++impl->next; HRESULT hr=impl->context->Signal(impl->fence11.Get(),value); if(FAILED(hr)) return hr;
    impl->context->Flush(); hr=impl->queue->Wait(impl->fence12.Get(),value); if(SUCCEEDED(hr)) impl->in12=true; return hr;
}
HRESULT SharedTextureBridge::ReturnTo11(ID3D11Texture2D* target) {
    if(!impl->Owner() || !impl->in12 || (target && !impl->Texture(target))) return E_INVALIDARG;
    UINT64 value=++impl->next; HRESULT hr=impl->queue->Signal(impl->fence12.Get(),value); if(FAILED(hr)) return hr;
    if(FAILED(hr=impl->context->Wait(impl->fence11.Get(),value))) return hr;
    if(target) impl->context->CopyResource(target,impl->texture11.Get()); impl->in12=false; return S_OK;
}
HRESULT SharedTextureBridge::WaitForConsumer(ID3D12Fence* fence,UINT64 value) {
    if(!impl->Owner() || !impl->queue || !impl->fence11 || !fence || impl->in12) return E_INVALIDARG;
    HRESULT hr=impl->queue->Wait(fence,value); if(FAILED(hr)) return hr;
    UINT64 bridgeValue=++impl->next; if(FAILED(hr=impl->queue->Signal(impl->fence12.Get(),bridgeValue))) return hr;
    return impl->context->Wait(impl->fence11.Get(),bridgeValue);
}
HRESULT SharedTextureBridge::Drain(unsigned milliseconds) {
    if(!impl->Owner() || !impl->fence11 || milliseconds>30000) return E_INVALIDARG;
    if(impl->in12) { HRESULT returned=ReturnTo11(nullptr); if(FAILED(returned)) return returned; }
    UINT64 value=++impl->next; HRESULT hr=impl->context->Signal(impl->fence11.Get(),value); if(FAILED(hr)) return hr;
    impl->context->Flush(); HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr); if(!event) return HRESULT_FROM_WIN32(GetLastError());
    hr=impl->fence12->SetEventOnCompletion(value,event);
    if(SUCCEEDED(hr)) { DWORD waited=WaitForSingleObject(event,milliseconds); hr=waited==WAIT_OBJECT_0?S_OK:waited==WAIT_TIMEOUT?HRESULT_FROM_WIN32(WAIT_TIMEOUT):E_FAIL; }
    CloseHandle(event); return hr;
}
}
