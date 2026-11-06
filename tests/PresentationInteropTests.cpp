#define TWIN_KART_PRESENTATION_TEST 1
#include "../native/NativePresentation.cpp"
#include <stdexcept>
using namespace TwinKartGraphics;
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void CheckPresentationInputCompletion(){
    GenerationState state{};state.inputCompletionFence=reinterpret_cast<ID3D12Fence*>(1);state.inputCompletionValue=7;
    unsigned waits=0,restores=0,drains=0;
    HRESULT result=CompletePresentationInputs(state,true,[&](ID3D12Fence*,UINT64){++waits;return DXGI_ERROR_DEVICE_REMOVED;},
        [&](){++restores;return S_OK;},[&](){++drains;return S_OK;});
    Need(result==DXGI_ERROR_DEVICE_REMOVED && waits==1 && !restores && !drains,"Failed provider fence wait allowed guide/resource reuse");
    waits=restores=drains=0;state.result=1;
    result=CompletePresentationInputs(state,true,[&](ID3D12Fence*,UINT64){++waits;return S_OK;},
        [&](){++restores;return S_OK;},[&](){++drains;return S_OK;});
    Need(FAILED(result) && !waits && !restores && !drains,"Failed provider query released unknown-lifetime inputs");
    state.result=0;
    result=CompletePresentationInputs(state,true,[&](ID3D12Fence*,UINT64){++waits;return S_OK;},
        [&](){++restores;return E_FAIL;},[&](){++drains;return S_OK;});
    Need(FAILED(result) && waits==1 && restores==1 && !drains,"Failed guide return submission accepted");
    waits=restores=drains=0;
    result=CompletePresentationInputs(state,false,[&](ID3D12Fence*,UINT64){++waits;return S_OK;},
        [&](){++restores;return S_OK;},[&](){++drains;return E_ABORT;});
    Need(result==E_ABORT && waits==1 && !restores && drains==1,"Native-only presentation omitted or repeated final drain");

    // Real WARP queue ordering: return-state commands may be recorded before
    // provider completion, but their queue execution and final fence stay
    // blocked behind that completion. The producer allocator/list stays owned
    // and is not reset to record the returns.
    ComPtr<IDXGIFactory4> factory;ComPtr<IDXGIAdapter> adapter;ComPtr<ID3D12Device> device;
    Need(SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))) && SUCCEEDED(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter))) &&
        SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device))),"D3D12 WARP return-order fixture");
    ComPtr<ID3D12CommandQueue> queue;D3D12_COMMAND_QUEUE_DESC queueDesc{};
    Need(SUCCEEDED(device->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&queue))),"Return-order queue");
    ComPtr<ID3D12CommandAllocator> producerAllocator;ComPtr<ID3D12GraphicsCommandList> producer;
    Need(SUCCEEDED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&producerAllocator))) &&
        SUCCEEDED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,producerAllocator.Get(),nullptr,IID_PPV_ARGS(&producer))),"Independent producer commands");
    D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;desc.Width=16;desc.Height=16;
    desc.DepthOrArraySize=1;desc.MipLevels=1;desc.SampleDesc.Count=1;desc.Format=DXGI_FORMAT_R32_FLOAT;
    ComPtr<ID3D12Resource> guide;
    Need(SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_COMMON,nullptr,IID_PPV_ARGS(&guide))),"Return-order guide");
    PresentationGuideReturns returns;Need(SUCCEEDED(returns.Initialize(device.Get())),"Independent return allocator/list");
    ComPtr<ID3D12Fence> providerFence,completed;
    Need(SUCCEEDED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&providerFence))) &&
        SUCCEEDED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&completed))),"Return-order fences");
    state={};state.inputCompletionFence=providerFence.Get();
    for(UINT64 frame=1;frame<=2;++frame){
        if(frame>1)Need(SUCCEEDED(producerAllocator->Reset()) && SUCCEEDED(producer->Reset(producerAllocator.Get(),nullptr)),"Producer reuse after completed final fence");
        Transition(producer.Get(),guide.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        Need(SUCCEEDED(producer->Close()),"Producer close");ID3D12CommandList* submit[]{producer.Get()};queue->ExecuteCommandLists(1,submit);
        state.inputCompletionValue=frame;waits=restores=drains=0;
        result=CompletePresentationInputs(state,true,[&](ID3D12Fence* fence,UINT64 value){++waits;return queue->Wait(fence,value);},[&](){
            ++restores;ID3D12Resource* resources[]{guide.Get()};return returns.Queue(queue.Get(),resources,1);
        },[&](){++drains;return queue->Signal(completed.Get(),frame);});
        Need(SUCCEEDED(result) && waits==1 && restores==1 && drains==1,"Return sequence did not queue exactly one final completion");
        Need(completed->GetCompletedValue()==frame-1,"Return commands passed the unresolved provider input fence");
        Need(SUCCEEDED(providerFence->Signal(frame)),"Release simulated provider inputs");
        HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);Need(event!=nullptr,"Return-order event");
        HRESULT armed=completed->SetEventOnCompletion(frame,event);DWORD finished=SUCCEEDED(armed)?WaitForSingleObject(event,10000):WAIT_FAILED;
        CloseHandle(event);Need(SUCCEEDED(armed) && finished==WAIT_OBJECT_0,"Final queue fence did not cover guide return commands");
    }
}
int main(){HWND window=nullptr;try{
    CheckPresentationInputCompletion();
    auto generated=VerifyPresentationInterval(120,120,0,0,120,0,240);
    Need(generated.verified && generated.generatedFrames==120,"Sustained 2x provider presentations were not verified");
    auto partial=VerifyPresentationInterval(120,120,0,0,120,0,121);
    Need(partial.verified && partial.generatedFrames==1,"Valid partial generation interval rejected");
    GenerationState observation{};
    Need(InspectPresentationObservation(observation,S_OK,true,true).qualified,"Valid presentation observation rejected");
    auto failedPresent=InspectPresentationObservation(observation,DXGI_ERROR_DEVICE_REMOVED,true,true);
    Need(failedPresent.presentError && !failedPresent.qualified,"Failed native Present counted as qualified");
    observation.status=1;
    auto badStatus=InspectPresentationObservation(observation,S_OK,true,true);
    Need(badStatus.providerStatusError && !badStatus.qualified,"Provider failure status counted as qualified");
    observation.status=0;observation.result=1;
    Need(!InspectPresentationObservation(observation,S_OK,true,true).qualified,"Failed provider query counted as qualified");
    observation.result=0;
    auto occluded=InspectPresentationObservation(observation,DXGI_STATUS_OCCLUDED,true,true);
    Need(occluded.occluded && !occluded.qualified,"Occluded native Present counted as qualified");
    Need(!InspectPresentationObservation(observation,S_OK,false,true).qualified &&
        !InspectPresentationObservation(observation,S_OK,true,false).qualified,"Missing current-frame evidence counted as qualified");
    Need(!VerifyPresentationInterval(120,120,0,0,120,0,240,1,0).verified,"Failed Present interval reported generated frames");
    Need(!VerifyPresentationInterval(120,120,0,0,120,0,240,0,1).verified,"Nonzero provider status interval reported generated frames");
    Need(!VerifyPresentationInterval(119,119,0,0,119,0,238).verified,"Incomplete interval accepted");
    Need(!VerifyPresentationInterval(121,121,0,0,121,0,242).verified,"Oversized interval accepted");
    Need(!VerifyPresentationInterval(0,0,0,0,0,0,0).verified,"Empty interval accepted");
    Need(!VerifyPresentationInterval(120,120,0,0,120,0,119).verified,"Provider count below native count accepted");
    auto impossible=VerifyPresentationInterval(120,120,0,0,120,0,241);
    Need(!impossible.verified && !impossible.generatedFrames,"Out-of-contract provider count interpreted as generated frames");
    Need(!VerifyPresentationInterval(120,120,0,0,120,0,~0ull).verified,"Overflow-sized provider count accepted");
    Need(!VerifyPresentationInterval(120,119,0,0,120,0,240).verified,"Unqualified presentation interval accepted");
    Need(!VerifyPresentationInterval(120,120,1,0,120,0,240).verified,"Provider query error accepted");
    Need(!VerifyPresentationInterval(120,120,0,1,120,0,240).verified,"Occluded presentation accepted");
    Need(!VerifyPresentationInterval(120,120,0,0,119,1,240).verified,"Repeated native frame accepted");
    Need(!VerifyPresentationInterval(120,120,0,0,120,0,120).verified,"Native-only interval reported generated frames");
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"TwinKartPresentationInteropFixture";
    Need(RegisterClassW(&wc)!=0,"Fixture class");
    window=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,0,0,256,128,nullptr,nullptr,wc.hInstance,nullptr);
    Need(window!=nullptr,"Hidden fixture window");
    DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=256;desc.BufferDesc.Height=128;desc.BufferDesc.Format=DXGI_FORMAT_R10G10B10A2_UNORM;
    desc.BufferCount=1;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.OutputWindow=window;desc.Windowed=TRUE;
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;ComPtr<IDXGISwapChain> original;
    Need(SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&original,&device,nullptr,&context)),"WARP fixture swapchain");
    ComPtr<ID3D11Device> otherDevice;
    Need(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&otherDevice,nullptr,nullptr)),"Independent WARP device identity fixture");
    Need(SameNativeGraphicsDevice(device.Get(),device.Get()) && !SameNativeGraphicsDevice(device.Get(),otherDevice.Get()) && !SameNativeGraphicsDevice(device.Get(),nullptr),"Shared NGX core ownership used an adapter or null identity");
    ComPtr<IDXGISwapChain4> display;Need(SUCCEEDED(original.As(&display)),"Current DXGI interfaces");
    auto proxy=new PresentationChain;Need(SUCCEEDED(proxy->InitializeFixture(device.Get(),desc,display.Get())),"Shadow texture setup");
    ComPtr<IDXGISwapChain4> owned;owned.Attach(proxy);
    ComPtr<IUnknown> identity;Need(SUCCEEDED(owned.As(&identity)),"IUnknown identity");
    ComPtr<IDXGISwapChain1> first;Need(SUCCEEDED(owned.As(&first)),"Swapchain1 routing");
    ComPtr<IUnknown> secondIdentity;Need(SUCCEEDED(first.As(&secondIdentity)) && identity.Get()==secondIdentity.Get(),"COM identity escaped wrapper");
    ComPtr<ID3D11Device> routed;Need(SUCCEEDED(owned->GetDevice(IID_PPV_ARGS(&routed))) && routed.Get()==device.Get(),"Native GetDevice did not return D3D11");
    ComPtr<ID3D11Texture2D> shadow;Need(SUCCEEDED(owned->GetBuffer(0,IID_PPV_ARGS(&shadow))),"Native GetBuffer did not return D3D11 texture");
    ComPtr<ID3D11Texture2D> real;Need(SUCCEEDED(original->GetBuffer(0,IID_PPV_ARGS(&real))) && real.Get()!=shadow.Get(),"Private display buffer exposed instead of shadow");
    ComPtr<ID3D11RenderTargetView> target;ComPtr<ID3D11ShaderResourceView> input;
    Need(SUCCEEDED(device->CreateRenderTargetView(shadow.Get(),nullptr,&target)) && SUCCEEDED(device->CreateShaderResourceView(shadow.Get(),nullptr,&input)),"Shadow render/read views");
    D3D11_TEXTURE2D_DESC writeDesc{};writeDesc.Width=256;writeDesc.Height=128;writeDesc.MipLevels=writeDesc.ArraySize=writeDesc.SampleDesc.Count=1;
    writeDesc.Format=DXGI_FORMAT_R32_UINT;writeDesc.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
    ComPtr<ID3D11Texture2D> writeTexture;ComPtr<ID3D11UnorderedAccessView> pixelWrite;
    Need(SUCCEEDED(device->CreateTexture2D(&writeDesc,nullptr,&writeTexture)) && SUCCEEDED(device->CreateUnorderedAccessView(writeTexture.Get(),nullptr,&pixelWrite)),"Pixel UAV fixture");
    ID3D11RenderTargetView* boundTarget=target.Get();ID3D11UnorderedAccessView* boundWrite=pixelWrite.Get();
    context->OMSetRenderTargetsAndUnorderedAccessViews(1,&boundTarget,nullptr,1,1,&boundWrite,nullptr);
    {
        SavedCompute state(context.Get());ID3D11ShaderResourceView* boundInput=input.Get();context->CSSetShaderResources(0,1,&boundInput);
        ComPtr<ID3D11ShaderResourceView> observed;context->CSGetShaderResources(0,1,&observed);
        Need(observed.Get()==input.Get(),"Output target remained bound while UI extraction read it");
    }
    ComPtr<ID3D11RenderTargetView> restoredTarget;ComPtr<ID3D11UnorderedAccessView> restoredWrite;
    context->OMGetRenderTargets(1,&restoredTarget,nullptr);context->OMGetRenderTargetsAndUnorderedAccessViews(0,nullptr,nullptr,1,1,&restoredWrite);
    Need(restoredTarget.Get()==target.Get() && restoredWrite.Get()==pixelWrite.Get(),"UI extraction lost native RTV/pixel-UAV state");
    context->ClearState();restoredTarget.Reset();restoredWrite.Reset();target.Reset();input.Reset();
    ID3D12Resource* wrong=nullptr;Need(FAILED(owned->GetBuffer(0,IID_PPV_ARGS(&wrong))) && !wrong,"D3D12 buffer escaped to native D3D11 renderer");
    void* invalid=reinterpret_cast<void*>(1);Need(owned->GetBuffer(1,__uuidof(ID3D11Texture2D),&invalid)==DXGI_ERROR_INVALID_CALL && !invalid,"Invalid native buffer index accepted");
    Need(owned->ResizeBuffers(1,512,256,DXGI_FORMAT_UNKNOWN,0)==DXGI_ERROR_INVALID_CALL,"Resize accepted while client held shadow references");
    DXGI_SWAP_CHAIN_DESC1 described{};Need(SUCCEEDED(owned->GetDesc1(&described)) && described.BufferCount==1 && described.SwapEffect==DXGI_SWAP_EFFECT_DISCARD,"Private flip descriptor leaked into native interface");
    BOOL fullscreen=TRUE;Need(SUCCEEDED(owned->GetFullscreenState(&fullscreen,nullptr)) && !fullscreen,"Native fullscreen-state query changed fixture mode");
    shadow.Reset();real.Reset();
    Need(SUCCEEDED(owned->ResizeBuffers(1,512,256,DXGI_FORMAT_UNKNOWN,0)),"Released-buffer resize failed before render-owner configuration");
    Need(SUCCEEDED(owned->GetDesc1(&described)) && described.Width==512 && described.Height==256 && described.BufferCount==1,"Resize did not update native shadow descriptor");
    Need(SUCCEEDED(owned->GetBuffer(0,IID_PPV_ARGS(&shadow))),"Resized native shadow missing");
    D3D11_TEXTURE2D_DESC shadowDesc{};shadow->GetDesc(&shadowDesc);Need(shadowDesc.Width==512 && shadowDesc.Height==256,"Resized D3D11 shadow dimensions wrong");
    shadow.Reset();identity.Reset();secondIdentity.Reset();first.Reset();owned.Reset();
    original.Reset();display.Reset();
    desc.BufferCount=2;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.Flags=DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH|DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    Need(Supported(desc),"Observed game flip/tearing contract rejected");
    ComPtr<IDXGIDevice> dxgi;ComPtr<IDXGIAdapter> adapter;ComPtr<IDXGIFactory> factory;
    Need(SUCCEEDED(device.As(&dxgi)) && SUCCEEDED(dxgi->GetAdapter(&adapter)) && SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&factory))),"Fixture factory");
    Need(SUCCEEDED(factory->CreateSwapChain(device.Get(),&desc,&original)) && SUCCEEDED(original.As(&display)),"Two-buffer flip fixture");
    proxy=new PresentationChain;Need(SUCCEEDED(proxy->InitializeFixture(device.Get(),desc,display.Get())),"Flip shadows");owned.Attach(proxy);
    ComPtr<ID3D11Texture2D> flipBuffers[2];
    for(UINT i=0;i<2;++i)Need(SUCCEEDED(owned->GetBuffer(i,IID_PPV_ARGS(&flipBuffers[i]))),"Native flip buffer unavailable");
    Need(flipBuffers[0].Get()!=flipBuffers[1].Get(),"Flip buffers alias");
    Need(SUCCEEDED(owned->GetDesc1(&described)) && described.BufferCount==2 && described.SwapEffect==DXGI_SWAP_EFFECT_FLIP_DISCARD && described.Flags==desc.Flags,"Native flip descriptor changed");
    Need(owned->GetCurrentBackBufferIndex()==0,"Initial logical flip index");
    ComPtr<ID3D11RenderTargetView> flipTarget;
    Need(SUCCEEDED(device->CreateRenderTargetView(flipBuffers[1].Get(),nullptr,&flipTarget)),"Flip draw target");
    D3D11_VIEWPORT viewport{0,0,256,128,0,1};context->RSSetViewports(1,&viewport);
    ID3D11RenderTargetView* flipBound=flipTarget.Get();context->OMSetRenderTargets(1,&flipBound,nullptr);
    const float clear[]{0.25f,0.5f,0.75f,1};context->ClearRenderTargetView(flipBound,clear);
    RecordNativeSceneDraw(context.Get(),73);
    Need(proxy->ProducedBuffer(73)==1,"Current-frame producer lost to logical/display index");
    Need(proxy->ProducedBuffer(74)==0,"Stale producer evidence selected");
    InvalidateNativeScene(flipBuffers[1].Get());Need(proxy->ProducedBuffer(73)==0,"Invalidated write evidence selected");
    context->ClearState();flipTarget.Reset();
    Need(owned->ResizeBuffers(0,512,256,DXGI_FORMAT_UNKNOWN,desc.Flags)==DXGI_ERROR_INVALID_CALL,"Held flip buffers did not block resize");
    flipBuffers[0].Reset();
    Need(owned->ResizeBuffers(0,512,256,DXGI_FORMAT_UNKNOWN,desc.Flags)==DXGI_ERROR_INVALID_CALL,"Held second flip buffer did not block resize");
    flipBuffers[1].Reset();
    Need(owned->ResizeBuffers(0,512,256,DXGI_FORMAT_UNKNOWN,0)==DXGI_ERROR_INVALID_CALL,"Resize changed immutable tearing support");
    Need(SUCCEEDED(owned->ResizeBuffers(0,512,256,DXGI_FORMAT_UNKNOWN,desc.Flags)),"Flip resize with retained count failed");
    Need(SUCCEEDED(owned->GetBuffer(1,IID_PPV_ARGS(&flipBuffers[1]))),"Rebuilt second flip buffer missing");
    flipBuffers[1]->GetDesc(&shadowDesc);Need(shadowDesc.Width==512 && shadowDesc.Height==256,"Flip resize dimensions");
    Need(SUCCEEDED(owned->GetDesc1(&described)) && described.BufferCount==2 && described.Flags==desc.Flags,"Flip resize descriptor");
    flipBuffers[1].Reset();owned.Reset();display.Reset();original.Reset();
    context->ClearState();DestroyWindow(window);window=nullptr;UnregisterClassW(wc.lpszClassName,wc.hInstance);
    puts("PASS hidden WARP native presentation boundary: failed provider-wait/query rejection, D3D12 provider-fence return ordering and allocator reuse, COM identity, D3D11 device/buffer routing, private display isolation, descriptor contract, held-resource resize rejection. No game or NVIDIA FG runtime started.");return 0;
}catch(const std::exception& e){if(window)DestroyWindow(window);fprintf(stderr,"%s\n",e.what());return 1;}}
