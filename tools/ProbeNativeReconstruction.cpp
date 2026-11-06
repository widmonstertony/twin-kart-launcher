// Explicit synthetic offscreen fixture of the production qualified-resource
// path. No game/client/shader identity is impersonated. The default is
// offscreen; optional frame generation uses an explicit standalone window.
#include "../native/NativeNeuralPass.cpp"
#include <vector>
#include <stdexcept>
#include <io.h>
#include <thread>
#include <atomic>
#include <algorithm>
#include "../native/NativeDeviceIdentity.h"
#include "../native/NativeHudAlphaShader.h"
using namespace TwinKartGraphics;
namespace {
bool guidesOnlyFixture=false;
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void Check(HRESULT value,const char* message){Need(SUCCEEDED(value),message);}
TwinKartNeuralState VerifyNeuralState(unsigned expected,unsigned width,unsigned height) {
    TwinKartNeuralState chain{sizeof(TwinKartNeuralState)};
    Need(runtime && runtime->readState(&chain) && chain.initialized,"Current reconstruction consumer state unavailable");
    if(neuralEnabled) {
        Need(chain.featureCreated && chain.width==width && chain.height==height &&
            chain.requestedPasses==requestedNrPasses && chain.style==requestedNrStyle &&
            chain.passResults[0]==1 && chain.passEvaluations[0]==expected &&
            (requestedNrPasses==1 || (chain.passResults[1]==1 && chain.passEvaluations[1]==expected)),
            "Independent NR passes did not run on the real low-resolution input before SR");
    } else {
        Need(!chain.featureCreated && !chain.privateProvider && !chain.evaluations &&
            !chain.passEvaluations[0] && !chain.passEvaluations[1] && !GetModuleHandleW(L"nvngx_dlssnr.dll"),
            "NR Off created/evaluated an NR feature or loaded its private model");
    }
    using ReadSuper=decltype(&TwinKartSuperResolutionGetState);
    auto readSuper=reinterpret_cast<ReadSuper>(GetProcAddress(runtime->module,"TwinKartSuperResolutionGetState"));
    TwinKartSuperResolutionState sr{sizeof(TwinKartSuperResolutionState)};
    Need(readSuper && readSuper(&sr),"Independent SR state unavailable");
    if(guidesOnlyFixture)Need(!sr.featureCreated && !sr.evaluations,"Guide-only path created/evaluated an SR feature");
    else Need(sr.featureCreated && sr.evaluateResult==1 && sr.evaluations==expected &&
        sr.inputWidth==width && sr.inputHeight==height && sr.outputWidth==runtime->width && sr.outputHeight==runtime->height,
        "Actual independent SR consumer did not evaluate the current input/output extent");
    return chain;
}
void Record(const char* line){fputs(line,stdout);fflush(stdout);}
ComPtr<ID3DBlob> Compile(const char* source,const char* target) {
    ComPtr<ID3DBlob> code,errors;
    Check(D3DCompile(source,strlen(source),nullptr,nullptr,nullptr,"main",target,D3DCOMPILE_ENABLE_STRICTNESS,0,&code,&errors),"First-party fixture shader compile");return code;
}
float ReadHalfMotion(ID3D11DeviceContext* context,ID3D11Device* device,ID3D11Texture2D* texture,unsigned width,unsigned height) {
    D3D11_TEXTURE2D_DESC desc{};texture->GetDesc(&desc);
    Need(desc.Format==DXGI_FORMAT_R16G16_FLOAT && desc.Width==width && desc.Height==height,"Dynamic converted motion extent/format mismatch");
    desc.Width=desc.Height=1;desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=desc.MiscFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> pixel;Check(device->CreateTexture2D(&desc,nullptr,&pixel),"Dynamic motion staging pixel");
    D3D11_BOX box{width/2,height/2,0,width/2+1,height/2+1,1};context->CopySubresourceRegion(pixel.Get(),0,0,0,0,texture,0,&box);
    D3D11_MAPPED_SUBRESOURCE mapped{};Check(context->Map(pixel.Get(),0,D3D11_MAP_READ,0,&mapped),"Actual converted motion GPU readback");
    unsigned short values[2];memcpy(values,mapped.pData,sizeof(values));context->Unmap(pixel.Get(),0);
    auto half=[](unsigned bits){unsigned exponent=(bits>>10)&31,mantissa=bits&1023;
        Need(exponent!=31,"Nonfinite converted dense motion");
        float value=exponent?std::ldexp(1.0f+mantissa/1024.0f,static_cast<int>(exponent)-15):std::ldexp(static_cast<float>(mantissa),-24);
        return bits&0x8000?-value:value;};
    Need(fabsf(half(values[1]))<0.02f,"Static vertical axis acquired spurious motion");return half(values[0]);
}
#ifdef TWIN_KART_RECONSTRUCTION_FG
class SyntheticSimulation {
    HANDLE request=nullptr,ready=nullptr;
    std::thread worker;
    std::atomic<bool> stopping{false};std::atomic<unsigned long long> pending{0};
    std::atomic<DWORD> thread{0};
public:
    ~SyntheticSimulation(){Stop();}
    void Stop(){
        stopping=true;if(request)SetEvent(request);if(worker.joinable())worker.join();
        if(request)CloseHandle(request);if(ready)CloseHandle(ready);request=ready=nullptr;
    }
    void Start(){
        request=CreateEventW(nullptr,FALSE,FALSE,nullptr);ready=CreateEventW(nullptr,FALSE,FALSE,nullptr);
        Need(request && ready,"Synthetic native simulation events");
        worker=std::thread([this]{
            thread=GetCurrentThreadId();
            while(WaitForSingleObject(request,INFINITE)==WAIT_OBJECT_0 && !stopping.load()) {
                auto id=pending.load();ObserveSyntheticNativeMarker(FrameStage::SimulationStart,id);
                ObserveSyntheticNativeMarker(FrameStage::SimulationEnd,id);SetEvent(ready);
            }
        });
    }
    void Submit(unsigned long long id){
        pending=id;Need(SetEvent(request)!=FALSE && WaitForSingleObject(ready,10000)==WAIT_OBJECT_0,"Synthetic native simulation/token/pacing timeout");
        Need(thread.load()!=GetCurrentThreadId(),"Synthetic simulation accidentally ran on render thread");
    }
    DWORD Thread()const{return thread.load();}
};
class PresentationProbe {
    FrameGeneration generation;
    SyntheticSimulation simulation;
    ComPtr<IDXGIFactory2> factory;ComPtr<ID3D12Device> device;ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> commands;ComPtr<ID3D12Fence> fence;
    ComPtr<IDXGISwapChain4> display;
    SharedTextureBridge depth,motion,color,alpha;
    ComPtr<ID3D11Texture2D> noUi;ComPtr<ID3D11UnorderedAccessView> noUiWrite;
    HWND window=nullptr,previous=nullptr;unsigned long long fenceValue=0;
    bool registered=false;
    bool nativeMarkers=false;unsigned long long nativeId=0;
    void Wait() {
        Check(queue->Signal(fence.Get(),++fenceValue),"Combined fixture queue signal");
        HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);Need(event!=nullptr,"Combined fixture event");
        HRESULT set=fence->SetEventOnCompletion(fenceValue,event);DWORD result=SUCCEEDED(set)?WaitForSingleObject(event,10000):WAIT_FAILED;
        CloseHandle(event);Need(result==WAIT_OBJECT_0,"Combined fixture GPU completion");
    }
public:
    inline static PresentationProbe* active=nullptr;
    unsigned generated=0,rendered=0;unsigned long long presented=0;
    bool enabled=false;
    PresentationProbe(){active=this;}
    static bool OwnsCurrentNgxDevice(ID3D12Device* selected){return active && SameNativeGraphicsDevice(active->device.Get(),selected);}
    ~PresentationProbe() {
        simulation.Stop();generation.Shutdown();bool restore=GetForegroundWindow()==window;
        if(IsWindow(window))DestroyWindow(window);if(restore && IsWindow(previous))SetForegroundWindow(previous);
        if(registered)UnregisterClassW(L"TwinKartCombinedGraphicsFixture",GetModuleHandleW(nullptr));
        active=nullptr;
    }
    void Load(const wchar_t* externalRuntime,const wchar_t* logs,bool nativeFixture) {
        nativeMarkers=nativeFixture;
        Need(generation.Load(externalRuntime,logs)==0,"Combined Streamline initialization before graphics APIs");
        using Factory=HRESULT(WINAPI*)(REFIID,void**);auto create=reinterpret_cast<Factory>(generation.GraphicsEntry("CreateDXGIFactory1"));
        Need(create!=nullptr,"Combined Streamline factory entry");Check(create(IID_PPV_ARGS(&factory)),"Combined Streamline factory");enabled=true;
    }
    void Initialize(ID3D11Device* source,IDXGIAdapter* adapter) {
        using Device=HRESULT(WINAPI*)(IUnknown*,D3D_FEATURE_LEVEL,REFIID,void**);auto create=reinterpret_cast<Device>(generation.GraphicsEntry("D3D12CreateDevice"));
        Need(create!=nullptr,"Combined Streamline device entry");Check(create(adapter,D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)),"Combined Streamline device");
        Need(generation.BindDevice(device.Get())==0 && generation.Configure(2,1280,720,DXGI_FORMAT_R10G10B10A2_UNORM,2)==0,"Combined FG configure");
        if(nativeMarkers){Need(generation.AttachNativeMarkers(),"Synthetic native marker sink binding");simulation.Start();}
        D3D12_COMMAND_QUEUE_DESC q{};Check(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)),"Combined presentation queue");
        Check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)),"Combined command allocator");
        Check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&commands)),"Combined commands");Check(commands->Close(),"Combined initial command close");
        Check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"Combined completion fence");
        WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"TwinKartCombinedGraphicsFixture";
        Need(RegisterClassW(&wc)!=0,"Combined synthetic window registration");registered=true;previous=GetForegroundWindow();
        window=CreateWindowExW(WS_EX_APPWINDOW,wc.lpszClassName,L"Twin Kart SR + NR + FG GPU fixture (synthetic, no game)",WS_OVERLAPPEDWINDOW,80,80,1280,720,nullptr,nullptr,wc.hInstance,nullptr);
        Need(window!=nullptr,"Combined synthetic window");
        DXGI_SWAP_CHAIN_DESC1 desc{};desc.Width=1280;desc.Height=720;desc.Format=DXGI_FORMAT_R10G10B10A2_UNORM;desc.SampleDesc.Count=1;
        desc.BufferCount=2;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
        ComPtr<IDXGISwapChain1> chain;Check(factory->CreateSwapChainForHwnd(queue.Get(),window,&desc,nullptr,nullptr,&chain),"Combined real FG display");Check(chain.As(&display),"Combined flip chain");
        Check(depth.Initialize(source,device.Get(),queue.Get(),1280,720,DXGI_FORMAT_R32_FLOAT),"Combined depth bridge");
        Check(motion.Initialize(source,device.Get(),queue.Get(),1280,720,DXGI_FORMAT_R16G16_FLOAT),"Combined motion bridge");
        Check(color.Initialize(source,device.Get(),queue.Get(),1280,720,DXGI_FORMAT_R10G10B10A2_UNORM),"Combined NR output bridge");
        Check(alpha.Initialize(source,device.Get(),queue.Get(),1280,720,DXGI_FORMAT_R8_UNORM),"Combined empty UI bridge");
        D3D11_TEXTURE2D_DESC ui{};ui.Width=1280;ui.Height=720;ui.MipLevels=ui.ArraySize=ui.SampleDesc.Count=1;ui.Format=DXGI_FORMAT_R8_UNORM;ui.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
        Check(source->CreateTexture2D(&ui,nullptr,&noUi),"Combined no-UI alpha");Check(source->CreateUnorderedAccessView(noUi.Get(),nullptr,&noUiWrite),"Combined alpha clear view");
        ComPtr<ID3D11DeviceContext> context;source->GetImmediateContext(&context);const unsigned zero[4]{};context->ClearUnorderedAccessViewUint(noUiWrite.Get(),zero);
        ShowWindow(window,SW_SHOWNORMAL);UpdateWindow(window);SetForegroundWindow(window);
    }
    void Begin(unsigned frame,const CameraFrame& camera) {
        MSG m;while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}
        if(frame==0 && IsWindow(window) && GetForegroundWindow()!=window) {
            // Initial activation can be queued while device/shader setup is
            // completing. Allow that one startup request to settle; never
            // reclaim focus later if the user switches to another window.
            ShowWindow(window,SW_SHOWNORMAL);SetForegroundWindow(window);
            // Bounded startup grace also allows the desktop automation client
            // to activate this explicitly synthetic window. No keys or focus
            // bypasses are injected by the probe itself.
            for(unsigned attempt=0;attempt<1200 && GetForegroundWindow()!=window;++attempt) {
                Sleep(25);while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);}
            }
        }
        Need(IsWindow(window) && GetForegroundWindow()==window,"Combined fixture lost focus; no generation success inferred");
        Sleep(16);
        if(nativeMarkers) {
            nativeId=0x100000000ull+frame;simulation.Submit(nativeId);
            ObserveSyntheticNativeMarker(FrameStage::RenderSubmitStart,nativeId);
            if(!frame)printf("{\"event\":\"synthetic_native_marker_threads\",\"simulation_thread\":%lu,\"render_thread\":%lu,\"first_frame_id\":%llu,\"game_started\":false}\n",simulation.Thread(),GetCurrentThreadId(),nativeId);
        } else Need(generation.BeginFrame(frame,camera)==0 && generation.Marker(FrameStage::SimulationStart)==0,"Combined synthetic frame markers");
    }
    void Present(const NativePresentationInputs& input) {
        bool qualified=true;
        if(nativeMarkers) {
            Need(input.nativeFrameObserved && input.nativeFrame==nativeId,"Production reconstruction lost synthetic native frame ID");
            ObserveSyntheticNativeMarker(FrameStage::RenderSubmitEnd,nativeId);ObserveSyntheticNativeMarker(FrameStage::PresentStart,nativeId);
            auto result=generation.BeginNativeFrame(nativeId,input.camera);
            if(result) {
                auto ticket=generation.InspectNativeFrame(nativeId);
                printf("{\"event\":\"synthetic_native_begin_frame_fallback\",\"result\":%u,\"native_id\":%llu,\"seen\":%u,\"completed\":%u,\"has_token\":%s,\"failed\":%s,\"age_ms\":%llu,\"game_started\":false}\n",result,nativeId,ticket.seen,ticket.completed,ticket.hasToken?"true":"false",ticket.failed?"true":"false",GetTickCount64()-ticket.time);
                Need(result==0xf2000003,"Actual provider rejected production native-frame camera interface");
                qualified=false;
            }
        } else Need(generation.Marker(FrameStage::SimulationEnd)==0,"Combined simulation-end marker");
        Check(depth.CopyTo12(input.depth),"Combined depth transfer");Check(motion.CopyTo12(input.motion),"Combined motion transfer");
        Check(color.CopyTo12(input.hudless),"Combined reconstructed color transfer");Check(alpha.CopyTo12(noUi.Get()),"Combined alpha transfer");
        Check(allocator->Reset(),"Combined allocator reset");Check(commands->Reset(allocator.Get(),nullptr),"Combined command reset");
        ID3D12Resource* inputs[]{depth.Texture12(),motion.Texture12(),color.Texture12(),alpha.Texture12()};
        for(auto p:inputs)Transition(commands.Get(),p,D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        GenerationResources tags{commands.Get(),inputs[0],inputs[1],inputs[2],inputs[3],D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,true,true};
        if(qualified)Need(generation.Tag(tags)==0,"Combined actual NR output/guide tags");
        Need(generation.SetActive(qualified)==0,"Combined final generation mode commit");
        ComPtr<ID3D12Resource> backbuffer;Check(display->GetBuffer(display->GetCurrentBackBufferIndex(),IID_PPV_ARGS(&backbuffer)),"Combined display buffer");
        Transition(commands.Get(),inputs[2],D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE);
        Transition(commands.Get(),backbuffer.Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_DEST);commands->CopyResource(backbuffer.Get(),inputs[2]);
        Transition(commands.Get(),backbuffer.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PRESENT);
        Transition(commands.Get(),inputs[2],D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        Check(commands->Close(),"Combined render close");if(!nativeMarkers)Need(generation.Marker(FrameStage::RenderSubmitStart)==0,"Combined render-start marker");
        ID3D12CommandList* lists[]{commands.Get()};queue->ExecuteCommandLists(1,lists);
        if(!nativeMarkers)Need(generation.Marker(FrameStage::RenderSubmitEnd)==0 && generation.Marker(FrameStage::PresentStart)==0,"Combined render/present markers");
        Check(display->Present(0,0),"Combined real presentation");
        if(nativeMarkers)ObserveSyntheticNativeMarker(FrameStage::PresentEnd,nativeId);else Need(generation.Marker(FrameStage::PresentEnd)==0,"Combined present-end marker");
        auto state=generation.Presented();Need(state.result==0,"Combined generation state unavailable");
        if(state.inputCompletionFence && state.inputCompletionValue)Check(queue->Wait(state.inputCompletionFence,state.inputCompletionValue),"Combined provider input fence");
        Wait();Check(allocator->Reset(),"Combined return allocator");Check(commands->Reset(allocator.Get(),nullptr),"Combined return commands");
        for(auto p:inputs)Transition(commands.Get(),p,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COMMON);
        Check(commands->Close(),"Combined return close");queue->ExecuteCommandLists(1,lists);Wait();
        Check(depth.ReturnTo11(nullptr),"Combined depth release");Check(motion.ReturnTo11(nullptr),"Combined motion release");Check(color.ReturnTo11(nullptr),"Combined color release");Check(alpha.ReturnTo11(nullptr),"Combined alpha release");
        ++rendered;presented+=state.lastPresented;if(state.lastPresented>=2)++generated;
        if(rendered%30==0)printf("{\"event\":\"synthetic_sr_nr_fg_present\",\"rendered\":%u,\"actually_presented\":%llu,\"frames_reporting_generation\":%u,\"status\":%u,\"game_started\":false}\n",rendered,presented,generated,state.status);
    }
};
#endif
}
int wmain(int argc,wchar_t** argv){try{
    // The desktop automation launcher accepts an executable path, not command
    // arguments. Its explicit private INI supplies only this synthetic probe's
    // external providers/cache/report; no game path or process injection exists.
    std::vector<std::wstring> desktopArguments;std::vector<wchar_t*> desktopPointers;
    if(argc==1) {
        wchar_t executable[32768]{};DWORD length=GetModuleFileNameW(nullptr,executable,32768);
        Need(length && length<32768,"Probe executable path unavailable");std::wstring path=executable;
        auto slash=path.find_last_of(L"\\/");Need(slash!=std::wstring::npos,"Probe directory unavailable");
        std::wstring ini=path.substr(0,slash+1)+L"ProbeNativeReconstruction.local.ini";
        auto field=[&](const wchar_t* key){wchar_t value[32768]{};DWORD size=GetPrivateProfileStringW(L"NativeReconstructionProbe",key,L"",value,32768,ini.c_str());
            Need(size && size<32767,"Missing/bounded private desktop probe configuration");return std::wstring(value);};
        auto reportPath=field(L"Report");Need(GetFileAttributesW(reportPath.c_str())==INVALID_FILE_ATTRIBUTES,"Refuse overwriting an existing probe report");
        FILE* file=nullptr;Need(_wfreopen_s(&file,reportPath.c_str(),L"w",stdout)==0 && file,"Private probe report creation");
        // A desktop launcher may supply no CRT stderr descriptor. The report
        // itself uses stdout; stderr duplication is best effort, never a reason
        // to abort before reporting a useful initialization error.
        setvbuf(stdout,nullptr,_IONBF,0);_dup2(_fileno(stdout),2);
        HANDLE reportHandle=reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(stdout)));
        SetStdHandle(STD_OUTPUT_HANDLE,reportHandle);SetStdHandle(STD_ERROR_HANDLE,reportHandle);
        puts("{\"event\":\"desktop_probe_configuration_ready\",\"game_started\":false}");
        // A desktop launch need not inherit a shell's environment. Preserve
        // the independently reviewed pin in the private INI; never derive an
        // expected hash from the module that is about to be loaded.
        auto consumerPin=field(L"OriginalConsumerSHA256");
        Need(SetEnvironmentVariableW(L"TWIN_KART_NEURAL_MODULE_SHA256",consumerPin.c_str())!=FALSE,"Desktop consumer pin unavailable");
        auto passes=GetPrivateProfileIntW(L"NativeReconstructionProbe",L"NrPasses",1,ini.c_str());
        auto style=GetPrivateProfileIntW(L"NativeReconstructionProbe",L"NrStyle",0,ini.c_str());
        Need(passes>=1 && passes<=2 && style<=2,"Invalid desktop NR pass/style configuration");
        wchar_t passText[]{static_cast<wchar_t>(L'0'+passes),0},styleText[]{static_cast<wchar_t>(L'0'+style),0};
        Need(SetEnvironmentVariableW(L"TWIN_KART_NR_PASSES",passText)!=FALSE &&
            SetEnvironmentVariableW(L"TWIN_KART_NR_STYLE",styleText)!=FALSE,"Desktop NR configuration unavailable");
        desktopArguments.push_back(path);
        for(auto key:{L"OriginalConsumer",L"NeuralProvider",L"Cache",L"StreamlineProvider",L"Logs"})desktopArguments.push_back(field(key));
        if(GetPrivateProfileIntW(L"NativeReconstructionProbe",L"NativeMarkers",0,ini.c_str())==1)desktopArguments.push_back(L"--native-markers");
        for(auto& value:desktopArguments)desktopPointers.push_back(value.data());
        argc=static_cast<int>(desktopPointers.size());argv=desktopPointers.data();
    }
    guidesOnlyFixture=argc==5 && !wcscmp(argv[4],L"--guides-only");
    if(guidesOnlyFixture)argc=4;
    bool nativeFixture=argc==7 && !wcscmp(argv[6],L"--native-markers");
    Need(argc==4 || argc==6 || nativeFixture,"Usage: ProbeNativeReconstruction original-consumer.dll external-provider-directory private-cache-directory [--guides-only | Streamline-directory private-log-directory [--native-markers]]");
