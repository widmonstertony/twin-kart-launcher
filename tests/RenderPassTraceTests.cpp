#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3dcompiler.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <wrl/client.h>
#include <fstream>
#include <iterator>
#include <vector>
#include <thread>
#include "../native/DriftRenderProbe.cpp"
using Microsoft::WRL::ComPtr;
void Need(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
void Check(HRESULT result) { Need(SUCCEEDED(result), "D3D11 fixture creation failed"); }
ULONG References(IUnknown* object) { object->AddRef(); return object->Release(); }
// The native DXGI vtable can be shared by a private generated-frame chain.
// Its worker thread must forward Present without touching D3D11 frame state.
struct PresentFixture final:IDXGISwapChain {
    ID3D11Device* device=nullptr;
    explicit PresentFixture(ID3D11Device* value):device(value){}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID,void**) override{return E_NOINTERFACE;}
    ULONG STDMETHODCALLTYPE AddRef() override{return 1;}ULONG STDMETHODCALLTYPE Release() override{return 1;}
    HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID,UINT,const void*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID,const IUnknown*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID,UINT*,void*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetParent(REFIID,void**) override{return E_NOINTERFACE;}
    HRESULT STDMETHODCALLTYPE GetDevice(REFIID id,void** out) override{if(!out)return E_POINTER;*out=nullptr;return device?device->QueryInterface(id,out):E_NOINTERFACE;}
    HRESULT STDMETHODCALLTYPE Present(UINT,UINT) override{return E_UNEXPECTED;}
    HRESULT STDMETHODCALLTYPE GetBuffer(UINT,REFIID,void** out) override{if(out)*out=nullptr;return E_NOINTERFACE;}
    HRESULT STDMETHODCALLTYPE SetFullscreenState(BOOL,IDXGIOutput*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetFullscreenState(BOOL*,IDXGIOutput**) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetDesc(DXGI_SWAP_CHAIN_DESC*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE ResizeBuffers(UINT,UINT,UINT,DXGI_FORMAT,UINT) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE ResizeTarget(const DXGI_MODE_DESC*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetContainingOutput(IDXGIOutput**) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetFrameStatistics(DXGI_FRAME_STATISTICS*) override{return E_NOTIMPL;}
    HRESULT STDMETHODCALLTYPE GetLastPresentCount(UINT*) override{return E_NOTIMPL;}
};
volatile LONG fixturePresents=0;
HRESULT STDMETHODCALLTYPE ForwardFixturePresent(IDXGISwapChain*,UINT,UINT){InterlockedIncrement(&fixturePresents);return S_OK;}
void CheckNativePresentIsolation(ID3D11Device* device) {
    auto savedPresent=present;LONG savedFrames=frames,savedHook=realContextHooked,savedSubmitted=nrLastSubmitted;
    present=ForwardFixturePresent;frames=41;realContextHooked=1;nrLastSubmitted=-1;
    PresentFixture foreign(nullptr),native(device);
    std::thread worker([&]{for(unsigned n=0;n<240;++n)OnPresent(&foreign,0,0);});worker.join();
    Need(frames==41 && fixturePresents==240,"Generated-frame worker Present changed native frame/history state");
    Need(OnPresent(&native,0,0)==S_OK && frames==42 && fixturePresents==241,"Native D3D11 Present was incorrectly filtered");
    Need(OnPresent(&native,0,DXGI_PRESENT_TEST)==S_OK && frames==42 && fixturePresents==242,"DXGI test presentation advanced native frame state");
    present=savedPresent;frames=savedFrames;realContextHooked=savedHook;nrLastSubmitted=savedSubmitted;
}
int main() {
    try {
        ComPtr<ID3D11Device> device; ComPtr<ID3D11DeviceContext> context;
        Check(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context));
        CheckNativePresentIsolation(device.Get());
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
        unsigned groupX=0,groupY=0,groupZ=0;
        Need(TwinKartRender::ShaderThreadGroup(compute.Get(),groupX,groupY,groupZ) && groupX==1 && groupY==1 && groupZ==1,"Actual native shader group metadata missing");
        ComPtr<ID3D11VertexShader> vertex; Check(device->CreateVertexShader(vertexCode->GetBufferPointer(),vertexCode->GetBufferSize(),nullptr,&vertex));
        ID3D11RenderTargetView* boundTarget=rtv.Get(); context->OMSetRenderTargets(1,&boundTarget,nullptr);
        ID3D11ShaderResourceView* boundInput=srv.Get(); context->PSSetShaderResources(127,1,&boundInput);
        context->PSSetShader(pixel.Get(),nullptr,0);
        char json[32768];
        ULONG textureRefs=References(input.Get()), viewRefs=References(srv.Get()), shaderRefs=References(pixel.Get());
        for (int i=0; i<1000; ++i) Need(TwinKartRender::DescribePass(context.Get(),false,120,0x1234,json,sizeof(json))>0,"Scene pass not observed");
        Need(strstr(json,"\"stage\":\"pixel\"") && strstr(json,"\"slot\":127") && strstr(json,"\"mip\":1"),"Binding slot or subresource lost");
        Need(strstr(json,"\"shader_sha256\":\"") && strstr(json,"\"guides_validated\":false"),"Hash missing or guide evidence invented");
        const char interfaceSource[]="struct CameraData { float4 ViewSizeAndInvSize; row_major float4x4 ClipToPrevClip; float PreExposure; }; cbuffer View:register(b2) { CameraData Camera; }; Texture2D<float4> SceneDepthTexture:register(t3); Texture2D<float4> VelocityTexture:register(t4); Texture2D<float4> PostprocessInput0:register(t5); float4 main():SV_Target { return SceneDepthTexture.Load(int3(0,0,0))+VelocityTexture.Load(int3(0,0,0))+PostprocessInput0.Load(int3(0,0,0))+Camera.ViewSizeAndInvSize*Camera.PreExposure+mul(float4(1,1,1,1),Camera.ClipToPrevClip); }";
        ComPtr<ID3DBlob> interfaceCode; ComPtr<ID3D11PixelShader> interfaceShader;
        Check(D3DCompile(interfaceSource,sizeof(interfaceSource),nullptr,nullptr,nullptr,"main","ps_5_0",0,0,&interfaceCode,nullptr));
        Check(device->CreatePixelShader(interfaceCode->GetBufferPointer(),interfaceCode->GetBufferSize(),nullptr,&interfaceShader));
        context->PSSetShader(interfaceShader.Get(),nullptr,0);
        Need(TwinKartRender::DescribePass(context.Get(),false,120,0,json,sizeof(json)) && strstr(json,"\"name\":\"SceneDepthTexture\",\"slot\":3") &&
            strstr(json,"\"name\":\"VelocityTexture\",\"slot\":4") && strstr(json,"\"name\":\"PostprocessInput0\",\"slot\":5") &&
            strstr(json,"\"name\":\"View.Camera.ClipToPrevClip\",\"offset\":16") && strstr(json,"\"name\":\"View.Camera.PreExposure\",\"offset\":80"),"Real shader reflection did not preserve resource slots and nested camera offsets");
        context->PSSetShader(pixel.Get(),nullptr,0);
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
        // Exercise selected-pass GPU staging on actual initialized resources,
        // check their bytes and leave every shader binding untouched.
        wchar_t captureDirectory[96]; swprintf_s(captureDirectory,L"TemporalCaptureTests-%lu-%lu",GetCurrentProcessId(),GetTickCount());
        Need(CreateDirectoryW(captureDirectory,nullptr)!=FALSE,"Capture directory creation failed");
        char selectedHash[65]; Need(TwinKartRender::ShaderHash(compute.Get(),selectedHash),"Compute digest unavailable");
        Need(!TwinKartGraphics::ConfigureTemporalCapture(captureDirectory,"invalid",1),"Malformed selector accepted");
        Need(TwinKartGraphics::ConfigureTemporalCapture(captureDirectory,selectedHash,1),"Capture configuration rejected");
        std::vector<BYTE> expected(32*16*4); for(size_t n=0;n<expected.size();++n) expected[n]=static_cast<BYTE>(n*37);
        D3D11_TEXTURE2D_DESC captureDesc{}; captureDesc.Width=32; captureDesc.Height=16; captureDesc.MipLevels=1; captureDesc.ArraySize=1;
        captureDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM; captureDesc.SampleDesc.Count=1; captureDesc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
        D3D11_SUBRESOURCE_DATA captureData{expected.data(),128,0};
        ComPtr<ID3D11Texture2D> captureTexture; Check(device->CreateTexture2D(&captureDesc,&captureData,&captureTexture));
        ComPtr<ID3D11ShaderResourceView> captureView; Check(device->CreateShaderResourceView(captureTexture.Get(),nullptr,&captureView));
        ID3D11ShaderResourceView* captureBinding=captureView.Get(); context->CSSetShaderResources(0,1,&captureBinding);
        D3D11_BUFFER_DESC constantDesc{}; constantDesc.ByteWidth=16; constantDesc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        const UINT knownConstants[]{0x3f800000,0x40000000,0x40400000,0x40800000}; D3D11_SUBRESOURCE_DATA constantData{knownConstants,0,0};
        ComPtr<ID3D11Buffer> knownBuffer; Check(device->CreateBuffer(&constantDesc,&constantData,&knownBuffer));
        ID3D11Buffer* constantBinding=knownBuffer.Get(); context->CSSetConstantBuffers(0,1,&constantBinding);
        ULONG beforeCapture=References(captureTexture.Get());
        Need(TwinKartGraphics::CaptureTemporalInputs(context.Get(),240),"Real GPU temporal input capture failed");
        Need(!TwinKartGraphics::CaptureTemporalInputs(context.Get(),360),"Capture limit not enforced");
        wchar_t resourceFile[160],constantFile[160],metadataFile[160];
        swprintf_s(resourceFile,L"%s\\frame-240-srv-0.bin",captureDirectory);
        swprintf_s(constantFile,L"%s\\frame-240-cb-0.bin",captureDirectory);
        swprintf_s(metadataFile,L"%s\\frame-240-capture-0.json",captureDirectory);
        auto read=[&](const wchar_t* path) { std::ifstream file(path,std::ios::binary); return std::vector<BYTE>(std::istreambuf_iterator<char>(file),{}); };
        Need(read(resourceFile)==expected,"Actual texture capture bytes differ");
        auto constantBytes=read(constantFile); Need(constantBytes.size()==16 && !memcmp(constantBytes.data(),knownConstants,16),"Camera constant bytes differ");
        auto metadataBytes=read(metadataFile); std::string metadata(metadataBytes.begin(),metadataBytes.end());
        Need(metadata.find("\"guides_validated\":false")!=std::string::npos && metadata.find("\"final_composition\":false")!=std::string::npos,"Capture invented rendering acceptance");
        ComPtr<ID3D11ShaderResourceView> captureAfter; context->CSGetShaderResources(0,1,&captureAfter);
        Need(captureAfter.Get()==captureView.Get() && beforeCapture==References(captureTexture.Get()),"Capture changed binding or leaked resource references");
        Need(DeleteFileW(resourceFile) && DeleteFileW(constantFile) && DeleteFileW(metadataFile) && RemoveDirectoryW(captureDirectory),"Capture fixture cleanup failed");
        tracePasses=false;
        // Same-frame geometry comes from actual draws into pooled targets,
        // never from allocation dimensions. Exercise installed write hooks,
        // including the context1 aliases, before allowing a padded SR crop.
        context->ClearState();
        D3D11_TEXTURE2D_DESC pooledDesc{};pooledDesc.Width=512;pooledDesc.Height=256;
        pooledDesc.MipLevels=pooledDesc.ArraySize=pooledDesc.SampleDesc.Count=1;pooledDesc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
        pooledDesc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
        ComPtr<ID3D11Texture2D> pooled,copySource;Check(device->CreateTexture2D(&pooledDesc,nullptr,&pooled));Check(device->CreateTexture2D(&pooledDesc,nullptr,&copySource));
        ComPtr<ID3D11RenderTargetView> pooledTarget;ComPtr<ID3D11UnorderedAccessView> pooledWrite;
        Check(device->CreateRenderTargetView(pooled.Get(),nullptr,&pooledTarget));Check(device->CreateUnorderedAccessView(pooled.Get(),nullptr,&pooledWrite));
        TwinKartGraphics::NativeRect rectangle{};
        auto hasRectangle=[&](){return TwinKartGraphics::NativeSceneRectangle(pooled.Get(),context.Get(),120,320,180,rectangle);};
        Need(!hasRectangle(),"Allocation size invented native scene geometry");
        auto drawPooled=[&](){
            context->ClearState();ID3D11RenderTargetView* targetView=pooledTarget.Get();context->OMSetRenderTargets(1,&targetView,nullptr);
            context->VSSetShader(vertex.Get(),nullptr,0);context->PSSetShader(pixel.Get(),nullptr,0);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            D3D11_VIEWPORT area{16,8,320,180,0,1};context->RSSetViewports(1,&area);context->Draw(3,0);
            Need(hasRectangle() && rectangle.left==16 && rectangle.top==8 && rectangle.right==336 && rectangle.bottom==188,"Actual native producer viewport was not retained");
        };
        drawPooled();
        Need(!TwinKartGraphics::NativeSceneRectangle(pooled.Get(),context.Get(),121,320,180,rectangle) &&
            !TwinKartGraphics::NativeSceneRectangle(pooled.Get(),context.Get(),120,512,256,rectangle),"Stale frame or allocation dimensions accepted as native scene");
        context->ClearRenderTargetView(pooledTarget.Get(),black);Need(!hasRectangle(),"Clear retained stale producer geometry");
        drawPooled();context->CopyResource(pooled.Get(),copySource.Get());Need(!hasRectangle(),"Copy retained stale producer geometry");
        drawPooled();context->CopySubresourceRegion(pooled.Get(),0,0,0,0,copySource.Get(),0,nullptr);Need(!hasRectangle(),"Subresource copy retained stale geometry");
        std::vector<unsigned> replaced(512*256,0xff00ff00);
        drawPooled();context->UpdateSubresource(pooled.Get(),0,nullptr,replaced.data(),512*4,0);Need(!hasRectangle(),"Update retained stale producer geometry");
        drawPooled();context->ClearUnorderedAccessViewUint(pooledWrite.Get(),clear);Need(!hasRectangle(),"Integer UAV clear retained stale geometry");
        drawPooled();context->ClearUnorderedAccessViewFloat(pooledWrite.Get(),black);Need(!hasRectangle(),"Float UAV clear retained stale geometry");
        drawPooled();context->OMSetRenderTargets(0,nullptr,nullptr);ID3D11UnorderedAccessView* writeView=pooledWrite.Get();context->CSSetUnorderedAccessViews(0,1,&writeView,nullptr);
        context->CSSetShader(compute.Get(),nullptr,0);context->Dispatch(1,1,1);Need(!hasRectangle(),"Compute write retained stale producer geometry");
        ComPtr<ID3D11DeviceContext> commandContext;Check(device->CreateDeferredContext(0,&commandContext));
        commandContext->CopyResource(copySource.Get(),pooled.Get());ComPtr<ID3D11CommandList> commandList;Check(commandContext->FinishCommandList(FALSE,&commandList));
        drawPooled();context->ExecuteCommandList(commandList.Get(),TRUE);Need(!hasRectangle(),"Unobserved command list retained producer geometry");
        ComPtr<ID3D11DeviceContext1> context1;Check(context.As(&context1));
        drawPooled();context1->CopySubresourceRegion1(pooled.Get(),0,0,0,0,copySource.Get(),0,nullptr,0);Need(!hasRectangle(),"Context1 copy retained geometry");
        drawPooled();context1->UpdateSubresource1(pooled.Get(),0,nullptr,replaced.data(),512*4,0,0);Need(!hasRectangle(),"Context1 update retained geometry");
        drawPooled();context1->DiscardResource(pooled.Get());Need(!hasRectangle(),"DiscardResource retained geometry");
        drawPooled();context1->DiscardView(pooledTarget.Get());Need(!hasRectangle(),"DiscardView retained geometry");
        drawPooled();context1->ClearView(pooledTarget.Get(),black,nullptr,0);Need(!hasRectangle(),"ClearView retained geometry");
        drawPooled();context1->DiscardView1(pooledTarget.Get(),nullptr,0);Need(!hasRectangle(),"DiscardView1 retained geometry");
        drawPooled();
        pooledDesc.Usage=D3D11_USAGE_STAGING;pooledDesc.BindFlags=0;pooledDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
        ComPtr<ID3D11Texture2D> pooledReadback;Check(device->CreateTexture2D(&pooledDesc,nullptr,&pooledReadback));context->CopyResource(pooledReadback.Get(),pooled.Get());
        D3D11_MAPPED_SUBRESOURCE actual{};Check(context->Map(pooledReadback.Get(),0,D3D11_MAP_READ,0,&actual));
        auto nativePixel=static_cast<const BYTE*>(actual.pData)+8*actual.RowPitch+16*4;
        bool red=nativePixel[0]==255 && nativePixel[1]==0 && nativePixel[2]==0 && nativePixel[3]==255;
        context->Unmap(pooledReadback.Get(),0);Need(red && hasRectangle(),"Write tracking corrupted real GPU pixels or invalidated a read-only copy source");
        gameContext.store(nullptr,std::memory_order_release);
        CloseHandle(output); output=INVALID_HANDLE_VALUE;
        ComPtr<ID3D11DeviceContext> deferred; Check(device->CreateDeferredContext(0,&deferred));
        Need(!TwinKartRender::DescribePass(deferred.Get(),false,1,0,json,sizeof(json)),"Deferred context incorrectly accepted");
        context->ClearState();
        puts("PASS D3D11 WARP: native versus generated-worker Present isolation, eight rendering paths and mutation hooks; native producer viewport/crop, frame/dimension rejection, resource-write/command-list invalidation, unchanged GPU pixels/bindings/references, bounded capture and 1000 observations. No game started.");
        return 0;
    } catch (const std::exception& error) { fprintf(stderr,"%s\n",error.what()); return 1; }
}
