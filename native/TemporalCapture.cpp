#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstring>
#include <string>
#include "TemporalCapture.h"
#include "RenderPassTrace.h"
using Microsoft::WRL::ComPtr;
namespace TwinKartGraphics {
namespace {
std::wstring directory;
char selected[65]{};
unsigned captured=0,limit=0,lastFrame=0;
bool waiting=false;
unsigned long long savedBytes=0;
constexpr unsigned long long MaxBytes=768ull*1024*1024;
unsigned PixelBytes(DXGI_FORMAT format) {
    switch(format) {
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: case DXGI_FORMAT_R16G16B16A16_FLOAT:
    case DXGI_FORMAT_R16G16B16A16_UNORM: case DXGI_FORMAT_R32G8X24_TYPELESS:
    case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS: case DXGI_FORMAT_R32G32_FLOAT:return 8;
    case DXGI_FORMAT_R11G11B10_FLOAT: case DXGI_FORMAT_R32_TYPELESS:
    case DXGI_FORMAT_R32_FLOAT: case DXGI_FORMAT_R24G8_TYPELESS:
    case DXGI_FORMAT_R16G16_FLOAT: case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_R10G10B10A2_UNORM:return 4;
    default:return 0;
    }
}
HANDLE File(unsigned frame,const wchar_t* kind,unsigned slot,const wchar_t* extension) {
    wchar_t path[32768];
    if(swprintf_s(path,L"%s\\frame-%u-%s-%u.%s",directory.c_str(),frame,kind,slot,extension)<0) return INVALID_HANDLE_VALUE;
    return CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
}
bool Write(HANDLE file,const void* bytes,unsigned count) {
    DWORD written=0; return WriteFile(file,bytes,count,&written,nullptr) && written==count;
}
bool SaveTexture(ID3D11DeviceContext* context,ID3D11Device* device,ID3D11ShaderResourceView* view,unsigned frame,unsigned slot,std::string& metadata) {
    if(!view) return false;
    D3D11_SHADER_RESOURCE_VIEW_DESC binding{}; view->GetDesc(&binding);
    if(binding.ViewDimension!=D3D11_SRV_DIMENSION_TEXTURE2D || binding.Texture2D.MostDetailedMip) return false;
    ComPtr<ID3D11Resource> resource; view->GetResource(&resource);
    ComPtr<ID3D11Texture2D> source; if(FAILED(resource.As(&source))) return false;
    D3D11_TEXTURE2D_DESC desc{}; source->GetDesc(&desc);
    unsigned bytes=PixelBytes(desc.Format);
    unsigned long long count=static_cast<unsigned long long>(desc.Width)*desc.Height*bytes;
    if(!bytes || !desc.Width || !desc.Height || desc.Width>8192 || desc.Height>8192 ||
       desc.ArraySize!=1 || desc.SampleDesc.Count!=1 || count>128ull*1024*1024 || savedBytes+count>MaxBytes) return false;
    D3D11_TEXTURE2D_DESC staging=desc; staging.MipLevels=1; staging.BindFlags=0;
    staging.MiscFlags=0; staging.Usage=D3D11_USAGE_STAGING; staging.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> copy; if(FAILED(device->CreateTexture2D(&staging,nullptr,&copy))) return false;
    context->CopySubresourceRegion(copy.Get(),0,0,0,0,source.Get(),0,nullptr);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if(FAILED(context->Map(copy.Get(),0,D3D11_MAP_READ,0,&mapped))) return false;
    HANDLE file=File(frame,L"srv",slot,L"bin"); bool ok=file!=INVALID_HANDLE_VALUE;
    if(ok) for(unsigned y=0;y<desc.Height && ok;++y) ok=Write(file,static_cast<const BYTE*>(mapped.pData)+static_cast<size_t>(y)*mapped.RowPitch,desc.Width*bytes);
    context->Unmap(copy.Get(),0); if(file!=INVALID_HANDLE_VALUE) CloseHandle(file);
    if(!ok) return false;
    savedBytes+=count;
    char text[384]; sprintf_s(text,"%s{\"slot\":%u,\"width\":%u,\"height\":%u,\"resource_format\":%u,\"view_format\":%u,\"pixel_bytes\":%u,\"bytes\":%llu}",metadata.empty()?"":",",slot,desc.Width,desc.Height,desc.Format,binding.Format,bytes,count);
    metadata+=text; return true;
}
bool SaveConstants(ID3D11DeviceContext* context,ID3D11Device* device,ID3D11Buffer* source,unsigned frame,unsigned slot,std::string& metadata) {
    if(!source) return false;
    D3D11_BUFFER_DESC desc{}; source->GetDesc(&desc);
    if(!desc.ByteWidth || desc.ByteWidth>65536 || savedBytes+desc.ByteWidth>MaxBytes) return false;
    D3D11_BUFFER_DESC staging=desc; staging.BindFlags=0; staging.MiscFlags=0;
    staging.Usage=D3D11_USAGE_STAGING; staging.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Buffer> copy; if(FAILED(device->CreateBuffer(&staging,nullptr,&copy))) return false;
    context->CopyResource(copy.Get(),source);
    D3D11_MAPPED_SUBRESOURCE mapped{}; if(FAILED(context->Map(copy.Get(),0,D3D11_MAP_READ,0,&mapped))) return false;
    HANDLE file=File(frame,L"cb",slot,L"bin"); bool ok=file!=INVALID_HANDLE_VALUE && Write(file,mapped.pData,desc.ByteWidth);
    context->Unmap(copy.Get(),0); if(file!=INVALID_HANDLE_VALUE) CloseHandle(file);
    if(!ok) return false;
    savedBytes+=desc.ByteWidth;
    char text[96]; sprintf_s(text,"%s{\"slot\":%u,\"bytes\":%u}",metadata.empty()?"":",",slot,desc.ByteWidth); metadata+=text; return true;
}
}
bool ConfigureTemporalCapture(const wchar_t* path,const char* hash,unsigned maximumFrames,bool waitForRequest) {
    if(!path || !*path || wcslen(path)>32000 || !hash || strlen(hash)!=64 || !maximumFrames || maximumFrames>4 || limit) return false;
    for(unsigned i=0;i<64;++i) if(!((hash[i]>='0' && hash[i]<='9') || (hash[i]>='a' && hash[i]<='f'))) return false;
    DWORD attributes=GetFileAttributesW(path);
    if(attributes==INVALID_FILE_ATTRIBUTES || !(attributes&FILE_ATTRIBUTE_DIRECTORY) || (attributes&FILE_ATTRIBUTE_REPARSE_POINT)) return false;
    directory=path; strcpy_s(selected,hash); limit=maximumFrames; waiting=waitForRequest; return true;
}
bool CaptureTemporalInputs(ID3D11DeviceContext* context,unsigned frame) {
    if(!limit || captured>=limit || !frame || frame==lastFrame || frame%120 || !context || context->GetType()!=D3D11_DEVICE_CONTEXT_IMMEDIATE) return false;
    if(waiting) {
        DWORD attributes=GetFileAttributesW((directory+L"\\capture.request").c_str());
        if(attributes==INVALID_FILE_ATTRIBUTES || (attributes&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT))) return false;
        waiting=false;
    }
    ComPtr<ID3D11ComputeShader> shader; context->CSGetShader(&shader,nullptr,nullptr);
    char hash[65]; if(!TwinKartRender::ShaderHash(shader.Get(),hash) || strcmp(hash,selected)) return false;
    lastFrame=frame; ++captured; // Bound failed attempts too.
    ComPtr<ID3D11Device> device; context->GetDevice(&device);
    std::string textures,constants;
    for(unsigned slot=0;slot<7;++slot) {
        ComPtr<ID3D11ShaderResourceView> view; context->CSGetShaderResources(slot,1,&view);
        SaveTexture(context,device.Get(),view.Get(),frame,slot,textures);
    }
    for(unsigned slot=0;slot<3;++slot) {
        ComPtr<ID3D11Buffer> buffer; context->CSGetConstantBuffers(slot,1,&buffer);
        SaveConstants(context,device.Get(),buffer.Get(),frame,slot,constants);
    }
    char header[384]; sprintf_s(header,"{\"event\":\"real_temporal_input_capture\",\"frame\":%u,\"shader_sha256\":\"%s\",\"thread\":%lu,\"guides_validated\":false,\"final_composition\":false,\"srv\":[",frame,hash,GetCurrentThreadId());
    std::string json=header+textures+"],\"constant_buffers\":["+constants+"]}\n";
    HANDLE file=File(frame,L"capture",0,L"json"); bool ok=file!=INVALID_HANDLE_VALUE && Write(file,json.data(),static_cast<unsigned>(json.size()));
    if(file!=INVALID_HANDLE_VALUE) CloseHandle(file); return ok;
}
}
