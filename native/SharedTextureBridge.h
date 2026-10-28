#pragma once
#include <d3d11_4.h>
#include <d3d12.h>
namespace TwinKartGraphics {
// One synchronized texture shared between the native D3D11 renderer and the
// D3D12 reconstruction/presentation queue. No CPU texture transfer is used.
class SharedTextureBridge {
    struct Impl; Impl* impl;
public:
    SharedTextureBridge();
    ~SharedTextureBridge();
    SharedTextureBridge(const SharedTextureBridge&)=delete;
    SharedTextureBridge& operator=(const SharedTextureBridge&)=delete;
    HRESULT Initialize(ID3D11Device*,ID3D12Device*,ID3D12CommandQueue*,unsigned width,unsigned height,DXGI_FORMAT);
    ID3D12Resource* Texture12() const;
    HRESULT CopyTo12(ID3D11Texture2D* source);
    // Caller submits its D3D12 work and transitions the shared texture back to
    // COMMON before handing it to D3D11. target may be null for display-only use.
    HRESULT ReturnTo11(ID3D11Texture2D* target);
    // FG may consume inputs asynchronously. Queue this wait before overwriting
    // tagged textures, using the completion fence returned by DLSS-G.
    HRESULT WaitForConsumer(ID3D12Fence*,unsigned long long value);
    HRESULT Drain(unsigned milliseconds=10000);
};
}
