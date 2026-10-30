#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdio>
#include <cmath>
#include <stdexcept>
#include <fstream>
#include <vector>
#include <algorithm>
#include "../native/NativeGuideShader.h"
#include "../native/NativeVelocity.h"
#include "../native/NativeGuideSnapshot.h"
using Microsoft::WRL::ComPtr;
void Need(bool value,const char* why){if(!value)throw std::runtime_error(why);}
void ReplayCapturedGuides(ID3D11Device* device,ID3D11DeviceContext* context,ID3D11ComputeShader* shader,
    ID3D11Buffer* viewBuffer,const float* view,const TwinKartGraphics::NativeTemporalContract& layout,const char* depthPath,const char* motionPath) {
    // This optional replay expects exact, unpadded captures. Dimensions and
    // formats are explicit; a partial file cannot stand in for missing pixels.
    unsigned w=layout.input.Width(),h=layout.input.Height();
    Need(layout.input.left==0 && layout.input.top==0 && w<=4096 && h<=4096,"Expected bounded unpadded guide captures");
    std::vector<unsigned char> depthBytes(static_cast<size_t>(w)*h*8),motionBytes(depthBytes.size());
    auto read=[](const char* path,std::vector<unsigned char>& bytes){std::ifstream file(path,std::ios::binary);file.read(reinterpret_cast<char*>(bytes.data()),bytes.size());return file.gcount()==static_cast<std::streamsize>(bytes.size()) && file.peek()==std::char_traits<char>::eof();};
    Need(read(depthPath,depthBytes) && read(motionPath,motionBytes),"Expected exact packed native depth/velocity files");
    D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> depth,motion;ComPtr<ID3D11ShaderResourceView> depthView,motionView;
    auto input=[&](DXGI_FORMAT format,DXGI_FORMAT binding,const std::vector<unsigned char>& bytes,ComPtr<ID3D11Texture2D>& texture,ComPtr<ID3D11ShaderResourceView>& srv){
        desc.Format=format;D3D11_SUBRESOURCE_DATA data{bytes.data(),w*8,0};Need(SUCCEEDED(device->CreateTexture2D(&desc,&data,&texture)),"Captured native texture allocation");
        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};sd.Format=binding;sd.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;sd.Texture2D.MipLevels=1;
        Need(SUCCEEDED(device->CreateShaderResourceView(texture.Get(),&sd,&srv)),"Captured native texture view");
    };
    input(DXGI_FORMAT_R32G8X24_TYPELESS,DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,depthBytes,depth,depthView);
    input(DXGI_FORMAT_R16G16B16A16_UNORM,DXGI_FORMAT_R16G16B16A16_UNORM,motionBytes,motion,motionView);
    ComPtr<ID3D11Texture2D> checked;bool cameraOnly=true;
    Need(TwinKartGraphics::ReadNativeVelocity(context,motionView.Get(),w,h,checked,cameraOnly) && !cameraOnly,"Actual dense native velocity rejected");
    const unsigned ow=w*3/2,oh=h*3/2;desc.Width=ow;desc.Height=oh;desc.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
    ComPtr<ID3D11Texture2D> convertedDepth,convertedMotion;ComPtr<ID3D11UnorderedAccessView> depthWrite,motionWrite;
    auto output=[&](DXGI_FORMAT format,ComPtr<ID3D11Texture2D>& texture,ComPtr<ID3D11UnorderedAccessView>& uav){desc.Format=format;Need(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&texture)) && SUCCEEDED(device->CreateUnorderedAccessView(texture.Get(),nullptr,&uav)),"Captured guide conversion target");};
    output(DXGI_FORMAT_R32_FLOAT,convertedDepth,depthWrite);output(DXGI_FORMAT_R32G32_FLOAT,convertedMotion,motionWrite);
    unsigned rect[8]={0,0,w,h,0,0,0,0};D3D11_BUFFER_DESC bd{};bd.ByteWidth=sizeof(rect);bd.BindFlags=D3D11_BIND_CONSTANT_BUFFER;D3D11_SUBRESOURCE_DATA data{rect,0,0};ComPtr<ID3D11Buffer> crop;
    Need(SUCCEEDED(device->CreateBuffer(&bd,&data,&crop)),"Captured guide rectangle buffer");
    context->ClearState();ID3D11Buffer* cb=viewBuffer;context->CSSetConstantBuffers(1,1,&cb);cb=crop.Get();context->CSSetConstantBuffers(3,1,&cb);
    ID3D11ShaderResourceView* inputs[]={depthView.Get(),motionView.Get()};ID3D11UnorderedAccessView* outputs[]={depthWrite.Get(),motionWrite.Get()};
    context->CSSetShader(shader,nullptr,0);context->CSSetShaderResources(0,2,inputs);context->CSSetUnorderedAccessViews(0,2,outputs,nullptr);context->Dispatch((ow+7)/8,(oh+7)/8,1);context->ClearState();
    unsigned encoded=0,moving=0;float minDepth=1,maxDepth=0;
    for(unsigned kind=0;kind<2;++kind) {
        auto texture=kind?convertedMotion.Get():convertedDepth.Get();texture->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> copy;Need(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&copy)),"Captured guide GPU readback allocation");context->CopyResource(copy.Get(),texture);
        D3D11_MAPPED_SUBRESOURCE mapped{};Need(SUCCEEDED(context->Map(copy.Get(),0,D3D11_MAP_READ,0,&mapped)),"Captured guide GPU map");bool valid=true;unsigned mismatches=0;
        for(unsigned y=0;y<oh;++y)for(unsigned x=0;x<ow;++x) {
            float u=(x+0.5f)/ow,v=(y+0.5f)/oh;
            int sx=std::clamp(static_cast<int>(floorf((u+view[520]*0.5f)*w)),0,static_cast<int>(w)-1);
            int sy=std::clamp(static_cast<int>(floorf((v-view[521]*0.5f)*h)),0,static_cast<int>(h)-1);size_t index=static_cast<size_t>(sy)*w+sx;
            float z;memcpy(&z,depthBytes.data()+index*8,4);unsigned short packed[4];memcpy(packed,motionBytes.data()+index*8,8);
            auto pixel=reinterpret_cast<const float*>(static_cast<const BYTE*>(mapped.pData)+y*mapped.RowPitch)+x*(kind?2:1);
            if(!std::isfinite(z) || z<0 || z>1 || !std::isfinite(pixel[0])){valid=false;continue;}
            if(!kind){if(fabsf(pixel[0]-z)>0.000001f){valid=false;if(mismatches++==0)fprintf(stderr,"Depth first mismatch (%u,%u), source (%d,%d): GPU %g reference %g\n",x,y,sx,sy,pixel[0],z);}minDepth=std::min(minDepth,z);maxDepth=std::max(maxDepth,z);continue;}
            float ndc[4]={u*2-1,1-v*2,z,1},previous[4]{};
            for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row)previous[col]+=ndc[row]*view[504+row*4+col];
            float dx=0,dy=0;if(fabsf(previous[3])>1e-8f){dx=previous[0]/previous[3]-ndc[0];dy=previous[1]/previous[3]-ndc[1];}
            if(packed[0]){dx=-(packed[0]/65535.0f-32767.0f/65535.0f)/(0.499f*0.5f);dy=-(packed[1]/65535.0f-32767.0f/65535.0f)/(0.499f*0.5f);++encoded;}
            float expected[2]={dx*ow*0.5f,-dy*oh*0.5f};
            if(!std::isfinite(pixel[1]) || fabsf(pixel[0]-expected[0])>0.001f*(1+fabsf(expected[0])) || fabsf(pixel[1]-expected[1])>0.001f*(1+fabsf(expected[1]))){valid=false;if(mismatches++==0)fprintf(stderr,"Motion first mismatch (%u,%u), source (%d,%d): GPU (%g,%g) reference (%g,%g), packed (%u,%u)\n",x,y,sx,sy,pixel[0],pixel[1],expected[0],expected[1],packed[0],packed[1]);}
            if(fabsf(pixel[0])+fabsf(pixel[1])>0.01f)++moving;
        }
        context->Unmap(copy.Get(),0);if(!valid)fprintf(stderr,"Captured guide kind %u: %u mismatches\n",kind,mismatches);Need(valid,"Actual captured GPU depth/motion differs from independent CPU reference or contains invalid values");
    }
    Need(moving>0,"Actual capture contains no temporal motion");
    printf("PASS captured depth/dense-motion GPU replay: %ux%u to %ux%u guide coordinates; %u encoded-motion pixels, %u moving pixels; depth [%g,%g]. Guide conversion only, no SR/NR/FG acceptance.\n",w,h,ow,oh,encoded,moving,minDepth,maxDepth);
}
int main(int argc,char** argv){try{
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Need(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"Guide WARP device");
    ComPtr<ID3DBlob> code,errors;Need(SUCCEEDED(D3DCompile(TwinKartGraphics::NativeGuideShader,sizeof(TwinKartGraphics::NativeGuideShader),nullptr,nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&errors)),"Actual guide conversion shader compile");
    ComPtr<ID3D11ComputeShader> shader;Need(SUCCEEDED(device->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&shader)),"Guide shader creation");
    float depths[32];for(unsigned i=0;i<32;++i)depths[i]=static_cast<float>(i)/32;
    unsigned short velocities[128]{};
    D3D11_TEXTURE2D_DESC desc{};desc.Width=8;desc.Height=4;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;desc.Format=DXGI_FORMAT_R32_FLOAT;
    ComPtr<ID3D11Texture2D> depth,velocity;D3D11_SUBRESOURCE_DATA data{depths,32,0};Need(SUCCEEDED(device->CreateTexture2D(&desc,&data,&depth)),"Native depth fixture");
    desc.Format=DXGI_FORMAT_R16G16B16A16_UNORM;data={velocities,64,0};Need(SUCCEEDED(device->CreateTexture2D(&desc,&data,&velocity)),"Native velocity fixture");
    ComPtr<ID3D11ShaderResourceView> depthView,velocityView;
    Need(SUCCEEDED(device->CreateShaderResourceView(depth.Get(),nullptr,&depthView)) && SUCCEEDED(device->CreateShaderResourceView(velocity.Get(),nullptr,&velocityView)),"Guide input views");
    ComPtr<ID3D11Texture2D> denseCheck;bool denseCameraOnly=true;
    Need(TwinKartGraphics::ReadNativeVelocity(context.Get(),velocityView.Get(),7,3,denseCheck,denseCameraOnly) && !denseCameraOnly,
        "Dense velocity rejected a valid smaller native rectangle");
    Need(!TwinKartGraphics::ReadNativeVelocity(context.Get(),velocityView.Get(),9,3,denseCheck,denseCameraOnly),"Dense velocity accepted pixels beyond its allocation");
    desc.BindFlags=D3D11_BIND_UNORDERED_ACCESS;desc.Format=DXGI_FORMAT_R32_FLOAT;
    ComPtr<ID3D11Texture2D> outDepth,outMotion;Need(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&outDepth)),"Converted depth");
    desc.Format=DXGI_FORMAT_R32G32_FLOAT;Need(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&outMotion)),"Converted motion");
    ComPtr<ID3D11UnorderedAccessView> depthWrite,motionWrite;
    Need(SUCCEEDED(device->CreateUnorderedAccessView(outDepth.Get(),nullptr,&depthWrite)) && SUCCEEDED(device->CreateUnorderedAccessView(outMotion.Get(),nullptr,&motionWrite)),"Converted views");
    float view[2048]{};view[504]=view[509]=view[514]=view[519]=1;view[516]=0.25f;view[520]=0.25f;view[536]=8;view[537]=4;
    D3D11_BUFFER_DESC constants{};constants.ByteWidth=sizeof(view);constants.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    data={view,0,0};ComPtr<ID3D11Buffer> buffer;Need(SUCCEEDED(device->CreateBuffer(&constants,&data,&buffer)),"Native view fixture");
    ID3D11Buffer* bound=buffer.Get();context->CSSetConstantBuffers(1,1,&bound);context->CSSetShader(shader.Get(),nullptr,0);
    unsigned resolvedCrop[8]={0,0,8,4,0,0,8,4};D3D11_BUFFER_DESC cropDesc{};cropDesc.ByteWidth=32;cropDesc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    data={resolvedCrop,0,0};ComPtr<ID3D11Buffer> resolvedRect;
    Need(SUCCEEDED(device->CreateBuffer(&cropDesc,&data,&resolvedRect)),"Resolved native rectangle");
    ID3D11Buffer* resolvedBound=resolvedRect.Get();context->CSSetConstantBuffers(3,1,&resolvedBound);
    ID3D11ShaderResourceView* reads[]{depthView.Get(),velocityView.Get()};ID3D11UnorderedAccessView* writes[]{depthWrite.Get(),motionWrite.Get()};
    context->CSSetShaderResources(0,2,reads);context->CSSetUnorderedAccessViews(0,2,writes,nullptr);
    auto inspect=[&](float expectedMotion,unsigned left=0,unsigned top=0,unsigned logicalWidth=8,unsigned logicalHeight=4,float expectedVertical=0){
        context->Dispatch(1,1,1);
        for(unsigned kind=0;kind<2;++kind){
            auto texture=kind?outMotion.Get():outDepth.Get();texture->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
            ComPtr<ID3D11Texture2D> copy;Need(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&copy)),"Guide readback allocation");context->CopyResource(copy.Get(),texture);
            D3D11_MAPPED_SUBRESOURCE mapped{};Need(SUCCEEDED(context->Map(copy.Get(),0,D3D11_MAP_READ,0,&mapped)),"Guide GPU readback");
            bool correct=true;
            for(int y=0;y<4;++y)for(int x=0;x<8;++x){
                auto pixel=reinterpret_cast<const float*>(static_cast<const BYTE*>(mapped.pData)+y*mapped.RowPitch)+x*(kind?2:1);
                int sx=static_cast<int>(floorf(((x+0.5f)/8+view[520]*0.5f)*logicalWidth));
                int sy=static_cast<int>(floorf(((y+0.5f)/4-view[521]*0.5f)*logicalHeight));
                sx=sx<0?0:sx>=static_cast<int>(logicalWidth)?logicalWidth-1:sx;
                sy=sy<0?0:sy>=static_cast<int>(logicalHeight)?logicalHeight-1:sy;
                float expected=kind?expectedMotion:static_cast<float>((sy+top)*8+sx+left)/32;
                if(!std::isfinite(pixel[0]) || fabsf(pixel[0]-expected)>0.001f || (kind && (!std::isfinite(pixel[1]) || fabsf(pixel[1]-expectedVertical)>0.001f)))correct=false;
            }
            context->Unmap(copy.Get(),0);Need(correct,"Native jitter/camera/object guide conversion mismatch");
        }
    };
    inspect(1);
    // A nonzero vertical camera delta catches uint-height negation before
    // float conversion; zero-Y fixtures cannot detect that unsigned wrap.
    view[517]=0.125f;context->UpdateSubresource(buffer.Get(),0,nullptr,view,0,0);inspect(1,0,0,8,4,-0.25f);
    view[517]=0;context->UpdateSubresource(buffer.Get(),0,nullptr,view,0,0);
    view[520]=-0.25f;context->UpdateSubresource(buffer.Get(),0,nullptr,view,0,0);inspect(1);
    D3D11_TEXTURE2D_DESC tiny{};tiny.Width=tiny.Height=tiny.MipLevels=tiny.ArraySize=tiny.SampleDesc.Count=1;
    tiny.BindFlags=D3D11_BIND_SHADER_RESOURCE;tiny.Format=DXGI_FORMAT_B8G8R8A8_TYPELESS;
    unsigned black=0xff000000;D3D11_SUBRESOURCE_DATA blackData{&black,4,0};
    ComPtr<ID3D11Texture2D> dummy;Need(SUCCEEDED(device->CreateTexture2D(&tiny,&blackData,&dummy)),"Native black velocity sentinel");
    D3D11_SHADER_RESOURCE_VIEW_DESC tinyView{};tinyView.Format=DXGI_FORMAT_B8G8R8A8_UNORM;tinyView.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;tinyView.Texture2D.MipLevels=1;
    ComPtr<ID3D11ShaderResourceView> dummyView;Need(SUCCEEDED(device->CreateShaderResourceView(dummy.Get(),&tinyView,&dummyView)),"Sentinel view");
    ComPtr<ID3D11Texture2D> checked;bool cameraOnly=false;
    Need(TwinKartGraphics::ReadNativeVelocity(context.Get(),dummyView.Get(),8,4,checked,cameraOnly) && cameraOnly,"Verified native zero sentinel rejected");
    ID3D11ShaderResourceView* dummyBound=dummyView.Get();context->CSSetShaderResources(1,1,&dummyBound);inspect(1);
    unsigned nonzero=0xff000001;context->UpdateSubresource(dummy.Get(),0,nullptr,&nonzero,4,0);
    Need(!TwinKartGraphics::ReadNativeVelocity(context.Get(),dummyView.Get(),8,4,checked,cameraOnly) && !cameraOnly,"Mutable non-black sentinel accepted");
    ID3D11ShaderResourceView* regularBound=velocityView.Get();context->CSSetShaderResources(1,1,&regularBound);
    for(unsigned i=0;i<32;++i){velocities[i*4]=static_cast<unsigned short>(lround((32767.0/65535.0+0.2495*0.25)*65535));velocities[i*4+1]=32767;}
    context->UpdateSubresource(velocity.Get(),0,nullptr,velocities,64,0);inspect(-1);
    for(unsigned i=0;i<32;++i)velocities[i*4+1]=static_cast<unsigned short>(lround((32767.0/65535.0+0.2495*0.125)*65535));
    context->UpdateSubresource(velocity.Get(),0,nullptr,velocities,64,0);inspect(-1,0,0,8,4,0.25f);
    for(unsigned i=0;i<32;++i)velocities[i*4+1]=32767;
    context->UpdateSubresource(velocity.Get(),0,nullptr,velocities,64,0);
    // Keep 8x4 allocations/output while the real viewport is an offset 4x2.
    // Distinct two-dimensional depth values expose both axis/crop mistakes.
    resolvedCrop[0]=2;resolvedCrop[1]=1;resolvedCrop[2]=4;resolvedCrop[3]=2;
    context->UpdateSubresource(resolvedRect.Get(),0,nullptr,resolvedCrop,0,0);
    view[536]=4;view[537]=2;view[520]=0.5f;view[521]=-1;
    context->UpdateSubresource(buffer.Get(),0,nullptr,view,0,0);inspect(-1,2,1,4,2);
    memset(velocities,0,sizeof(velocities));context->UpdateSubresource(velocity.Get(),0,nullptr,velocities,64,0);
    black=0xff000000;context->UpdateSubresource(dummy.Get(),0,nullptr,&black,4,0);
    context->CSSetShaderResources(1,1,&dummyBound);inspect(1,2,1,4,2);
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
    unsigned crop[8]={1,1,6,2,0,0,6,2};constants.ByteWidth=32;data={crop,0,0};ComPtr<ID3D11Buffer> rect;
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
            float expected=kind==0?((y+1)*8+x+1)/32.0f:kind==1?0.75f:static_cast<float>((y+1)*8+x+1);
            if(fabsf(pixel[0]-expected)>0.001f || (kind==1 && fabsf(pixel[1])>0.001f))correct=false;
        }
        context->Unmap(copy.Get(),0);Need(correct,"Raw cropped guides/color shifted or camera motion wrong");
    }
    context->ClearState();
    // Resolved SR color can use a different observed crop from depth/velocity.
    // Copy the exact source pixels, without shrinking an allocated image.
    crop[4]=2;crop[5]=1;crop[6]=6;crop[7]=2;context->UpdateSubresource(rect.Get(),0,nullptr,crop,0,0);
    view[520]=view[521]=0;context->UpdateSubresource(buffer.Get(),0,nullptr,view,0,0);
    context->CSSetConstantBuffers(1,1,&viewBuffer);context->CSSetConstantBuffers(3,1,&rectBuffer);
    context->CSSetShader(shader.Get(),nullptr,0);context->CSSetShaderResources(0,3,rawReads);context->CSSetUnorderedAccessViews(0,3,rawWrites,nullptr);
    context->Dispatch(1,1,1);
    rawColor->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> resolvedCopy;Need(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&resolvedCopy)),"Resolved color crop readback");
    context->CopyResource(resolvedCopy.Get(),rawColor.Get());D3D11_MAPPED_SUBRESOURCE resolvedMap{};
    Need(SUCCEEDED(context->Map(resolvedCopy.Get(),0,D3D11_MAP_READ,0,&resolvedMap)),"Resolved color crop map");bool exact=true;
    for(unsigned y=0;y<2;++y)for(unsigned x=0;x<6;++x) {
        auto pixel=reinterpret_cast<const float*>(static_cast<const BYTE*>(resolvedMap.pData)+y*resolvedMap.RowPitch)+x*4;
        if(pixel[0]!=static_cast<float>((y+1)*8+x+2) || pixel[3]!=1)exact=false;
    }
    context->Unmap(resolvedCopy.Get(),0);Need(exact,"Resolved padded scene resized or used the guide origin for color");
    context->ClearState();
    // Exercise the production snapshot on real GPU buffers. Native allocation
    // padding and a later view update must not alter the captured guide frame.
    float parameters[1024]{};TwinKartGraphics::NativeRect input{16,8,144,136},output{4,2,132,130};
    TwinKartGraphics::NativeRect inclusive=input;--inclusive.right;--inclusive.bottom;memcpy(parameters+80,&inclusive,sizeof(inclusive));
    parameters[96]=static_cast<float>(output.left);parameters[97]=static_cast<float>(output.top);parameters[98]=static_cast<float>(output.right);parameters[99]=static_cast<float>(output.bottom);
    parameters[60]=128.0f/192;parameters[61]=128.0f/160;parameters[62]=16.0f/192;parameters[63]=8.0f/160;
    parameters[76]=192;parameters[77]=160;parameters[78]=1.0f/192;parameters[79]=1.0f/160;
    parameters[92]=parameters[93]=128;parameters[94]=parameters[95]=1.0f/128;
    view[536]=view[537]=128;view[538]=view[539]=1.0f/128;view[520]=view[521]=0;view[557]=2;view[558]=0.5f;
    context->UpdateSubresource(buffer.Get(),0,nullptr,view,0,0);
    constants.ByteWidth=sizeof(parameters);data={parameters,0,0};ComPtr<ID3D11Buffer> parameterBuffer,captured;
    Need(SUCCEEDED(device->CreateBuffer(&constants,&data,&parameterBuffer)),"Native parameter fixture");
    TwinKartGraphics::NativeTemporalContract layout;
    Need(TwinKartGraphics::SnapshotNativeGuides(context.Get(),parameterBuffer.Get(),buffer.Get(),192,160,160,144,layout,captured) &&
        layout.input.left==16 && layout.input.Width()==128 && layout.output.top==2,"Production padded 1:1 guide capture");
    view[536]=192;context->UpdateSubresource(buffer.Get(),0,nullptr,view,0,0);
    float copied[2048]{};
    Need(TwinKartGraphics::ReadNativeGuideBuffer(context.Get(),captured.Get(),copied,sizeof(copied),8192) && copied[536]==128 && copied[557]==2,
        "Captured guide View mutated with original buffer");
    auto previous=captured.Get();
    Need(!TwinKartGraphics::SnapshotNativeGuides(context.Get(),parameterBuffer.Get(),buffer.Get(),192,160,160,144,layout,captured) && captured.Get()==previous,
        "Invalid viewport accepted or previous immutable snapshot replaced");
    if(argc==3 || argc==5) {
        auto read=[](const char* path,void* bytes,size_t count){std::ifstream file(path,std::ios::binary);file.read(static_cast<char*>(bytes),count);return file.gcount()==static_cast<std::streamsize>(count) && file.peek()==std::char_traits<char>::eof();};
        Need(read(argv[1],parameters,sizeof(parameters)) && read(argv[2],view,sizeof(view)),"Expected exact private native constant captures");
        Need(TwinKartGraphics::DecodeNativeTemporalLayout(parameters,sizeof(parameters),view,8192,8192,8192,8192,layout),"Invalid private capture live registers");
        unsigned nativeWidth=static_cast<unsigned>(parameters[76]),nativeHeight=static_cast<unsigned>(parameters[77]);auto nativeOutput=layout.output;
        context->UpdateSubresource(parameterBuffer.Get(),0,nullptr,parameters,0,0);context->UpdateSubresource(buffer.Get(),0,nullptr,view,0,0);
        unsigned failure=99;TwinKartGraphics::NativeGuideSnapshotFacts facts;
        Need(TwinKartGraphics::SnapshotNativeGuides(context.Get(),parameterBuffer.Get(),buffer.Get(),nativeWidth,nativeHeight,nativeOutput.right,nativeOutput.bottom,layout,captured,&failure,&facts) && failure==0,"Production GPU snapshot rejected actual captured constants");
        Need(!memcmp(facts.parameters,parameters,sizeof(facts.parameters)) && facts.viewExtent[0]==view[536] && facts.exposure[0]==view[557],"Snapshot rejection facts differ from exact GPU inputs");
        Need(TwinKartGraphics::ReadNativeGuideBuffer(context.Get(),captured.Get(),copied,sizeof(copied),8192) && !memcmp(copied,view,sizeof(view)),"Actual native capture GPU snapshot changed values");
        puts("PASS private native capture replay through production GPU snapshot and rejection facts; no live game/quality acceptance.");
        if(argc==5)ReplayCapturedGuides(device.Get(),context.Get(),shader.Get(),captured.Get(),view,layout,argv[3],argv[4]);
    } else Need(argc==1,"Expected paired native parameter/view captures, optionally followed by depth/velocity captures");
    puts("PASS actual WARP guide shaders: independent offset color/depth crops without image resizing, jitter clamps, camera/object motion, raw HDR crop and immutable same-frame snapshot. No game or NVIDIA runtime started.");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
