#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <bcrypt.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include "RenderPassTrace.h"

namespace TwinKartRender {
namespace {
const GUID ShaderDigest = {0x95df261a,0x7bbc,0x4197,{0xa2,0x20,0x42,0x44,0x71,0x96,0x52,0xac}};
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
    return ok && SUCCEEDED(shader->SetPrivateData(ShaderDigest, sizeof(digest), &digest));
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