#ifdef TWIN_KART_RECONSTRUCTION_FG
    PresentationProbe presentation;if(argc>=6)presentation.Load(argv[4],argv[5],nativeFixture);
#else
    Need(argc==4,"Build with external Streamline headers for the optional focused combined fixture");
#endif
    SetEnvironmentVariableW(L"TWIN_KART_SUPER_RESOLUTION",guidesOnlyFixture?L"0":L"1");
    if(guidesOnlyFixture)SetEnvironmentVariableW(L"TWIN_KART_NEURAL_RENDERING",L"0");
    wchar_t expected[65]{};char moduleSha[65]{};
    DWORD hashLength=GetEnvironmentVariableW(L"TWIN_KART_NEURAL_MODULE_SHA256",expected,ARRAYSIZE(expected));
    if(hashLength==64)for(unsigned i=0;i<64;++i)moduleSha[i]=expected[i]<128?static_cast<char>(expected[i]):'!';
    NativeNeuralCoreOwner coreOwner=nullptr;
#ifdef TWIN_KART_RECONSTRUCTION_FG
    if(presentation.enabled)coreOwner=PresentationProbe::OwnsCurrentNgxDevice;
#endif
    Need(ConfigureNativeNeuralPass(argv[1],moduleSha,argv[2],argv[3],Record,coreOwner),"Manifest-pinned original consumer hash/configuration rejected");
    ComPtr<IDXGIFactory1> factory;Check(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"DXGI factory");
    ComPtr<IDXGIAdapter1> adapter;
    for(UINT index=0;;++index) {
        ComPtr<IDXGIAdapter1> candidate;if(factory->EnumAdapters1(index,&candidate)==DXGI_ERROR_NOT_FOUND)break;
        DXGI_ADAPTER_DESC1 desc{};candidate->GetDesc1(&desc);
        if(desc.VendorId==0x10de && !(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)){adapter=candidate;break;}
    }
    Need(adapter!=nullptr,"NVIDIA adapter required; no WARP success substituted");
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Check(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context),"Actual NVIDIA D3D11 device");
#ifdef TWIN_KART_RECONSTRUCTION_FG
    if(presentation.enabled)presentation.Initialize(device.Get(),adapter.Get());
