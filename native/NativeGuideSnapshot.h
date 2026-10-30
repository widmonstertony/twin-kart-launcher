#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include "NativeTemporalContract.h"
namespace TwinKartGraphics {
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
    if(failure)*failure=0;
    layout=decoded;snapshot=captured;return true;
}
}
