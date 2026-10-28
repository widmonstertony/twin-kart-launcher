#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <wrl/client.h>
#include "../native/DriftRenderProbe.cpp"
using Microsoft::WRL::ComPtr;
void Need(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void Check(HRESULT result) { Need(SUCCEEDED(result), "D3D11 fixture creation failed"); }
ULONG References(IUnknown* object) { object->AddRef(); return object->Release(); }
int main() {
    try {
        ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
        Check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
        Need(HookDevice(device.Get()) && HookContext(context.Get()),"Real WARP vtable hook installation failed");
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width=512; desc.Height=256; desc.ArraySize=1; desc.MipLevels=2;
        desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count=1;
        desc.BindFlags=D3D11_BIND_RENDER_TARGET | D3D11_BIND_UNORDERED_ACCESS;
        ComPtr<ID3D11Texture2D> target; Check(device->CreateTexture2D(&desc,nullptr,&target));
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
        rtvDesc.Format=desc.Format; rtvDesc.ViewDimension=D3D11_RTV_DIMENSION_TEXTURE2D; rtvDesc.Texture2D.MipSlice=1;
        ComPtr<ID3D11RenderTargetView> rtv; Check(device->CreateRenderTargetView(target.Get(),&rtvDesc,&rtv));
        ComPtr<ID3D11UnorderedAccessView> uav; Check(device->CreateUnorderedAccessView(target.Get(),nullptr,&uav));
        desc.MipLevels=1; desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        ComPtr<ID3D11Texture2D> input; Check(device->CreateTexture2D(&desc,nullptr,&input));
        ComPtr<ID3D11ShaderResourceView> srv; Check(device->CreateShaderResourceView(input.Get(),nullptr,&srv));
        const char pixelSource[]="float4 main():SV_Target { return float4(1,0,0,1); }";
        const char vertexSource[]="float4 main(uint id:SV_VertexID):SV_Position { float2 p[3]={float2(-1,-1),float2(-1,3),float2(3,-1)}; return float4(p[id],0,1); }";
        const char computeSource[]="RWTexture2D<float4> output:register(u0); [numthreads(1,1,1)] void main(uint3 id:SV_DispatchThreadID) { output[id.xy]=float4(1,0,0,1); }";
        ComPtr<ID3DBlob> pixelCode, computeCode, vertexCode;
        Check(D3DCompile(pixelSource,sizeof(pixelSource),nullptr,nullptr,nullptr,"main","ps_5_0",0,0,&pixelCode,nullptr));
        Check(D3DCompile(computeSource,sizeof(computeSource),nullptr,nullptr,nullptr,"main","cs_5_0",0,0,&computeCode,nullptr));
        Check(D3DCompile(vertexSource,sizeof(vertexSource),nullptr,nullptr,nullptr,"main","vs_5_0",0,0,&vertexCode,nullptr));
        ComPtr<ID3D11PixelShader> pixel; ComPtr<ID3D11ComputeShader> compute;
        Check(device->CreatePixelShader(pixelCode->GetBufferPointer(),pixelCode->GetBufferSize(),nullptr,&pixel));
        Check(device->CreateComputeShader(computeCode->GetBufferPointer(),computeCode->GetBufferSize(),nullptr,&compute));
        ComPtr<ID3D11VertexShader> vertex; Check(device->CreateVertexShader(vertexCode->GetBufferPointer(),vertexCode->GetBufferSize(),nullptr,&vertex));
        ID3D11RenderTargetView* boundTarget=rtv.Get(); context->OMSetRenderTargets(1,&boundTarget,nullptr);
        ID3D11ShaderResourceView* boundInput=srv.Get(); context->PSSetShaderResources(127,1,&boundInput);
        context->PSSetShader(pixel.Get(),nullptr,0);
        char json[32768];
        ULONG textureRefs=References(input.Get()), viewRefs=References(srv.Get()), shaderRefs=References(pixel.Get());
        for (int i=0; i<1000; ++i) Need(TwinKartRender::DescribePass(context.Get(),false,120,0x1234,json,sizeof(json))>0,"Scene pass not observed");
        Need(strstr(json,"\"stage\":\"pixel\"") && strstr(json,"\"slot\":127") && strstr(json,"\"mip\":1"),"Binding slot or subresource lost");
        Need(strstr(json,"\"shader_sha256\":\"") && strstr(json,"\"guides_validated\":false"),"Hash missing or guide evidence invented");
        Need(textureRefs==References(input.Get()) && viewRefs==References(srv.Get()) && shaderRefs==References(pixel.Get()),"Capture leaks COM references");
        char tiny[8]; Need(!TwinKartRender::DescribePass(context.Get(),false,120,0,tiny,sizeof(tiny)) && !tiny[0],"Truncated JSON accepted");
        Need(textureRefs==References(input.Get()) && viewRefs==References(srv.Get()),"Truncation leaked references");
        ComPtr<ID3D11RenderTargetView> afterTarget; ComPtr<ID3D11ShaderResourceView> afterInput; ComPtr<ID3D11PixelShader> afterShader;
        context->OMGetRenderTargets(1,&afterTarget,nullptr); context->PSGetShaderResources(127,1,&afterInput); context->PSGetShader(&afterShader,nullptr,nullptr);
        Need(afterTarget.Get()==rtv.Get() && afterInput.Get()==srv.Get() && afterShader.Get()==pixel.Get(),"Observer changed pipeline bindings");
        wchar_t tracePath[96]; swprintf_s(tracePath,L"RenderPassTraceTests-%lu-%lu.jsonl",GetCurrentProcessId(),GetTickCount());
        output=CreateFileW(tracePath,GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_NEW,FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE,nullptr);
        Need(output!=INVALID_HANDLE_VALUE,"Private test trace creation failed");
        gameContext.store(context.Get(),std::memory_order_release); tracePasses=true; frames=120;
        // Exercise installed wrappers and verify pixels on a real WARP device.
        context->VSSetShader(vertex.Get(),nullptr,0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        D3D11_VIEWPORT viewport{0,0,256,128,0,1}; context->RSSetViewports(1,&viewport);
        USHORT indices[]{0,1,2,0,1,2}; D3D11_BUFFER_DESC bufferDesc{};
        bufferDesc.ByteWidth=sizeof(indices); bufferDesc.BindFlags=D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA bufferData{indices,0,0};
        ComPtr<ID3D11Buffer> indexBuffer; Check(device->CreateBuffer(&bufferDesc,&bufferData,&indexBuffer));
        context->IASetIndexBuffer(indexBuffer.Get(),DXGI_FORMAT_R16_UINT,0);
        UINT drawArguments[]{3,1,0,1}, indexedArguments[]{3,1,3,0,1}, dispatchArguments[]{1,1,1};
        auto arguments=[&](UINT* values,UINT length) {
            D3D11_BUFFER_DESC argsDesc{}; argsDesc.ByteWidth=length*sizeof(UINT); argsDesc.MiscFlags=D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
            D3D11_SUBRESOURCE_DATA data{values,0,0}; ComPtr<ID3D11Buffer> result;
            Check(device->CreateBuffer(&argsDesc,&data,&result)); return result;
        };
        auto drawArgs=arguments(drawArguments,4), indexedArgs=arguments(indexedArguments,5), dispatchArgs=arguments(dispatchArguments,3);
        D3D11_TEXTURE2D_DESC stagingDesc=desc; stagingDesc.BindFlags=0; stagingDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        stagingDesc.Usage=D3D11_USAGE_STAGING; stagingDesc.MipLevels=2;
        ComPtr<ID3D11Texture2D> staging; Check(device->CreateTexture2D(&stagingDesc,nullptr,&staging));
        auto readPixel=[&](UINT mip) {
            context->CopyResource(staging.Get(),target.Get());
            D3D11_MAPPED_SUBRESOURCE mapped{}; Check(context->Map(staging.Get(),mip,D3D11_MAP_READ,0,&mapped));
            BYTE* bytes=static_cast<BYTE*>(mapped.pData);
            bool red=bytes[0]==255 && bytes[1]==0 && bytes[2]==0 && bytes[3]==255;
            context->Unmap(staging.Get(),mip); Need(red,"Hook dropped or corrupted a real GPU draw/dispatch");
        };
        const float black[4]{0,0,0,0};
        Need(Hooks(context.Get()).table!=nullptr,"Runtime changed context vtable after pipeline setup");
        for(int variant=0;variant<6;++variant) {
            context->ClearRenderTargetView(rtv.Get(),black);
            switch(variant) {
                case 0: context->Draw(3,0); break;
                case 1: context->DrawIndexed(3,3,0); break;
                case 2: context->DrawInstanced(3,2,0,1); break;
                case 3: context->DrawIndexedInstanced(3,2,3,0,1); break;
                case 4: context->DrawInstancedIndirect(drawArgs.Get(),0); break;
                case 5: context->DrawIndexedInstancedIndirect(indexedArgs.Get(),0); break;
            }
            readPixel(1);
            Need(passRows==variant+1,"Draw wrapper stopped recording after runtime dispatch initialization");
        }
        ComPtr<ID3D11PixelShader> latePixel;
        Check(device->CreatePixelShader(pixelCode->GetBufferPointer(),pixelCode->GetBufferSize(),nullptr,&latePixel));
        context->PSSetShader(latePixel.Get(),nullptr,0);
        Need(TwinKartRender::DescribePass(context.Get(),false,120,0,json,sizeof(json)) && strstr(json,"\"shader_sha256\":\""),"Shader creation hook lost after drawing");
        context->OMSetRenderTargets(0,nullptr,nullptr);
        Need(!TwinKartRender::DescribePass(context.Get(),false,120,0,json,sizeof(json)),"Input-only pass reported a scene output");
        ID3D11UnorderedAccessView* boundUav=uav.Get(); context->CSSetUnorderedAccessViews(0,1,&boundUav,nullptr);
        context->CSSetShader(compute.Get(),nullptr,0); context->CSSetShaderResources(9,1,&boundInput);
        Need(TwinKartRender::DescribePass(context.Get(),true,240,0x5678,json,sizeof(json)) && strstr(json,"\"stage\":\"compute\"") && strstr(json,"\"slot\":9"),"Compute input/output correlation missing");
        Need(strstr(json,"\"shader_sha256\":\""),"Compute shader creation hook did not attach its hash");
        const UINT clear[4]{};
        context->ClearUnorderedAccessViewUint(uav.Get(),clear); context->Dispatch(1,1,1); readPixel(0);
        context->ClearUnorderedAccessViewUint(uav.Get(),clear); context->DispatchIndirect(dispatchArgs.Get(),0); readPixel(0);
        if (passRows!=8) fprintf(stderr,"Pass capture counters: rows=%ld sampled=%ld\n",passRows,sampledRows);
        Need(passRows==8,"Installed hooks did not produce eight sampled pass records");
        tracePasses=false; gameContext.store(nullptr,std::memory_order_release);
        CloseHandle(output); output=INVALID_HANDLE_VALUE;
        ComPtr<ID3D11DeviceContext> deferred; Check(device->CreateDeferredContext(0,&deferred));
        Need(!TwinKartRender::DescribePass(deferred.Get(),false,1,0,json,sizeof(json)),"Deferred context incorrectly accepted");
        context->ClearState();
        puts("PASS D3D11 WARP: eight installed draw/dispatch hooks preserve rendered pixels; shader creation hooks, pixel/compute bindings, mip/high-slot identity, unchanged pipeline, 1000 captures without COM leaks, truncation and deferred-context rejection. No game started.");
        return 0;
    } catch (const std::exception& error) { fprintf(stderr,"%s\n",error.what()); return 1; }
}