#endif
    constexpr unsigned iw=640,ih=360,aw=768,ah=416,ow=1280,oh=720;
    D3D11_TEXTURE2D_DESC desc{};desc.Width=aw;desc.Height=ah;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;
    desc.Format=DXGI_FORMAT_R32G8X24_TYPELESS;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    struct DepthPixel{float z;unsigned stencil;};std::vector<DepthPixel> depths(aw*ah,{0.5f,0});
    D3D11_SUBRESOURCE_DATA data{depths.data(),aw*sizeof(DepthPixel),0};ComPtr<ID3D11Texture2D> depth;
    Check(device->CreateTexture2D(&desc,&data,&depth),"Synthetic padded depth");
    D3D11_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;srv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;srv.Texture2D.MipLevels=1;
    ComPtr<ID3D11ShaderResourceView> depthView,velocityView;
    Check(device->CreateShaderResourceView(depth.Get(),&srv,&depthView),"Synthetic depth view");
    desc.Width=704;desc.Height=400;desc.Format=DXGI_FORMAT_R16G16B16A16_UNORM;
    std::vector<unsigned long long> motion(desc.Width*desc.Height,0);data={motion.data(),desc.Width*8,0};ComPtr<ID3D11Texture2D> velocity;
    Check(device->CreateTexture2D(&desc,&data,&velocity),"Synthetic independently padded velocity");
    Check(device->CreateShaderResourceView(velocity.Get(),nullptr,&velocityView),"Synthetic velocity view");
    desc.Width=aw;desc.Height=ah;desc.Format=DXGI_FORMAT_R8G8B8A8_UNORM;desc.BindFlags=D3D11_BIND_RENDER_TARGET|D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> color;ComPtr<ID3D11RenderTargetView> colorWrite;ComPtr<ID3D11ShaderResourceView> colorRead;
    Check(device->CreateTexture2D(&desc,nullptr,&color),"Pooled synthetic color");
    Check(device->CreateRenderTargetView(color.Get(),nullptr,&colorWrite),"Color producer target");Check(device->CreateShaderResourceView(color.Get(),nullptr,&colorRead),"Color producer input");
    desc.Width=ow;desc.Height=oh;desc.Format=DXGI_FORMAT_R10G10B10A2_UNORM;
    ComPtr<ID3D11Texture2D> output;ComPtr<ID3D11RenderTargetView> outputWrite;
    Check(device->CreateTexture2D(&desc,nullptr,&output),"Synthetic final pre-HUD target");Check(device->CreateRenderTargetView(output.Get(),nullptr,&outputWrite),"Synthetic output target");
    const char vs[]= "float4 main(uint id:SV_VertexID):SV_Position{float2 p[3]={float2(-1,-1),float2(-1,3),float2(3,-1)};return float4(p[id],0,1);}";
    const char producer[]= "cbuffer FixtureMotion:register(b0){float pixelShift;float3 pad;}float4 main(float4 p:SV_Position):SV_Target{float x=frac((p.x-16-pixelShift)/640);return float4(x,(p.y-8)/360,((uint(x*640)/4+uint(p.y)/4)%2)*0.7+0.1,1);}";
    const char composite[]= "Texture2D<float4> scene:register(t0);float4 main(float4 p:SV_Position):SV_Target{return scene.Load(int3(uint2(p.xy*0.5)+uint2(16,8),0));}";
    auto vertexCode=Compile(vs,"vs_5_0"),producerCode=Compile(producer,"ps_5_0"),compositeCode=Compile(composite,"ps_5_0");
    ComPtr<ID3D11VertexShader> vertex;ComPtr<ID3D11PixelShader> produce,compose;
    Check(device->CreateVertexShader(vertexCode->GetBufferPointer(),vertexCode->GetBufferSize(),nullptr,&vertex),"Fixture vertex shader");
    Check(device->CreatePixelShader(producerCode->GetBufferPointer(),producerCode->GetBufferSize(),nullptr,&produce),"Fixture producer shader");
    Check(device->CreatePixelShader(compositeCode->GetBufferPointer(),compositeCode->GetBufferSize(),nullptr,&compose),"Fixture composite shader");
    auto uiCode=Compile(NativeHudAlphaShader,"cs_5_0");ComPtr<ID3D11ComputeShader> uiShader;
    Check(device->CreateComputeShader(uiCode->GetBufferPointer(),uiCode->GetBufferSize(),nullptr,&uiShader),"Production UI alpha shader");
    float parameters[1024]{},view[2048]{};NativeRect inclusive{8,16,647,375};memcpy(parameters+80,&inclusive,16);
    parameters[96]=8;parameters[97]=16;parameters[98]=648;parameters[99]=376;
    parameters[60]=static_cast<float>(iw)/672;parameters[61]=static_cast<float>(ih)/384;parameters[62]=8.0f/672;parameters[63]=16.0f/384;
    parameters[76]=672;parameters[77]=384;parameters[78]=1.0f/672;parameters[79]=1.0f/384;
    parameters[92]=iw;parameters[93]=ih;parameters[94]=1.0f/iw;parameters[95]=1.0f/ih;
    float projection[16]={1.125f,0,0,0,0,2,0,0,0,0,0,1,0,0,10,0};
    memcpy(view+32,projection,64);memcpy(view+128,projection,64);memcpy(view+144,projection,64);memcpy(view+316,projection,64);
    view[504]=view[509]=view[514]=view[519]=1;view[536]=iw;view[537]=ih;view[538]=1.0f/iw;view[539]=1.0f/ih;view[557]=view[558]=1;
    D3D11_BUFFER_DESC cb{};cb.ByteWidth=sizeof(parameters);cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    data={parameters,0,0};ComPtr<ID3D11Buffer> parameterBuffer,viewBuffer;Check(device->CreateBuffer(&cb,&data,&parameterBuffer),"Synthetic native-layout parameters");
    cb.ByteWidth=sizeof(view);data.pSysMem=view;Check(device->CreateBuffer(&cb,&data,&viewBuffer),"Synthetic native-layout View");
    cb.ByteWidth=16;ComPtr<ID3D11Buffer> motionParameters;Check(device->CreateBuffer(&cb,nullptr,&motionParameters),"Dynamic synthetic color parameters");
    auto updateMovingInput=[&](unsigned frameIndex) {
        float shift[4]={frameIndex*2.0f,0,0,0};context->UpdateSubresource(motionParameters.Get(),0,nullptr,shift,0,0);
        ID3D11Buffer* parameters=motionParameters.Get();context->PSSetConstantBuffers(0,1,&parameters);
        // Native encoded object velocity: current minus previous NDC. The
        // production shader decodes it to previous-minus-current pixel motion.
        unsigned short x=static_cast<unsigned short>(32767+std::lround((4.0f/iw)*(0.499f*0.5f)*65535));
        unsigned long long encoded=x|(32767ull<<16);std::fill(motion.begin(),motion.end(),encoded);
        context->UpdateSubresource(velocity.Get(),0,nullptr,motion.data(),704*8,0);
    };
    unsigned changed[3]{};unsigned long long hashes[3]{};
    const unsigned frameCount=argc>=6?120:5;
    unsigned epochStart=0,sharedCoreRebuilds=0;
    for(unsigned index=0;index<frameCount;++index) {
#ifdef TWIN_KART_RECONSTRUCTION_FG
        if(presentation.enabled && index==60) {
            Need(runtime && runtime->borrowedCore,"Combined NR did not borrow the exact presentation NGX device");
            Need(RetireRuntime(),"Shared-core reconstruction retirement failed");epochStart=index;++sharedCoreRebuilds;
            puts("{\"event\":\"synthetic_shared_ngx_core_preserved_for_fg\",\"after_rendered_frames\":60,\"game_started\":false}");
        }
#endif
        unsigned outputWidth=argc==4 && index==3?960:ow,outputHeight=argc==4 && index==3?540:oh;
        ComPtr<ID3D11Texture2D> frameOutput=output;ComPtr<ID3D11RenderTargetView> frameWrite=outputWrite;
        if(outputWidth!=ow || outputHeight!=oh) {
            D3D11_TEXTURE2D_DESC resized{};output->GetDesc(&resized);resized.Width=outputWidth;resized.Height=outputHeight;
            frameOutput.Reset();frameWrite.Reset();Check(device->CreateTexture2D(&resized,nullptr,&frameOutput),"Owned resize target");
            Check(device->CreateRenderTargetView(frameOutput.Get(),nullptr,&frameWrite),"Owned resize output view");
        }
#ifdef TWIN_KART_RECONSTRUCTION_FG
        if(presentation.enabled) {
            CameraFrame camera{};Need(DecodeNativeCamera(view,iw,ih,index<2,camera),"Synthetic current presentation camera");
            presentation.Begin(index,ResolvedPresentationCamera(camera,ow,oh));
        }
#endif
        unsigned frame=120+index;context->ClearState();context->VSSetShader(vertex.Get(),nullptr,0);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        updateMovingInput(index);
        ID3D11RenderTargetView* target=colorWrite.Get();context->OMSetRenderTargets(1,&target,nullptr);context->PSSetShader(produce.Get(),nullptr,0);
        D3D11_VIEWPORT viewport{16,8,iw,ih,0,1};context->RSSetViewports(1,&viewport);context->Draw(3,0);
        RecordNativeSceneDraw(context.Get(),frame);NativeRect observed;
        Need(NativeSceneRectangle(color.Get(),context.Get(),frame,iw,ih,observed) && observed.left==16 && observed.top==8,"Actual producer crop unavailable");
        Need(SnapshotNativeGuides(context.Get(),parameterBuffer.Get(),viewBuffer.Get(),aw,ah,aw,ah,nativeLayout,nativeView),"Immutable qualified synthetic guide snapshot");
        nativeDepth=depthView;nativeVelocity=velocityView;guideFrame=frame;
        target=frameWrite.Get();context->OMSetRenderTargets(1,&target,nullptr);ID3D11ShaderResourceView* scene=colorRead.Get();context->PSSetShaderResources(0,1,&scene);context->PSSetShader(compose.Get(),nullptr,0);
        viewport={0,0,static_cast<float>(outputWidth),static_cast<float>(outputHeight),0,1};context->RSSetViewports(1,&viewport);context->Draw(3,0);
        unsigned before[1024]{},after[1024]{};bool readback=index<3 || argc==4;if(readback)Need(ReadPatch(context.Get(),device.Get(),frameOutput.Get(),before),"Baseline GPU readback");
        // The game entry must reject this first-party shader. Only the explicit
        // fixture invokes qualified processing; exact native identity is intact.
        Need(!ProcessNativeNeuralPixelPass(context.Get(),frame),"Synthetic shader incorrectly passed the native identity guard");
        Need(ProcessQualifiedNeuralPixelPass(context.Get(),frame),"Production low-resolution NR-before-SR reconstruction failed");
        unsigned expected=argc==4 && index>=3?1:index-epochStart+1;
        Need(runtime && runtime->inputWidth==(guidesOnlyFixture?0:iw) && runtime->inputHeight==(guidesOnlyFixture?0:ih) && runtime->width==outputWidth && runtime->height==outputHeight &&
            runtime->superEvaluations==(guidesOnlyFixture?0:expected) && runtime->evaluations==(neuralEnabled?expected:0) &&
            NativeSuperResolutionOutputMatches(frameOutput.Get(),frame)==!guidesOnlyFixture,"Actual configured consumers or resized target correlation missing");
        Need(NativeNeuralOutputMatches(frameOutput.Get(),frame)==neuralEnabled,"NR Off output was falsely reported as an NR evaluation");
        Need(runtime->borrowedCore==(coreOwner!=nullptr),"Production adapter did not select the correct NGX core lifetime");
        Need(runtime->lowResolutionNeural==!guidesOnlyFixture,"Actual NR/SR ordering differs from the requested mode");
        VerifyNeuralState(expected,iw,ih);
        NativePresentationInputs exported;
        Need(!GetNativePresentationInputs(frame+1,exported),"Presentation accepted stale guides for an unevaluated frame");
        Need(GetNativePresentationInputs(frame,exported) && exported.hudless==runtime->colorBridge.Texture11(),
            "Presentation did not export the current full-resolution SR output");
        for(auto texture:{exported.depth,exported.motion,exported.hudless}) {
            D3D11_TEXTURE2D_DESC extent{};texture->GetDesc(&extent);
            Need(extent.Width==outputWidth && extent.Height==outputHeight,"Presentation guides retained low-resolution NR dimensions");
        }
        float srMotionX=guidesOnlyFixture?0:ReadHalfMotion(context.Get(),device.Get(),runtime->srMotion.Get(),iw,ih);
        float nrMotionX=ReadHalfMotion(context.Get(),device.Get(),runtime->motion.Get(),outputWidth,outputHeight);
        Need((guidesOnlyFixture || fabsf(srMotionX+2)<0.03f) && fabsf(nrMotionX+2.0f*outputWidth/iw)<0.05f,"Moving source/object guides lost native scale or direction");
        if(index<3 || argc==4)printf("{\"event\":\"synthetic_dense_motion_gpu_readback\",\"frame\":%u,\"source_shift_pixels\":%u,\"sr_motion_x\":%.6g,\"nr_motion_x\":%.6g,\"expected_sr_motion_x\":%d,\"moving_synthetic_inputs\":true,\"game_started\":false,\"quality_verified\":false}\n",frame,index*2,srMotionX,nrMotionX,guidesOnlyFixture?0:-2);
        ComPtr<ID3D11PixelShader> restoredShader;ComPtr<ID3D11ShaderResourceView> restoredScene;ComPtr<ID3D11RenderTargetView> restoredTarget;
        context->PSGetShader(&restoredShader,nullptr,nullptr);context->PSGetShaderResources(0,1,&restoredScene);context->OMGetRenderTargets(1,&restoredTarget,nullptr);
        Need(restoredShader.Get()==compose.Get() && restoredScene.Get()==scene && restoredTarget.Get()==target,"Production reconstruction changed the native rendering state");
        if(readback) {
            Need(ReadPatch(context.Get(),device.Get(),frameOutput.Get(),after),"Combined GPU output readback");unsigned differences=0;unsigned long long hash=14695981039346656037ull;
            for(unsigned pixel=0;pixel<1024;++pixel){if(before[pixel]!=after[pixel])++differences;hash=(hash^after[pixel])*1099511628211ull;}
            Need(guidesOnlyFixture?differences==0:differences>0,"Reconstruction change/guide-only preservation contract failed");
            unsigned retained[1024]{};
            Need(ReadPatch(context.Get(),device.Get(),exported.hudless,retained) &&
                memcmp(retained,after,sizeof(retained))==0,"Presentation HUDless pixels differ from the actual pre-UI target");
            if(argc==4) {
                // A late UI draw must affect the final target, never the retained
                // pre-UI image that FG compares against it. Use a centre patch
                // instead of touching bridge ownership or relaxing its contract.
                ComPtr<ID3D11DeviceContext1> context1;Check(context.As(&context1),"Late synthetic UI context");
                D3D11_RECT rectangle{static_cast<LONG>(outputWidth/2-16),static_cast<LONG>(outputHeight/2-16),
                    static_cast<LONG>(outputWidth/2+16),static_cast<LONG>(outputHeight/2+16)};
                const float uiColor[4]={1,0,1,1};context1->ClearView(frameWrite.Get(),uiColor,&rectangle,1);
                unsigned ui[1024]{},retainedAfter[1024]{};
                Need(ReadPatch(context.Get(),device.Get(),frameOutput.Get(),ui) && memcmp(ui,after,sizeof(ui))!=0,
                    "Late synthetic UI did not change the actual final target");
                Need(ReadPatch(context.Get(),device.Get(),exported.hudless,retainedAfter) &&
                    memcmp(retainedAfter,retained,sizeof(retained))==0,"Late UI contaminated retained presentation HUDless pixels");
                D3D11_TEXTURE2D_DESC maskDesc{};maskDesc.Width=outputWidth;maskDesc.Height=outputHeight;
                maskDesc.MipLevels=maskDesc.ArraySize=maskDesc.SampleDesc.Count=1;maskDesc.Format=DXGI_FORMAT_R8_UNORM;
                maskDesc.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
                ComPtr<ID3D11Texture2D> mask,maskRead;ComPtr<ID3D11UnorderedAccessView> maskWrite;
                Check(device->CreateTexture2D(&maskDesc,nullptr,&mask),"Actual UI mask target");
                Check(device->CreateUnorderedAccessView(mask.Get(),nullptr,&maskWrite),"Actual UI mask write view");
                ComPtr<ID3D11ShaderResourceView> finalRead,hudlessRead;
                Check(device->CreateShaderResourceView(frameOutput.Get(),nullptr,&finalRead),"Actual final UI input");
                Check(device->CreateShaderResourceView(exported.hudless,nullptr,&hudlessRead),"Actual pre-UI input");
                context->OMSetRenderTargets(0,nullptr,nullptr);
                ID3D11ShaderResourceView* reads[]{finalRead.Get(),hudlessRead.Get()};auto write=maskWrite.Get();
                context->CSSetShader(uiShader.Get(),nullptr,0);context->CSSetShaderResources(0,2,reads);
                context->CSSetUnorderedAccessViews(0,1,&write,nullptr);context->Dispatch((outputWidth+7)/8,(outputHeight+7)/8,1);
                ID3D11ShaderResourceView* noReads[2]{};ID3D11UnorderedAccessView* noWrite=nullptr;
                context->CSSetShaderResources(0,2,noReads);context->CSSetUnorderedAccessViews(0,1,&noWrite,nullptr);
                context->CSSetShader(nullptr,nullptr,0);context->OMSetRenderTargets(1,&target,nullptr);
                maskDesc.BindFlags=0;maskDesc.Usage=D3D11_USAGE_STAGING;maskDesc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
                Check(device->CreateTexture2D(&maskDesc,nullptr,&maskRead),"UI mask readback target");context->CopyResource(maskRead.Get(),mask.Get());
                D3D11_MAPPED_SUBRESOURCE mapped{};Check(context->Map(maskRead.Get(),0,D3D11_MAP_READ,0,&mapped),"UI mask GPU readback");
                unsigned marked=0,unexpected=0;
                for(unsigned y=0;y<outputHeight;++y)for(unsigned x=0;x<outputWidth;++x) {
                    LONG pixelX=static_cast<LONG>(x),pixelY=static_cast<LONG>(y);
                    bool isUi=pixelX>=rectangle.left && pixelX<rectangle.right && pixelY>=rectangle.top && pixelY<rectangle.bottom;
                    unsigned actual=static_cast<const BYTE*>(mapped.pData)[y*mapped.RowPitch+x];
                    if(actual)++marked;if(actual!=(isUi?255u:0u))++unexpected;
                }
                context->Unmap(maskRead.Get(),0);Need(marked==1024 && unexpected==0,"Production UI alpha misclassified final/pre-UI pixels");
                printf("{\"event\":\"synthetic_low_nr_full_sr_hud_isolation\",\"frame\":%u,\"nr_input\":[%u,%u],\"nr_passes\":%u,\"presentation_output\":[%u,%u],\"matched_pre_ui_pixels\":1024,\"late_ui_isolated\":true,\"stale_frame_rejected\":true,\"production_ui_mask_checked_pixels\":%u,\"ui_pixels\":%u,\"ui_mask_mismatches\":%u,\"game_started\":false,\"quality_verified\":false}\n",
                    frame,iw,ih,neuralEnabled?requestedNrPasses:0,outputWidth,outputHeight,outputWidth*outputHeight,marked,unexpected);
            }
            if(index<3){changed[index]=differences;hashes[index]=hash;}
            else printf("{\"event\":\"synthetic_owned_resize_sr_nr_readback\",\"input\":[%u,%u],\"output\":[%u,%u],\"changed_pixels\":%u,\"pixels\":1024,\"game_started\":false,\"window_created\":false}\n",iw,ih,outputWidth,outputHeight,differences);
        }
#ifdef TWIN_KART_RECONSTRUCTION_FG
        if(presentation.enabled) {
            NativePresentationInputs inputs;Need(GetNativePresentationInputs(frame,inputs),"Current production reconstructed guides for combined FG");presentation.Present(inputs);
        }
#endif
        if(argc==4 && index==0) {
            // Simulate a temporary loading/resize callback on another thread.
            // Its new target size used to attempt owner-guarded retirement,
            // then permanently disable an otherwise healthy GPU runtime.
            D3D11_TEXTURE2D_DESC changedDesc{};output->GetDesc(&changedDesc);changedDesc.Width=800;changedDesc.Height=450;
            ComPtr<ID3D11Texture2D> changedTarget;ComPtr<ID3D11RenderTargetView> changedWrite;
            Check(device->CreateTexture2D(&changedDesc,nullptr,&changedTarget),"Foreign-owner resize target");
            Check(device->CreateRenderTargetView(changedTarget.Get(),nullptr,&changedWrite),"Foreign-owner resize view");
            ID3D11RenderTargetView* changed=changedWrite.Get();context->OMSetRenderTargets(1,&changed,nullptr);
            D3D11_VIEWPORT changedViewport{0,0,800,450,0,1};context->RSSetViewports(1,&changedViewport);
            auto retainedRuntime=runtime;auto evaluations=runtime->evaluations;bool accepted=true;
            std::thread foreign([&]{accepted=ProcessQualifiedNeuralPixelPass(context.Get(),frame+1);});foreign.join();
            Need(!accepted && !disabled && runtime==retainedRuntime && runtime->evaluations==evaluations,
                "Foreign render/resize callback permanently disabled reconstruction or changed owned GPU state");
            ID3D11RenderTargetView* nativeTarget=outputWrite.Get();context->OMSetRenderTargets(1,&nativeTarget,nullptr);
            D3D11_VIEWPORT nativeViewport{0,0,static_cast<float>(ow),static_cast<float>(oh),0,1};context->RSSetViewports(1,&nativeViewport);
            puts("{\"event\":\"synthetic_foreign_owner_resize_deferred\",\"owned_runtime_preserved\":true,\"disabled\":false,\"game_started\":false,\"window_created\":false}");
        }
        FinishNativeNeuralFrame(frame);
        NativePresentationInputs ended;
        Need(!GetNativePresentationInputs(frame,ended),"Finished frame retained presentation input eligibility");
    }
    Need(hashes[0]!=hashes[1] && hashes[1]!=hashes[2],"Moving source produced stale SR/NR GPU output");
    unsigned handoffEvaluations=0;
    if(argc==4) {
        Need(runtime && runtime->owner==GetCurrentThreadId(),"Expected current owner before explicit handoff fixture");
        auto preserved=runtime;unsigned originalOwner=runtime->owner;
        Need(!runtime->shutdownDrained(originalOwner+1,runtime->fence.Get(),runtime->nextFence),"Wrong expected consumer owner retired live features");
        Need(!runtime->shutdownDrained(originalOwner,nullptr,runtime->nextFence),"Absent completion fence retired live features");
        Need(!runtime->shutdownDrained(originalOwner,runtime->fence.Get(),0),"Zero fence value retired live features");
        Need(!runtime->shutdownDrained(originalOwner,runtime->fence.Get(),runtime->nextFence+1),"Uncompleted GPU work accepted as retirement proof");
        using GetState=decltype(&TwinKartNeuralGetState);
        auto getState=reinterpret_cast<GetState>(GetProcAddress(runtime->module,"TwinKartNeuralGetState"));
        TwinKartNeuralState state{sizeof(TwinKartNeuralState)};
        Need(getState && getState(&state) && state.initialized && state.featureCreated==neuralEnabled && state.evaluations==(neuralEnabled?1u:0u),
            "Rejected retirement changed the real consumer state");
        ComPtr<ID3D11DeviceContext> deferred;Check(device->CreateDeferredContext(0,&deferred),"Wrong-context handoff fixture");
        Need(!RetireNativeNeuralForRenderThread(deferred.Get()) && runtime==preserved,"Deferred context stole immediate reconstruction ownership");
        {
            RuntimeUse held;Need(held.entered,"Fixture could not exclude old CPU resource use");bool accepted=true;
            std::thread contender([&]{accepted=RetireNativeNeuralForRenderThread(context.Get());});contender.join();
            Need(!accepted && runtime==preserved,"Explicit handoff raced live CPU resource use");
        }
        // NR now precedes SR: its output bridge has input dimensions, unlike
        // the final target. Keep strict CopyTo12 extent validation intact.
        ID3D11Texture2D* nrInput=runtime->lowResolutionNeural?runtime->srColor.Get():output.Get();
        D3D11_TEXTURE2D_DESC nrExtent{};nrInput->GetDesc(&nrExtent);
        Need(nrExtent.Width==(runtime->lowResolutionNeural?iw:ow) && nrExtent.Height==(runtime->lowResolutionNeural?ih:oh),
            "In-flight retirement fixture selected the wrong NR resource extent");
        if(runtime->lowResolutionNeural)
            Need(runtime->outputBridge.CopyTo12(output.Get())==E_INVALIDARG,"Low-resolution NR bridge accepted a full-resolution source");
        Check(runtime->outputBridge.CopyTo12(nrInput),"Acquire bridge for in-flight retirement rejection");
        HRESULT bridgeRetired=S_OK;
        std::thread premature([&]{bridgeRetired=runtime->outputBridge.DrainForRetirement(originalOwner,context.Get());});premature.join();
        Need(FAILED(bridgeRetired),"Foreign retirement consumed a bridge still owned by D3D12");
        Check(runtime->outputBridge.ReturnTo11(nullptr),"Restore bridge after rejected in-flight retirement");
        auto evaluateNewOwner=[&](unsigned frame) {
            context->ClearState();context->VSSetShader(vertex.Get(),nullptr,0);context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            updateMovingInput(frame-120);
            ID3D11RenderTargetView* target=colorWrite.Get();context->OMSetRenderTargets(1,&target,nullptr);context->PSSetShader(produce.Get(),nullptr,0);
            D3D11_VIEWPORT viewport{16,8,iw,ih,0,1};context->RSSetViewports(1,&viewport);context->Draw(3,0);RecordNativeSceneDraw(context.Get(),frame);
            Need(SnapshotNativeGuides(context.Get(),parameterBuffer.Get(),viewBuffer.Get(),aw,ah,aw,ah,nativeLayout,nativeView),"New-owner immutable guide snapshot");
            nativeDepth=depthView;nativeVelocity=velocityView;guideFrame=frame;
            target=outputWrite.Get();context->OMSetRenderTargets(1,&target,nullptr);ID3D11ShaderResourceView* scene=colorRead.Get();
            context->PSSetShaderResources(0,1,&scene);context->PSSetShader(compose.Get(),nullptr,0);
            viewport={0,0,static_cast<float>(ow),static_cast<float>(oh),0,1};context->RSSetViewports(1,&viewport);context->Draw(3,0);
            unsigned before[1024]{},after[1024]{};Need(ReadPatch(context.Get(),device.Get(),output.Get(),before),"New-owner baseline readback");
            Need(!ProcessNativeNeuralPixelPass(context.Get(),frame),"New-owner fixture shader escaped original identity guard");
            Need(ProcessQualifiedNeuralPixelPass(context.Get(),frame) && runtime && runtime->owner==GetCurrentThreadId() &&
                 runtime->evaluations==(neuralEnabled?1u:0u) && runtime->superEvaluations==(guidesOnlyFixture?0u:1u) &&
                 NativeSuperResolutionOutputMatches(output.Get(),frame)==!guidesOnlyFixture,
                 "New owner failed to rebuild actual SR/NR consumers and temporal history");
            VerifyNeuralState(1,iw,ih);
            Need(ReadPatch(context.Get(),device.Get(),output.Get(),after),"New-owner actual GPU output readback");unsigned changed=0;
            for(unsigned n=0;n<1024;++n)if(before[n]!=after[n])++changed;
            Need(guidesOnlyFixture?changed==0:changed>0,"Rebuilt owner violated output change/preservation contract");++handoffEvaluations;
            printf("{\"event\":\"synthetic_new_owner_sr_nr_readback\",\"owner\":%lu,\"frame\":%u,\"pixels\":1024,\"changed_pixels\":%u,\"sr_evaluations\":%u,\"nr_evaluations\":%u,\"game_started\":false,\"window_created\":false}\n",GetCurrentThreadId(),frame,changed,guidesOnlyFixture?0u:1u,neuralEnabled?1u:0u);
            FinishNativeNeuralFrame(frame);
        };
        std::exception_ptr workerFailure;DWORD workerOwner=0;
        std::thread worker([&]{try {
            workerOwner=GetCurrentThreadId();
            Need(!ProcessQualifiedNeuralPixelPass(context.Get(),125) && runtime==preserved,"Unannounced foreign draw stole consumer ownership");
            Need(RetireNativeNeuralForRenderThread(context.Get()) && !runtime && !disabled,"Explicit new-thread GPU retirement failed");
            evaluateNewOwner(125);
        }catch(...){workerFailure=std::current_exception();}});worker.join();if(workerFailure)std::rethrow_exception(workerFailure);
        Need(workerOwner!=GetCurrentThreadId() && runtime && runtime->owner==workerOwner,"Separate actual owner thread was not exercised");
        Need(!ProcessQualifiedNeuralPixelPass(context.Get(),126),"Old owner resumed without explicit context handoff");
        Need(RetireNativeNeuralForRenderThread(context.Get()) && !runtime && !disabled,"Explicit return-thread GPU retirement failed");
        evaluateNewOwner(126);
        puts("{\"event\":\"synthetic_owner_handoff_complete\",\"actual_owner_changes\":2,\"completed_gpu_and_bridge_retirements\":2,\"new_owner_sr_nr_rebuilds\":2,\"invalid_owner_and_fence_proofs_rejected\":true,\"cpu_overlap_rejected\":true,\"d3d12_owned_bridge_retirement_rejected\":true,\"unannounced_foreign_draws_rejected\":true,\"game_started\":false,\"window_created\":false,\"quality_verified\":false}");
    }
