#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include "NativeTemporalContract.h"
#include "NativeViewMath.h"
namespace TwinKartGraphics {
// D3D11 owns a copy of these CPU bytes for exactly the immutable buffer's
// lifetime. Reading this private data does not submit GPU work or map a GPU
// resource; the original native View was already read at its dispatch.
inline constexpr GUID NativeGuideSnapshotViewId={0x0bb45bf6,0x588a,0x4ced,{0x92,0x7c,0x53,0x16,0x32,0x4f,0x2e,0x61}};
struct NativeGuideSnapshotFacts {
    float parameters[NativeTemporalParameterFloats]{};
    float viewExtent[2]{},viewJitter[2]{},exposure[2]{};
};
inline bool ReadNativeGuideBuffer(ID3D11DeviceContext* context,ID3D11Buffer* source,
    void* destination,unsigned count,unsigned expected) {
    if(!context || !source || !destination || count>expected || context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
    D3D11_BUFFER_DESC desc{};source->GetDesc(&desc);if(desc.ByteWidth!=expected)return false;
    Microsoft::WRL::ComPtr<ID3D11Device> device;context->GetDevice(&device);
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=desc.MiscFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    Microsoft::WRL::ComPtr<ID3D11Buffer> copy;
    if(FAILED(device->CreateBuffer(&desc,nullptr,&copy)))return false;
    context->CopyResource(copy.Get(),source);D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(context->Map(copy.Get(),0,D3D11_MAP_READ,0,&mapped)))return false;
    memcpy(destination,mapped.pData,count);context->Unmap(copy.Get(),0);return true;
}
inline bool ReadNativeGuideSnapshotView(ID3D11DeviceContext* context,ID3D11Buffer* buffer,
    unsigned guideFrame,unsigned frame,unsigned width,unsigned height,
    float* jitter=nullptr,CameraFrame* camera=nullptr,bool reset=false) {
    if(!context || !buffer || !frame || guideFrame!=frame || !width || !height ||
       context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE)return false;
    D3D11_BUFFER_DESC desc{};buffer->GetDesc(&desc);
    if(desc.ByteWidth!=8192 || desc.Usage!=D3D11_USAGE_IMMUTABLE ||
       desc.BindFlags!=D3D11_BIND_CONSTANT_BUFFER || desc.CPUAccessFlags || desc.MiscFlags)return false;
    Microsoft::WRL::ComPtr<ID3D11Device> contextDevice,bufferDevice;
    context->GetDevice(&contextDevice);buffer->GetDevice(&bufferDevice);
    if(contextDevice.Get()!=bufferDevice.Get())return false;
    float view[2048]{};UINT bytes=sizeof(view);
    if(FAILED(buffer->GetPrivateData(NativeGuideSnapshotViewId,&bytes,view)) || bytes!=sizeof(view))return false;
    for(unsigned i=0;i<576;++i)if(!std::isfinite(view[i]))return false;
    // Preserve the exact native ViewSizeAndInvSize, inverted-Z perspective,
    // jitter and current/previous no-AA transform checks of the GPU reader.
    bool valid=view[536]==static_cast<float>(width) && view[537]==static_cast<float>(height) &&
        fabsf(view[538]*width-1)<0.001f && fabsf(view[539]*height-1)<0.001f &&
        view[142]==10 && view[143]==0 && view[139]==1 && view[138]==0 &&
        fabsf(view[520])<0.01f && fabsf(view[521])<0.01f &&
        fabsf(view[519])>0.1f && fabsf(view[519])<10;
    if(valid) {
        double current[4][4],previous[4][4];
        for(int r=0;r<4;++r)for(int c=0;c<4;++c) {
            current[r][c]=view[32+r*4+c];previous[r][c]=view[316+r*4+c];
        }
        for(int r=0;r<4;++r) {
            current[r][0]-=current[r][3]*view[520];current[r][1]-=current[r][3]*view[521];
            previous[r][0]-=previous[r][3]*view[522];previous[r][1]-=previous[r][3]*view[523];
        }
        for(int r=0;r<4;++r)for(int c=0;c<4;++c) {
            double value=0;for(int k=0;k<4;++k)value+=current[r][k]*view[504+k*4+c];
            if(fabs(value-previous[r][c])>0.02*(1+fabs(previous[r][c])))valid=false;
        }
    }
    CameraFrame decoded{};
    if(valid && camera)valid=DecodeNativeCamera(view,width,height,reset,decoded);
    if(!valid)return false;
    if(jitter){jitter[0]=-view[520]*width*0.5f;jitter[1]=view[521]*height*0.5f;}
    if(camera)*camera=decoded;
    return true;
}
// Capture at the identified native temporal dispatch. Native constant buffers
// can be overwritten before the pre-HUD composite; retaining their COM pointer
// alone does not preserve the same view or its actual rendering rectangle.
inline bool SnapshotNativeGuides(ID3D11DeviceContext* context,ID3D11Buffer* parameters,ID3D11Buffer* view,
    unsigned inputWidth,unsigned inputHeight,unsigned outputWidth,unsigned outputHeight,
    NativeTemporalContract& layout,Microsoft::WRL::ComPtr<ID3D11Buffer>& snapshot,unsigned* failure=nullptr,NativeGuideSnapshotFacts* facts=nullptr) {
    float fields[NativeTemporalParameterFloats]{},values[2048]{};NativeTemporalContract decoded;
    if(failure)*failure=1;
    if(!ReadNativeGuideBuffer(context,parameters,fields,sizeof(fields),4096))return false;
    if(failure)*failure=2;
    if(!ReadNativeGuideBuffer(context,view,values,sizeof(values),8192))return false;
    if(facts){memcpy(facts->parameters,fields,sizeof(fields));
        memcpy(facts->viewExtent,values+536,sizeof(facts->viewExtent));
        memcpy(facts->viewJitter,values+520,sizeof(facts->viewJitter));
        memcpy(facts->exposure,values+557,sizeof(facts->exposure));}
    if(failure)*failure=3;
    if(!DecodeNativeTemporalLayout(fields,sizeof(fields),values,inputWidth,inputHeight,outputWidth,outputHeight,decoded))return false;
    Microsoft::WRL::ComPtr<ID3D11Device> device;context->GetDevice(&device);
    D3D11_BUFFER_DESC desc{};desc.ByteWidth=sizeof(values);desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;desc.Usage=D3D11_USAGE_IMMUTABLE;
    D3D11_SUBRESOURCE_DATA data{values,0,0};Microsoft::WRL::ComPtr<ID3D11Buffer> captured;
    if(failure)*failure=4;
    if(FAILED(device->CreateBuffer(&desc,&data,&captured)))return false;
    if(FAILED(captured->SetPrivateData(NativeGuideSnapshotViewId,sizeof(values),values)))return false;
    if(failure)*failure=0;
    layout=decoded;snapshot=captured;return true;
}
}
