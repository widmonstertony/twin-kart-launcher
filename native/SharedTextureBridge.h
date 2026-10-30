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
    // Borrowed view of the shared texture, valid while this bridge exists.
    ID3D11Texture2D* Texture11() const;
    HRESULT CopyTo12(ID3D11Texture2D* source);
    // Caller submits its D3D12 work and transitions the shared texture back to
    // COMMON before handing it to D3D11. target may be null for display-only use.
    HRESULT ReturnTo11(ID3D11Texture2D* target);
    // FG may consume inputs asynchronously. Queue this wait before overwriting
    // tagged textures, using the completion fence returned by DLSS-G.
    HRESULT WaitForConsumer(ID3D12Fence*,unsigned long long value);
    HRESULT Drain(unsigned milliseconds=10000);
    // Only at an explicit, externally serialized immediate-context handoff.
    // Does not change ownership or allow CopyTo12 from a foreign callback.
    // A texture still held by D3D12 cannot be retired through this boundary.
    HRESULT DrainForRetirement(unsigned expectedOwnerThread,ID3D11DeviceContext*,unsigned milliseconds=10000);
};
}
