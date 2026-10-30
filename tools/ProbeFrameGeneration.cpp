#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdio>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include "../native/FrameGenerationD3D12.h"
#include "../native/NativeViewMath.h"
using Microsoft::WRL::ComPtr;
void Need(HRESULT result) { if(FAILED(result)) throw std::runtime_error("Frame-generation fixture API failed"); }
void Ok(unsigned result) { if(result) { fprintf(stderr,"Streamline result %u\n",result); throw std::runtime_error("Frame-generation fixture contract rejected"); } }
unsigned PresentFixture(TwinKartGraphics::FrameGeneration& generation,IDXGIFactory6* factory,ID3D12Device* device,bool focused) {
    WNDCLASSW windowClass{}; windowClass.lpfnWndProc=DefWindowProcW; windowClass.hInstance=GetModuleHandleW(nullptr); windowClass.lpszClassName=L"TwinKartHiddenGraphicsFixture";
    if(!RegisterClassW(&windowClass)) throw std::runtime_error("Hidden graphics fixture window unavailable");
    HWND previous=GetForegroundWindow();
    HWND window=CreateWindowExW(focused?WS_EX_TOOLWINDOW:WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,windowClass.lpszClassName,
        L"Twin Kart GPU test (synthetic, no game)",focused?WS_OVERLAPPEDWINDOW:WS_POPUP,80,80,1280,720,nullptr,nullptr,windowClass.hInstance,nullptr);
    if(!window) throw std::runtime_error("Hidden graphics fixture creation failed");
    struct WindowOwner { HWND window,previous; ~WindowOwner() { bool restore=GetForegroundWindow()==window;if(IsWindow(window))DestroyWindow(window);
        if(restore && IsWindow(previous))SetForegroundWindow(previous);UnregisterClassW(L"TwinKartHiddenGraphicsFixture",GetModuleHandleW(nullptr)); } } owner{window,previous};
    if(focused) {
        ShowWindow(window,SW_SHOWNORMAL);UpdateWindow(window);SetForegroundWindow(window);
        MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
        if(GetForegroundWindow()!=window)throw std::runtime_error("Focused fixture could not obtain actual window focus; no generated frames claimed");
    }
    ComPtr<ID3D12CommandQueue> queue; D3D12_COMMAND_QUEUE_DESC queueDesc{}; Need(device->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&queue)));
    ComPtr<ID3D12CommandAllocator> allocator; ComPtr<ID3D12GraphicsCommandList> commands; ComPtr<ID3D12Fence> fence;
    Need(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)));
    Need(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&commands))); Need(commands->Close());
    Need(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)));
    DXGI_SWAP_CHAIN_DESC1 desc{}; desc.Width=1280; desc.Height=720; desc.Format=DXGI_FORMAT_R10G10B10A2_UNORM; desc.SampleDesc.Count=1;
    desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT; desc.BufferCount=2; desc.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;
    ComPtr<IDXGISwapChain1> base; ComPtr<IDXGISwapChain4> swapchain;
    Need(factory->CreateSwapChainForHwnd(queue.Get(),window,&desc,nullptr,nullptr,&base)); Need(base.As(&swapchain));
    ComPtr<ID3D12Resource> textures[4]; ComPtr<ID3D12DescriptorHeap> gpuHeap,cpuHeap;
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{}; heapDesc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; heapDesc.NumDescriptors=4; heapDesc.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    Need(device->CreateDescriptorHeap(&heapDesc,IID_PPV_ARGS(&gpuHeap))); heapDesc.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE; Need(device->CreateDescriptorHeap(&heapDesc,IID_PPV_ARGS(&cpuHeap)));
    const DXGI_FORMAT formats[]={DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R16G16_FLOAT,DXGI_FORMAT_R10G10B10A2_UNORM,DXGI_FORMAT_R8_UNORM};
    D3D12_HEAP_PROPERTIES properties{}; properties.Type=D3D12_HEAP_TYPE_DEFAULT;
    auto gpuCpu=gpuHeap->GetCPUDescriptorHandleForHeapStart(); auto cpu=cpuHeap->GetCPUDescriptorHandleForHeapStart();
    unsigned increment=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    for(unsigned index=0;index<4;++index) {
        D3D12_RESOURCE_DESC resource{}; resource.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D; resource.Width=1280; resource.Height=720;
        resource.Format=formats[index]; resource.DepthOrArraySize=1; resource.MipLevels=1; resource.SampleDesc.Count=1; resource.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        Need(device->CreateCommittedResource(&properties,D3D12_HEAP_FLAG_NONE,&resource,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,nullptr,IID_PPV_ARGS(&textures[index])));
        D3D12_UNORDERED_ACCESS_VIEW_DESC view{}; view.Format=formats[index]; view.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(textures[index].Get(),nullptr,&view,gpuCpu); device->CreateUnorderedAccessView(textures[index].Get(),nullptr,&view,cpu); gpuCpu.ptr+=increment; cpu.ptr+=increment;
    }
    TwinKartGraphics::CameraFrame camera{};
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    memcpy(camera.clipToPrevious,identity,sizeof(identity)); memcpy(camera.previousToClip,identity,sizeof(identity));
    camera.nearPlane=0.1f; camera.farPlane=1000; camera.fieldOfView=1.570796327f; camera.aspect=1280.0f/720;
    float x=1/camera.aspect,y=1,z=camera.farPlane/(camera.farPlane-camera.nearPlane),w=-camera.nearPlane*z;
    const float projection[16]={x,0,0,0,0,y,0,0,0,0,z,1,0,0,w,0};
    const float inverse[16]={1/x,0,0,0,0,1/y,0,0,0,0,0,1/w,0,0,1,-z/w};
    memcpy(camera.viewToClip,projection,sizeof(projection)); memcpy(camera.clipToView,inverse,sizeof(inverse));
    camera.up[1]=camera.right[0]=camera.forward[2]=1; camera.motionScaleX=1.0f/1280; camera.motionScaleY=1.0f/720;
    unsigned framesWithGenerated=0; unsigned long long total=0;
    unsigned frameCount=focused?120:32;
    for(unsigned frame=0;frame<frameCount;++frame) {
        if(focused) {
            MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}
            if(!IsWindow(window))throw std::runtime_error("Focused fixture was closed");
            Sleep(16);
        }
        MSG message; while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
        camera.reset=frame<2; Ok(generation.BeginFrame(frame,camera)); Ok(generation.Marker(TwinKartGraphics::FrameStage::SimulationStart));
        Need(allocator->Reset()); Need(commands->Reset(allocator.Get(),nullptr));
        ID3D12DescriptorHeap* heaps[]={gpuHeap.Get()}; commands->SetDescriptorHeaps(1,heaps);
        gpuCpu=gpuHeap->GetCPUDescriptorHandleForHeapStart(); cpu=cpuHeap->GetCPUDescriptorHandleForHeapStart(); auto gpu=gpuHeap->GetGPUDescriptorHandleForHeapStart();
        const float clears[][4]={{0.5f,0,0,0},{0,0,0,0},{0.2f+frame*0.01f,0.4f,0.6f,1},{0,0,0,0}};
        for(unsigned index=0;index<4;++index) {
            D3D12_RESOURCE_BARRIER barrier{}; barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; barrier.Transition.pResource=textures[index].Get();
            barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE; barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_UNORDERED_ACCESS; barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            commands->ResourceBarrier(1,&barrier); commands->ClearUnorderedAccessViewFloat(gpu,cpu,textures[index].Get(),clears[index],0,nullptr);
            barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_UNORDERED_ACCESS; barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE; commands->ResourceBarrier(1,&barrier);
            gpu.ptr+=increment; gpuCpu.ptr+=increment; cpu.ptr+=increment;
        }
        Ok(generation.Marker(TwinKartGraphics::FrameStage::SimulationEnd));
        TwinKartGraphics::GenerationResources inputs{commands.Get(),textures[0].Get(),textures[1].Get(),textures[2].Get(),textures[3].Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,true,true};
        Ok(generation.Tag(inputs));
        unsigned backIndex=swapchain->GetCurrentBackBufferIndex(); ComPtr<ID3D12Resource> backbuffer; Need(swapchain->GetBuffer(backIndex,IID_PPV_ARGS(&backbuffer)));
        D3D12_RESOURCE_BARRIER barriers[2]{}; for(auto& barrier:barriers) { barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; }
        barriers[0].Transition={backbuffer.Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_DEST};
        barriers[1].Transition={textures[2].Get(),D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_SOURCE};
        commands->ResourceBarrier(2,barriers); commands->CopyResource(backbuffer.Get(),textures[2].Get());
        for(auto& barrier:barriers) { auto previousState=barrier.Transition.StateBefore; barrier.Transition.StateBefore=barrier.Transition.StateAfter; barrier.Transition.StateAfter=previousState; } commands->ResourceBarrier(2,barriers);
        Need(commands->Close()); Ok(generation.Marker(TwinKartGraphics::FrameStage::RenderSubmitStart));
        ID3D12CommandList* lists[]={commands.Get()}; queue->ExecuteCommandLists(1,lists); Ok(generation.Marker(TwinKartGraphics::FrameStage::RenderSubmitEnd));
        Ok(generation.Marker(TwinKartGraphics::FrameStage::PresentStart)); HRESULT presented=swapchain->Present(0,0); Ok(generation.Marker(TwinKartGraphics::FrameStage::PresentEnd));
        auto state=generation.Presented(); if(state.lastPresented>=2) ++framesWithGenerated; total+=state.lastPresented;
        if(state.inputCompletionFence && state.inputCompletionValue) Need(queue->Wait(state.inputCompletionFence,state.inputCompletionValue));
        Need(queue->Signal(fence.Get(),frame+1)); HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr); if(!event) throw std::runtime_error("FG fixture fence event failed");
        Need(fence->SetEventOnCompletion(frame+1,event)); DWORD waited=WaitForSingleObject(event,10000); CloseHandle(event); if(waited!=WAIT_OBJECT_0) throw std::runtime_error("FG fixture GPU wait timed out");
        if(frame%8==0) { printf("{\"event\":\"fg_%s_fixture_present\",\"frame\":%u,\"hresult\":%u,\"result\":%u,\"status\":%u,\"actually_presented\":%u,\"window_focused\":%s,\"synthetic_inputs\":true,\"game_acceptance\":false}\n",focused?"focused":"hidden",frame,static_cast<unsigned>(presented),state.result,state.status,state.lastPresented,GetForegroundWindow()==window?"true":"false"); fflush(stdout); }
    }
    printf("{\"event\":\"fg_%s_fixture_complete\",\"real_frames\":%u,\"frames_reporting_generation\":%u,\"presented_total\":%llu,\"game_started\":false,\"visible_quality_verified\":false}\n",focused?"focused":"hidden",frameCount,framesWithGenerated,total); fflush(stdout);
    return framesWithGenerated;
}
int wmain(int argc,wchar_t** argv) {
    bool cameraOnly=argc==4 && !wcscmp(argv[3],L"--camera-contract");
    if(argc!=3 && !(argc==4 && (cameraOnly || !wcscmp(argv[3],L"--present-fixture") || !wcscmp(argv[3],L"--focused-present-fixture")))) { puts("Usage: ProbeFrameGeneration.exe <external-Streamline-directory> <existing-private-log-directory> [--camera-contract | --present-fixture | --focused-present-fixture]"); return 2; }
    TwinKartGraphics::FrameGeneration generation;
    unsigned loaded=generation.Load(argv[1],argv[2]); printf("{\"event\":\"fg_runtime_init\",\"result\":%u}\n",loaded); fflush(stdout);
    if(loaded) return 3;
    using Factory=HRESULT(WINAPI*)(REFIID,void**);
    using Device=HRESULT(WINAPI*)(IUnknown*,D3D_FEATURE_LEVEL,REFIID,void**);
    auto createFactory=reinterpret_cast<Factory>(generation.GraphicsEntry("CreateDXGIFactory1"));
    auto createDevice=reinterpret_cast<Device>(generation.GraphicsEntry("D3D12CreateDevice"));
    if(!createFactory || !createDevice) return 4;
    ComPtr<IDXGIFactory6> factory; ComPtr<IDXGIAdapter1> adapter; ComPtr<ID3D12Device> device;
    if(FAILED(createFactory(IID_PPV_ARGS(&factory)))) return 5;
    for(UINT index=0;factory->EnumAdapterByGpuPreference(index,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter))!=DXGI_ERROR_NOT_FOUND;++index) {
        DXGI_ADAPTER_DESC1 desc{}; adapter->GetDesc1(&desc);
        if(desc.VendorId==0x10de && !(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE) && SUCCEEDED(createDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)))) break;
        adapter.Reset();
    }
    if(!device) return 6;
    unsigned bound=generation.BindDevice(device.Get()); printf("{\"event\":\"fg_device_bind\",\"result\":%u}\n",bound); fflush(stdout);
    unsigned configured=bound?bound:generation.Configure(2,1280,720,DXGI_FORMAT_R10G10B10A2_UNORM,2);
    printf("{\"event\":\"fg_configure\",\"result\":%u,\"requested_multiplier\":2}\n",configured);
    auto state=generation.Presented();
    printf("{\"event\":\"fg_runtime_state\",\"result\":%u,\"status\":%u,\"maximum_generated\":%u,\"actually_presented\":%u,\"game_started\":false,\"frame_generation_verified\":false}\n",state.result,state.status,state.maximumGeneratedFrames,state.lastPresented); fflush(stdout);
    if(!configured && cameraOnly) {
        try {
            float view[576]{};view[536]=640;view[537]=360;
            const float projection[16]={1.125f,0,0,0,0,2,0,0,0,0,0,1,0,0,10,0};
            memcpy(view+32,projection,64);memcpy(view+144,projection,64);
            view[504]=view[509]=view[514]=view[519]=1;
            TwinKartGraphics::CameraFrame camera{};
            if(!TwinKartGraphics::DecodeNativeCamera(view,640,360,true,camera))throw std::runtime_error("Infinite native camera fixture decode failed");
            Ok(generation.BeginFrame(1,camera));
            Ok(generation.Marker(TwinKartGraphics::FrameStage::SimulationStart));
            Ok(generation.Marker(TwinKartGraphics::FrameStage::SimulationEnd));
            auto sentinel=camera;sentinel.farPlane=FLT_MAX;
            if(generation.BeginFrame(2,sentinel)==0)throw std::runtime_error("Uninitialized cameraFar sentinel accepted");
            auto mismatch=camera;mismatch.invertedDepth=false;
            if(generation.BeginFrame(3,mismatch)==0)throw std::runtime_error("Inconsistent infinite camera contract accepted");
            printf("{\"event\":\"fg_infinite_camera_constants_accepted\",\"infinite_reversed_z\":true,\"invalid_sentinel_rejected\":true,\"inconsistent_depth_rejected\":true,\"window_created\":false,\"game_started\":false,\"generated_frames_verified\":false,\"quality_verified\":false}\n");
        } catch(const std::exception& error) {fprintf(stderr,"%s\n",error.what());configured=1;}
    } else if(!configured && argc==4) {
        try { unsigned generated=PresentFixture(generation,factory.Get(),device.Get(),!wcscmp(argv[3],L"--focused-present-fixture")); if(!generated) configured=1; }
        catch(const std::exception& error) { fprintf(stderr,"%s\n",error.what()); configured=1; }
    }
    generation.Shutdown(); device.Reset(); adapter.Reset(); factory.Reset();
    return configured?1:0;
}
