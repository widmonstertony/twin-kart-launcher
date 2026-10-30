#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <d3dcompiler.h>
#include <d3d11shader.h>
#include <string>
#include "RenderPassTrace.h"

namespace TwinKartRender {
namespace {
const GUID ShaderDigest = {0x95df261a,0x7bbc,0x4197,{0xa2,0x20,0x42,0x44,0x71,0x96,0x52,0xac}};
const GUID ShaderInterface = {0x813a4b4d,0xaae6,0x46c3,{0xa6,0x62,0x99,0xfa,0xe1,0xc8,0x55,0x4d}};
const GUID ThreadGroup = {0xf4573c63,0xc815,0x4998,{0x96,0x76,0x15,0x9a,0xdb,0xc0,0x75,0xcb}};
struct Digest { unsigned version; unsigned long long bytes; BYTE hash[32]; };
struct Writer {
    char* output; size_t capacity, used; bool failed;
    void Add(const char* format, ...) {
        if (failed) return;
        va_list args; va_start(args, format);
        int count = vsnprintf_s(output + used, capacity - used, _TRUNCATE, format, args);
        va_end(args);
        if (count < 0) { failed = true; return; }
        used += static_cast<size_t>(count);
    }
};
bool Identifier(const char* name) {
    if(!name || !name[0] || strlen(name)>128) return false;
    for(const char* ch=name;*ch;++ch) if(!((*ch>='A' && *ch<='Z') || (*ch>='a' && *ch<='z') || (*ch>='0' && *ch<='9') || *ch=='_' || *ch=='.' || *ch=='[' || *ch==']')) return false;
    return true;
}
bool CameraField(const char* name) {
    if(!Identifier(name)) return false;
    const char* fields[]={"ViewSizeAndInvSize","BufferSizeAndInvSize","ViewRectMin","TemporalAAJitter","ClipToPrevClip","ViewToClip","ClipToView","TranslatedWorldToClip","PrevViewProj","ViewForward","ViewRight","ViewUp","WorldCameraOrigin","PreExposure"};
    for(const char* field:fields) if(strstr(name,field)) return true;
    return false;
}
void DescribeMembers(Writer& writer,ID3D11ShaderReflectionType* type,const std::string& prefix,UINT base,unsigned depth,bool& comma,unsigned& fields) {
    if(!type || depth>3 || fields>=48) return;
    D3D11_SHADER_TYPE_DESC desc{}; if(FAILED(type->GetDesc(&desc))) return;
    for(UINT member=0;member<desc.Members && member<512 && fields<48;++member) {
        const char* name=type->GetMemberTypeName(member); if(!Identifier(name)) continue;
        auto child=type->GetMemberTypeByIndex(member); D3D11_SHADER_TYPE_DESC item{}; if(!child || FAILED(child->GetDesc(&item))) continue;
        std::string full=prefix+"."+name;
        if(CameraField(name)) {
            writer.Add("%s{\"name\":\"%s\",\"offset\":%u,\"rows\":%u,\"columns\":%u,\"class\":%u}",comma?",":"",full.c_str(),base+item.Offset,item.Rows,item.Columns,item.Class);
            comma=true; ++fields;
        }
        if(item.Members) DescribeMembers(writer,child,full,base+item.Offset,depth+1,comma,fields);
    }
}
bool TagInterface(ID3D11DeviceChild* shader,const void* bytecode,size_t length) {
    ID3D11ShaderReflection* reflection=nullptr;
    if(FAILED(D3DReflect(bytecode,length,__uuidof(ID3D11ShaderReflection),reinterpret_cast<void**>(&reflection)))) return false;
    D3D11_SHADER_DESC desc{}; bool success=SUCCEEDED(reflection->GetDesc(&desc));
    if(success && D3D11_SHVER_GET_TYPE(desc.Version)==D3D11_SHVER_COMPUTE_SHADER) {
        unsigned group[3]{};
        if(reflection->GetThreadGroupSize(group,group+1,group+2) && group[0] && group[1] && group[2])
            shader->SetPrivateData(ThreadGroup,sizeof(group),group);
    }
    char text[16384]{}; Writer writer{text,sizeof(text),0,false};
    if(success) {
        writer.Add("{\"bindings\":["); bool comma=false;
        for(UINT index=0;index<desc.BoundResources && index<64;++index) {
            D3D11_SHADER_INPUT_BIND_DESC binding{};
            if(FAILED(reflection->GetResourceBindingDesc(index,&binding)) || !Identifier(binding.Name)) continue;
            writer.Add("%s{\"name\":\"%s\",\"slot\":%u,\"count\":%u,\"type\":%u,\"dimension\":%u}",comma?",":"",binding.Name,binding.BindPoint,binding.BindCount,binding.Type,binding.Dimension); comma=true;
        }
        writer.Add("],\"camera_layout\":["); comma=false; unsigned fields=0;
        for(UINT index=0;index<desc.ConstantBuffers && index<32;++index) {
            auto buffer=reflection->GetConstantBufferByIndex(index); D3D11_SHADER_BUFFER_DESC info{};
            if(!buffer || FAILED(buffer->GetDesc(&info)) || !Identifier(info.Name)) continue;
            for(UINT variable=0;variable<info.Variables && variable<512 && fields<48;++variable) {
                auto field=buffer->GetVariableByIndex(variable); D3D11_SHADER_VARIABLE_DESC value{};
                if(!field || FAILED(field->GetDesc(&value)) || !Identifier(value.Name)) continue;
                auto type=field->GetType(); D3D11_SHADER_TYPE_DESC shape{}; if(!type || FAILED(type->GetDesc(&shape))) continue;
                std::string full=std::string(info.Name)+"."+value.Name;
                if(CameraField(value.Name)) {
                    writer.Add("%s{\"name\":\"%s\",\"offset\":%u,\"size\":%u,\"rows\":%u,\"columns\":%u,\"class\":%u}",comma?",":"",full.c_str(),value.StartOffset,value.Size,shape.Rows,shape.Columns,shape.Class); comma=true; ++fields;
                }
                if(shape.Members) DescribeMembers(writer,type,full,value.StartOffset,0,comma,fields);
            }
        }
        writer.Add("],\"guides_validated\":false}");
    }
    reflection->Release();
    return success && !writer.failed && SUCCEEDED(shader->SetPrivateData(ShaderInterface,static_cast<UINT>(writer.used+1),text));
}
template<class T> void Drop(T*& value) { if (value) { value->Release(); value = nullptr; } }
struct Binding {
    ID3D11Resource* resource = nullptr;
    D3D11_TEXTURE2D_DESC texture{};
    UINT format = 0, mip = 0, slice = 0, dimension = 0;
    bool scene = false;
};
template<class T> Binding Texture(T* view) {
    Binding result;
    if (!view) return result;
    view->GetResource(&result.resource);
    ID3D11Texture2D* texture = nullptr;
    if (SUCCEEDED(result.resource->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&texture)))) {
        texture->GetDesc(&result.texture); texture->Release();
        result.scene = result.texture.Width >= 128 && result.texture.Height >= 128;
    }
    return result;
}
Binding Read(ID3D11ShaderResourceView* view) {
    Binding result = Texture(view);
    if (!view) return result;
    D3D11_SHADER_RESOURCE_VIEW_DESC desc{}; view->GetDesc(&desc);
    result.format = desc.Format; result.dimension = desc.ViewDimension;
    if (desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2D) result.mip = desc.Texture2D.MostDetailedMip;
    if (desc.ViewDimension == D3D11_SRV_DIMENSION_TEXTURE2DARRAY) {
        result.mip = desc.Texture2DArray.MostDetailedMip; result.slice = desc.Texture2DArray.FirstArraySlice;
    }
    return result;
}
Binding Read(ID3D11RenderTargetView* view) {
    Binding result = Texture(view);
    if (!view) return result;
    D3D11_RENDER_TARGET_VIEW_DESC desc{}; view->GetDesc(&desc);
    result.format = desc.Format; result.dimension = desc.ViewDimension;
    if (desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D) result.mip = desc.Texture2D.MipSlice;
    if (desc.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2DARRAY) {
        result.mip = desc.Texture2DArray.MipSlice; result.slice = desc.Texture2DArray.FirstArraySlice;
    }
    return result;
}
Binding Read(ID3D11UnorderedAccessView* view) {
    Binding result = Texture(view);
    if (!view) return result;
    D3D11_UNORDERED_ACCESS_VIEW_DESC desc{}; view->GetDesc(&desc);
    result.format = desc.Format; result.dimension = desc.ViewDimension;
    if (desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2D) result.mip = desc.Texture2D.MipSlice;
    if (desc.ViewDimension == D3D11_UAV_DIMENSION_TEXTURE2DARRAY) {
        result.mip = desc.Texture2DArray.MipSlice; result.slice = desc.Texture2DArray.FirstArraySlice;
    }
    return result;
}
Binding Read(ID3D11DepthStencilView* view) {
    Binding result = Texture(view);
    if (!view) return result;
    D3D11_DEPTH_STENCIL_VIEW_DESC desc{}; view->GetDesc(&desc);
    result.format = desc.Format; result.dimension = desc.ViewDimension;
    if (desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2D) result.mip = desc.Texture2D.MipSlice;
    if (desc.ViewDimension == D3D11_DSV_DIMENSION_TEXTURE2DARRAY) {
        result.mip = desc.Texture2DArray.MipSlice; result.slice = desc.Texture2DArray.FirstArraySlice;
    }
    return result;
}
void Write(Writer& writer, Binding& value, unsigned slot) {
    writer.Add("{\"slot\":%u,\"resource\":\"%p\",\"width\":%u,\"height\":%u,\"resource_format\":%u,\"view_format\":%u,\"view_dimension\":%u,\"mip\":%u,\"first_slice\":%u,\"samples\":%u}",
        slot, value.resource, value.texture.Width, value.texture.Height, value.texture.Format,
        value.format, value.dimension, value.mip, value.slice, value.texture.SampleDesc.Count);
}
template<class T, size_t N> bool Inspect(T* (&views)[N], Binding (&bindings)[N]) {
    bool scene = false;
    for (unsigned i = 0; i < N; ++i) { bindings[i] = Read(views[i]); scene |= bindings[i].scene; }
    return scene;
}
template<class T, size_t N> void WriteViews(Writer& writer, const char* label, T* (&views)[N], Binding (&bindings)[N]) {
    writer.Add(",\"%s\":[", label); bool comma = false;
    for (unsigned i = 0; i < N; ++i) {
        // Include small/non-texture bindings too when a scene output is bound;
        // a LUT or buffer must not be silently described as a missing binding.
        if (views[i]) { if (comma) writer.Add(","); Write(writer, bindings[i], i); comma = true; }
    }
    writer.Add("]");
}
template<class T, size_t N> void ReleaseViews(T* (&views)[N], Binding (&bindings)[N]) {
    for (unsigned i = 0; i < N; ++i) { Drop(bindings[i].resource); Drop(views[i]); }
}
// Explicit local research only. Shipping shaders can have stripped reflection;
// their DXBC register use is needed before treating loose-buffer gaps as live
// parameters. Save only the exact requested hash, once, outside source packages.
void CaptureSelectedShaderLayout(const void* bytecode,size_t length,const Digest& digest) {
    wchar_t directory[32768]{},requested[65]{},hex[65]{};
    DWORD size=GetEnvironmentVariableW(L"TWIN_KART_SHADER_LAYOUT_DIRECTORY",directory,ARRAYSIZE(directory));
    if(!size || size>32000 || GetEnvironmentVariableW(L"TWIN_KART_SHADER_LAYOUT_HASH",requested,ARRAYSIZE(requested))!=64)return;
    for(unsigned i=0;i<32;++i)swprintf_s(hex+i*2,65-i*2,L"%02x",digest.hash[i]);
    if(wcscmp(requested,hex))return;
    DWORD attributes=GetFileAttributesW(directory);
    if(attributes==INVALID_FILE_ATTRIBUTES || !(attributes&FILE_ATTRIBUTE_DIRECTORY) || (attributes&FILE_ATTRIBUTE_REPARSE_POINT))return;
    wchar_t path[32768];if(swprintf_s(path,L"%s\\layout-%s.txt",directory,hex)<0)return;
    if(GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES)return;
    ID3DBlob* assembly=nullptr;
    if(FAILED(D3DDisassemble(bytecode,length,0,nullptr,&assembly)) || !assembly)return;
    if(assembly->GetBufferSize()<=2*1024*1024) {
        HANDLE file=CreateFileW(path,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(file!=INVALID_HANDLE_VALUE){DWORD written=0;WriteFile(file,assembly->GetBufferPointer(),static_cast<DWORD>(assembly->GetBufferSize()),&written,nullptr);CloseHandle(file);}
    }
    assembly->Release();
}
}
bool TagShader(ID3D11DeviceChild* shader, const void* bytecode, size_t length) {
    if (!shader || !bytecode || !length || length > 16 * 1024 * 1024) return false;
    BCRYPT_ALG_HANDLE algorithm = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    Digest digest{}; digest.version = 1; digest.bytes = length;
    bool ok = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0 &&
        BCryptCreateHash(algorithm, &hash, nullptr, 0, nullptr, 0, 0) >= 0 &&
        BCryptHashData(hash, static_cast<PUCHAR>(const_cast<void*>(bytecode)), static_cast<ULONG>(length), 0) >= 0 &&
        BCryptFinishHash(hash, digest.hash, sizeof(digest.hash), 0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm, 0);
    bool stored=ok && SUCCEEDED(shader->SetPrivateData(ShaderDigest, sizeof(digest), &digest));
    // Bytecode is never retained. Optional exact-hash disassembly is private
    // research evidence, excluded from published source/resources.
    if(stored) TagInterface(shader,bytecode,length);
    if(stored) CaptureSelectedShaderLayout(bytecode,length,digest);
    return stored;
}
bool ShaderHash(ID3D11DeviceChild* shader,char (&hex)[65]) {
    hex[0]=0; Digest value{}; UINT length=sizeof(value);
    if(!shader || FAILED(shader->GetPrivateData(ShaderDigest,&length,&value)) || length!=sizeof(value) || value.version!=1) return false;
    for(unsigned i=0;i<32;++i) sprintf_s(hex+i*2,65-i*2,"%02x",value.hash[i]);
    return true;
}
bool ShaderThreadGroup(ID3D11ComputeShader* shader,unsigned& x,unsigned& y,unsigned& z) {
    unsigned group[3]{};UINT length=sizeof(group);
    if(!shader || FAILED(shader->GetPrivateData(ThreadGroup,&length,group)) || length!=sizeof(group) ||
       !group[0] || !group[1] || !group[2] || group[0]>1024 || group[1]>1024 || group[2]>64 ||
       static_cast<unsigned long long>(group[0])*group[1]*group[2]>1024)return false;
    x=group[0];y=group[1];z=group[2];return true;
}
size_t DescribePass(ID3D11DeviceContext* context, bool compute, unsigned frame,
    uintptr_t callerRva, char* output, size_t capacity) {
    if (!context || !output || !capacity || context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE) return 0;
    output[0] = 0;
    ID3D11ShaderResourceView* resources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT]{};
    ID3D11RenderTargetView* targets[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT]{};
    ID3D11UnorderedAccessView* unordered[D3D11_PS_CS_UAV_REGISTER_COUNT]{};
    ID3D11DepthStencilView* depth = nullptr;
    ID3D11PixelShader* pixel = nullptr; ID3D11ComputeShader* shader = nullptr;
    ID3D11Buffer* constants[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT]{};
    if (compute) {
        context->CSGetShaderResources(0, ARRAYSIZE(resources), resources);
        context->CSGetUnorderedAccessViews(0, ARRAYSIZE(unordered), unordered);
        context->CSGetShader(&shader, nullptr, nullptr);
        context->CSGetConstantBuffers(0, ARRAYSIZE(constants), constants);
    } else {
        context->PSGetShaderResources(0, ARRAYSIZE(resources), resources);
        context->OMGetRenderTargets(ARRAYSIZE(targets), targets, &depth);
        context->PSGetShader(&pixel, nullptr, nullptr);
        context->PSGetConstantBuffers(0, ARRAYSIZE(constants), constants);
    }
    Binding inputs[ARRAYSIZE(resources)]{}, outputs[ARRAYSIZE(targets)]{}, writes[ARRAYSIZE(unordered)]{};
    Inspect(resources, inputs);
    bool scene = Inspect(targets, outputs) | Inspect(unordered, writes);
    Binding depthBinding = Read(depth);
    Writer writer{output, capacity, 0, false};
    if (scene) {
        writer.Add("{\"event\":\"render_pass_observed\",\"frame\":%u,\"thread\":%lu,\"stage\":\"%s\",\"caller_rva\":\"%llx\",\"shader\":\"%p\",\"shader_sha256\":",
            frame, GetCurrentThreadId(), compute ? "compute" : "pixel", static_cast<unsigned long long>(callerRva),
            compute ? static_cast<ID3D11DeviceChild*>(shader) : static_cast<ID3D11DeviceChild*>(pixel));
        Digest digest{}; UINT length = sizeof(digest);
        ID3D11DeviceChild* selected = compute ? static_cast<ID3D11DeviceChild*>(shader) : static_cast<ID3D11DeviceChild*>(pixel);
        if (selected && SUCCEEDED(selected->GetPrivateData(ShaderDigest, &length, &digest)) && length == sizeof(digest) && digest.version == 1) {
            writer.Add("\""); for (BYTE byte : digest.hash) writer.Add("%02x", byte); writer.Add("\"");
        } else writer.Add("null");
        char interfaceText[16384]{}; UINT interfaceLength=sizeof(interfaceText);
        if(selected && SUCCEEDED(selected->GetPrivateData(ShaderInterface,&interfaceLength,interfaceText)) && interfaceLength>1 && interfaceLength<=sizeof(interfaceText) && interfaceText[interfaceLength-1]==0)
            writer.Add(",\"shader_interface\":%s",interfaceText);
        else writer.Add(",\"shader_interface\":null");
        WriteViews(writer, "srv", resources, inputs);
        WriteViews(writer, "rtv", targets, outputs);
        WriteViews(writer, "uav", unordered, writes);
        writer.Add(",\"dsv\":"); if (depth) Write(writer, depthBinding, 0); else writer.Add("null");
        writer.Add(",\"constant_buffers\":["); bool comma = false;
        for (unsigned i = 0; i < ARRAYSIZE(constants); ++i) if (constants[i]) {
            D3D11_BUFFER_DESC desc{}; constants[i]->GetDesc(&desc);
            writer.Add("%s{\"slot\":%u,\"bytes\":%u}", comma ? "," : "", i, desc.ByteWidth); comma = true;
        }
        writer.Add("],\"guides_validated\":false,\"hudless_validated\":false}\r\n");
    }
    ReleaseViews(resources, inputs); ReleaseViews(targets, outputs); ReleaseViews(unordered, writes);
    Drop(depthBinding.resource); Drop(depth); Drop(pixel); Drop(shader);
    for (auto& buffer : constants) Drop(buffer);
    if (!scene || writer.failed) { output[0] = 0; return 0; }
    return writer.used;
}
}
