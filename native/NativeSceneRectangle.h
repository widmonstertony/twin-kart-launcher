#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <atomic>
#include <cmath>
#include "NativeTemporalContract.h"
namespace TwinKartGraphics {
// Same-frame producer viewport evidence, attached to the resource itself so
// recycled COM addresses cannot inherit an old texture's rendering rectangle.
inline constexpr GUID NativeSceneRectangleKey={0xf5a279da,0x84dd,0x4bcb,{0x8c,0x41,0x1b,0xdd,0xa2,0xb8,0x74,0x9e}};
inline std::atomic<unsigned long long> sceneWriteEpoch{1};
struct NativeSceneRectangleStamp {
    ID3D11DeviceContext* context=nullptr;unsigned long long epoch=0;
    unsigned frame=0;DWORD thread=0;NativeRect rectangle{};
};
inline void InvalidateNativeScene(ID3D11Resource* resource) {
    if(resource)resource->SetPrivateData(NativeSceneRectangleKey,0,nullptr);
}
inline void InvalidateNativeSceneView(ID3D11View* view) {
    if(!view)return;Microsoft::WRL::ComPtr<ID3D11Resource> resource;view->GetResource(&resource);InvalidateNativeScene(resource.Get());
}
inline void InvalidateNativeSceneCommandList(){sceneWriteEpoch.fetch_add(1,std::memory_order_acq_rel);}
inline void InvalidateNativeComputeTargets(ID3D11DeviceContext* context) {
    ID3D11UnorderedAccessView* writes[8]{};context->CSGetUnorderedAccessViews(0,8,writes);
    for(auto p:writes)if(p){InvalidateNativeSceneView(p);p->Release();}
}
inline void RecordNativeSceneDraw(ID3D11DeviceContext* context,unsigned frame) {
    if(!context || context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE || !frame)return;
    ID3D11RenderTargetView* targets[8]{};context->OMGetRenderTargets(8,targets,nullptr);
    ID3D11UnorderedAccessView* writes[8]{};context->OMGetRenderTargetsAndUnorderedAccessViews(0,nullptr,nullptr,0,8,writes);
    for(auto p:writes)if(p){InvalidateNativeSceneView(p);p->Release();}
    D3D11_VIEWPORT viewport{};UINT count=1;context->RSGetViewports(&count,&viewport);
    const float values[]{viewport.TopLeftX,viewport.TopLeftY,viewport.Width,viewport.Height};bool valid=count==1;
    for(float v:values)valid=valid && std::isfinite(v) && v>=0 && v<=8192 && floorf(v)==v;
    NativeRect rectangle{};
    if(valid)rectangle={static_cast<int>(viewport.TopLeftX),static_cast<int>(viewport.TopLeftY),
        static_cast<int>(viewport.TopLeftX+viewport.Width),static_cast<int>(viewport.TopLeftY+viewport.Height)};
    for(auto view:targets)if(view) {
        Microsoft::WRL::ComPtr<ID3D11Resource> resource;view->GetResource(&resource);InvalidateNativeScene(resource.Get());
        Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;D3D11_RENDER_TARGET_VIEW_DESC binding{};view->GetDesc(&binding);
        if(valid && SUCCEEDED(resource.As(&texture))) {
            D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
            if(desc.Usage==D3D11_USAGE_DEFAULT && desc.SampleDesc.Count==1 && desc.MipLevels==1 && desc.ArraySize==1 &&
               !desc.CPUAccessFlags && !(desc.MiscFlags&D3D11_RESOURCE_MISC_TILED) &&
               binding.ViewDimension==D3D11_RTV_DIMENSION_TEXTURE2D && binding.Texture2D.MipSlice==0 && rectangle.Fits(desc.Width,desc.Height)) {
                NativeSceneRectangleStamp stamp{context,sceneWriteEpoch.load(std::memory_order_acquire),frame,GetCurrentThreadId(),rectangle};
                resource->SetPrivateData(NativeSceneRectangleKey,sizeof(stamp),&stamp);
            }
        }
        view->Release();
    }
}
inline bool NativeSceneRectangle(ID3D11Texture2D* texture,ID3D11DeviceContext* context,unsigned frame,
    unsigned logicalWidth,unsigned logicalHeight,NativeRect& rectangle) {
    if(!texture || !context || !frame)return false;
    NativeSceneRectangleStamp stamp{};UINT size=sizeof(stamp);D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
    if(FAILED(texture->GetPrivateData(NativeSceneRectangleKey,&size,&stamp)) || size!=sizeof(stamp) ||
       stamp.context!=context || stamp.frame!=frame || stamp.thread!=GetCurrentThreadId() ||
       stamp.epoch!=sceneWriteEpoch.load(std::memory_order_acquire) || !stamp.rectangle.Fits(desc.Width,desc.Height) ||
       stamp.rectangle.Width()!=logicalWidth || stamp.rectangle.Height()!=logicalHeight)return false;
    rectangle=stamp.rectangle;return true;
}
}
