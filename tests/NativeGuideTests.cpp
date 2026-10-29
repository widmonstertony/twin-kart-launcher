#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdio>
#include <cmath>
#include <stdexcept>
#include "../native/NativeGuideShader.h"
using Microsoft::WRL::ComPtr;
void Need(bool value,const char* why){if(!value)throw std::runtime_error(why);}
int main(){try{
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Need(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"Guide WARP device");
    ComPtr<ID3DBlob> code,errors;Need(SUCCEEDED(D3DCompile(TwinKartGraphics::NativeGuideShader,sizeof(TwinKartGraphics::NativeGuideShader),nullptr,nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&errors)),"Actual guide conversion shader compile");
    ComPtr<ID3D11ComputeShader> shader;Need(SUCCEEDED(device->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&shader)),"Guide shader creation");
    float depths[32];for(unsigned i=0;i<32;++i)depths[i]=static_cast<float>(i%8)/8;
    unsigned short velocities[128]{};
    D3D11_TEXTURE2D_DESC desc{};desc.Width=8;desc.Height=4;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;desc.Format=DXGI_FORMAT_R32_FLOAT;
    ComPtr<ID3D11Texture2D> depth,velocity;D3D11_SUBRESOURCE_DATA data{depths,32,0};Need(SUCCEEDED(device->CreateTexture2D(&desc,&data,&depth)),"Native depth fixture");
    desc.Format=DXGI_FORMAT_R16G16B16A16_UNORM;data={velocities,64,0};Need(SUCCEEDED(device->CreateTexture2D(&desc,&data,&velocity)),"Native velocity fixture");
    ComPtr<ID3D11ShaderResourceView> depthView,velocityView;
    Need(SUCCEEDED(device->CreateShaderResourceView(depth.Get(),nullptr,&depthView)) && SUCCEEDED(device->CreateShaderResourceView(velocity.Get(),nullptr,&velocityView)),"Guide input views");
    desc.BindFlags=D3D11_BIND_UNORDERED_ACCESS;desc.Format=DXGI_FORMAT_R32_FLOAT;
    ComPtr<ID3D11Texture2D> outDepth,outMotion;Need(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&outDepth)),"Converted depth");
    desc.Format=DXGI_FORMAT_R32G32_FLOAT;Need(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&outMotion)),"Converted motion");
    ComPtr<ID3D11UnorderedAccessView> depthWrite,motionWrite;
    Need(SUCCEEDED(device->CreateUnorderedAccessView(outDepth.Get(),nullptr,&depthWrite)) && SUCCEEDED(device->CreateUnorderedAccessView(outMotion.Get(),nullptr,&motionWrite)),"Converted views");
    float view[2048]{};view[504]=view[509]=view[514]=view[519]=1;view[516]=0.25f;view[520]=0.25f;view[536]=8;view[537]=4;
    D3D11_BUFFER_DESC constants{};constants.ByteWidth=sizeof(view);constants.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    data={view,0,0};ComPtr<ID3D11Buffer> buffer;Need(SUCCEEDED(device->CreateBuffer(&constants,&data,&buffer)),"Native view fixture");
    ID3D11Buffer* bound=buffer.Get();context->CSSetConstantBuffers(1,1,&bound);context->CSSetShader(shader.Get(),nullptr,0);
    ID3D11ShaderResourceView* reads[]{depthView.Get(),velocityView.Get()};ID3D11UnorderedAccessView* writes[]{depthWrite.Get(),motionWrite.Get()};
    context->CSSetShaderResources(0,2,reads);context->CSSetUnorderedAccessViews(0,2,writes,nullptr);
    auto inspect=[&](int shift,float expectedMotion){
        context->Dispatch(1,1,1);
        for(unsigned kind=0;kind<2;++kind){
            auto texture=kind?outMotion.Get():outDepth.Get();texture->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
            ComPtr<ID3D11Texture2D> copy;Need(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&copy)),"Guide readback allocation");context->CopyResource(copy.Get(),texture);
            D3D11_MAPPED_SUBRESOURCE mapped{};Need(SUCCEEDED(context->Map(copy.Get(),0,D3D11_MAP_READ,0,&mapped)),"Guide GPU readback");
            bool correct=true;
            for(int y=0;y<4;++y)for(int x=0;x<8;++x){
                auto pixel=reinterpret_cast<const float*>(static_cast<const BYTE*>(mapped.pData)+y*mapped.RowPitch)+x*(kind?2:1);
                float expected=kind?expectedMotion:static_cast<float>((x+shift)<0?0:(x+shift)>7?7:x+shift)/8;
                if(!std::isfinite(pixel[0]) || fabsf(pixel[0]-expected)>0.001f || (kind && fabsf(pixel[1])>0.001f))correct=false;
            }
            context->Unmap(copy.Get(),0);Need(correct,"Native jitter/camera/object guide conversion mismatch");
        }
    };
    inspect(1,1);
    view[520]=-0.25f;context->UpdateSubresource(buffer.Get(),0,nullptr,view,0,0);inspect(-1,1);
    for(unsigned i=0;i<32;++i){velocities[i*4]=static_cast<unsigned short>(lround((32767.0/65535.0+0.2495*0.25)*65535));velocities[i*4+1]=32767;}
    context->UpdateSubresource(velocity.Get(),0,nullptr,velocities,64,0);inspect(-1,-1);
    context->ClearState();
    // Execute the raw pre-AA variant too: the cropped jittered pixel remains
    // jittered for NGX, rather than being shifted as in the resolved NR path.
    D3D_SHADER_MACRO definitions[]={{"TK_TEMPORAL_INPUT","1"},{nullptr,nullptr}};
    code.Reset();Need(SUCCEEDED(D3DCompile(TwinKartGraphics::NativeGuideShader,sizeof(TwinKartGraphics::NativeGuideShader),nullptr,definitions,nullptr,"main","cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&errors)),"Raw temporal guide shader compile");
    ComPtr<ID3D11ComputeShader> raw;Need(SUCCEEDED(device->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&raw)),"Raw temporal guide shader");
    memset(velocities,0,sizeof(velocities));context->UpdateSubresource(velocity.Get(),0,nullptr,velocities,64,0);
    float colors[128]{};for(unsigned i=0;i<32;++i){colors[i*4]=static_cast<float>(i);colors[i*4+3]=1;}
    desc={};desc.Width=8;desc.Height=4;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
    desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;desc.Format=DXGI_FORMAT_R32G32B32A32_FLOAT;
    ComPtr<ID3D11Texture2D> color;data={colors,128,0};Need(SUCCEEDED(device->CreateTexture2D(&desc,&data,&color)),"Raw HDR fixture");
    ComPtr<ID3D11ShaderResourceView> colorView;Need(SUCCEEDED(device->CreateShaderResourceView(color.Get(),nullptr,&colorView)),"Raw color view");
    desc.Width=6;desc.Height=2;desc.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
    ComPtr<ID3D11Texture2D> rawDepth,rawMotion,rawColor;ComPtr<ID3D11UnorderedAccessView> rawDw,rawMw,rawCw;
    auto create=[&](DXGI_FORMAT format,ComPtr<ID3D11Texture2D>& texture,ComPtr<ID3D11UnorderedAccessView>& write){desc.Format=format;return SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&texture)) && SUCCEEDED(device->CreateUnorderedAccessView(texture.Get(),nullptr,&write));};
    Need(create(DXGI_FORMAT_R32_FLOAT,rawDepth,rawDw) && create(DXGI_FORMAT_R32G32_FLOAT,rawMotion,rawMw) && create(DXGI_FORMAT_R32G32B32A32_FLOAT,rawColor,rawCw),"Raw cropped output resources");
    unsigned crop[4]={1,1,6,2};constants.ByteWidth=16;data={crop,0,0};ComPtr<ID3D11Buffer> rect;
    Need(SUCCEEDED(device->CreateBuffer(&constants,&data,&rect)),"Raw input rect constants");
    view[536]=6;view[537]=2;context->UpdateSubresource(buffer.Get(),0,nullptr,view,0,0);
    ID3D11Buffer* viewBuffer=buffer.Get();ID3D11Buffer* rectBuffer=rect.Get();context->CSSetConstantBuffers(1,1,&viewBuffer);context->CSSetConstantBuffers(3,1,&rectBuffer);
    ID3D11ShaderResourceView* rawReads[]={depthView.Get(),velocityView.Get(),colorView.Get()};ID3D11UnorderedAccessView* rawWrites[]={rawDw.Get(),rawMw.Get(),rawCw.Get()};
    context->CSSetShader(raw.Get(),nullptr,0);context->CSSetShaderResources(0,3,rawReads);context->CSSetUnorderedAccessViews(0,3,rawWrites,nullptr);context->Dispatch(1,1,1);
    for(unsigned kind=0;kind<3;++kind) {
        auto texture=kind==0?rawDepth.Get():kind==1?rawMotion.Get():rawColor.Get();texture->GetDesc(&desc);
        desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> copy;Need(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&copy)),"Raw guide readback");context->CopyResource(copy.Get(),texture);
        D3D11_MAPPED_SUBRESOURCE mapped{};Need(SUCCEEDED(context->Map(copy.Get(),0,D3D11_MAP_READ,0,&mapped)),"Raw guide GPU map");bool correct=true;
        for(unsigned y=0;y<2;++y)for(unsigned x=0;x<6;++x) {
            unsigned channels=kind==0?1:kind==1?2:4;
            auto pixel=reinterpret_cast<const float*>(static_cast<const BYTE*>(mapped.pData)+y*mapped.RowPitch)+x*channels;
            float expected=kind==0?(x+1)/8.0f:kind==1?0.75f:static_cast<float>((y+1)*8+x+1);
            if(fabsf(pixel[0]-expected)>0.001f || (kind==1 && fabsf(pixel[1])>0.001f))correct=false;
        }
        context->Unmap(copy.Get(),0);Need(correct,"Raw cropped guides/color shifted or camera motion wrong");
    }
    context->ClearState();puts("PASS actual WARP guide shaders: resolved jitter alignment/edge clamps, camera/object motion and raw HDR subrect conversion without double unjittering. No game or NVIDIA runtime started.");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