#ifdef TWIN_KART_RECONSTRUCTION_FG
    if(presentation.enabled) {
        printf("{\"event\":\"synthetic_production_sr_nr_fg_complete\",\"rendered\":%u,\"actually_presented\":%llu,\"frames_reporting_generation\":%u,\"sr_evaluations\":%u,\"nr_evaluations\":%u,\"shared_core_rebuilds\":%u,\"production_native_marker_path\":%s,\"synthetic_inputs\":true,\"synthetic_frame_markers\":true,\"game_started\":false,\"quality_verified\":false}\n",
            presentation.rendered,presentation.presented,presentation.generated,frameCount,neuralEnabled?frameCount:0,sharedCoreRebuilds,nativeFixture?"true":"false");
        Need(presentation.generated>0 && presentation.presented>presentation.rendered,"Combined path did not actually report generated frames");
    }
#endif
    Need(RetireNativeNeuralForDeviceShutdown(context.Get()),"Completed production pre-presentation-shutdown retirement failed");context->ClearState();
    if(argc==4)printf("{\"event\":\"synthetic_production_sr_nr_complete\",\"input\":[640,360],\"output\":[1280,720],\"sr_evaluations\":%u,\"nr_evaluations\":%u,\"same_owner_resize_rebuilds\":2,\"explicit_owner_handoff_rebuilds\":%u,\"foreign_owner_callback_rejected_without_disabling\":true,\"gpu_readback_pixels_per_frame\":1024,\"initial_changed_pixels\":[%u,%u,%u],\"initial_output_hashes\":[\"%016llx\",\"%016llx\",\"%016llx\"],\"independent_color_depth_origins\":true,\"padded_allocations\":true,\"native_shader_guard_preserved\":true,\"source_pixels_resized\":false,\"frame_generation_tested\":false,\"window_created\":false,\"game_started\":false,\"synthetic_inputs\":true,\"quality_verified\":false}\n",
        guidesOnlyFixture?0:frameCount+handoffEvaluations,neuralEnabled?frameCount+handoffEvaluations:0,handoffEvaluations,changed[0],changed[1],changed[2],hashes[0],hashes[1],hashes[2]);return 0;
}catch(const std::exception& e){fprintf(stdout,"%s\n",e.what());fflush(stdout);return 1;}}
