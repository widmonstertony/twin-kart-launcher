#pragma once
#include <d3d11.h>
#include <wrl/client.h>
namespace TwinKartGraphics {
// Only called for the independently identified native temporal pass. The
// renderer binds a 1x1 black sentinel when object velocity is absent. Verify
// its actual current pixel; camera motion still comes from depth/view matrices.
inline bool ReadNativeVelocity(ID3D11DeviceContext* context,ID3D11ShaderResourceView* view,unsigned width,unsigned height,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture,bool& cameraOnly) {
    cameraOnly=false;if(!context || !view || context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC binding{};view->GetDesc(&binding);
    if(binding.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURE2D || binding.Texture2D.MostDetailedMip || binding.Texture2D.MipLevels!=1)return false;
    Microsoft::WRL::ComPtr<ID3D11Resource> resource;view->GetResource(&resource);
    if(FAILED(resource.As(&texture)))return false;
    D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
    if(desc.ArraySize!=1 || desc.MipLevels!=1 || desc.SampleDesc.Count!=1)return false;
    // Separate pooled color/depth/velocity allocations need not have the same
    // extents. The caller supplies the required rectangle's far corner and
    // validates its origin; encoded velocity is loaded at those native pixels.
    if(desc.Width>=width && desc.Height>=height && desc.Width<=8192 && desc.Height<=8192 &&
       desc.Format==DXGI_FORMAT_R16G16B16A16_UNORM && binding.Format==desc.Format)return true;
    if(desc.Width!=1 || desc.Height!=1)return false;
    bool rgba=(desc.Format==DXGI_FORMAT_R8G8B8A8_TYPELESS || desc.Format==DXGI_FORMAT_R8G8B8A8_UNORM) && binding.Format==DXGI_FORMAT_R8G8B8A8_UNORM;
    bool bgra=(desc.Format==DXGI_FORMAT_B8G8R8A8_TYPELESS || desc.Format==DXGI_FORMAT_B8G8R8A8_UNORM) && binding.Format==DXGI_FORMAT_B8G8R8A8_UNORM;
    bool encoded=desc.Format==DXGI_FORMAT_R16G16B16A16_UNORM && binding.Format==desc.Format;
    if(!rgba && !bgra && !encoded)return false;
    // A single metadata-sized pixel, never a scene-color CPU transfer. Repeat
    // validation for mutable sentinels instead of assuming an address is black.
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=desc.MiscFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    Microsoft::WRL::ComPtr<ID3D11Device> device;context->GetDevice(&device);
    Microsoft::WRL::ComPtr<ID3D11Texture2D> copy;
    if(FAILED(device->CreateTexture2D(&desc,nullptr,&copy)))return false;
    context->CopyResource(copy.Get(),texture.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(context->Map(copy.Get(),0,D3D11_MAP_READ,0,&mapped)))return false;
    auto p=static_cast<const unsigned char*>(mapped.pData);
    bool zero=p[0]==0 && p[1]==0 && p[2]==0 && (!encoded || p[3]==0);
    context->Unmap(copy.Get(),0);cameraOnly=zero;return zero;
}
}
