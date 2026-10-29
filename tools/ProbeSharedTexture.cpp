#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11_4.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstring>
#include "../native/SharedTextureBridge.h"
#include "../native/FrameGenerationD3D12.h"
using Microsoft::WRL::ComPtr;
int wmain(int argc,wchar_t** argv) {
    if(argc!=1 && argc!=3)return 2;
    TwinKartGraphics::FrameGeneration generation;
    using Factory=HRESULT(WINAPI*)(REFIID,void**);using Device=HRESULT(WINAPI*)(IUnknown*,D3D_FEATURE_LEVEL,REFIID,void**);
    Factory createFactory=CreateDXGIFactory1;Device createDevice=D3D12CreateDevice;
    if(argc==3) {
        unsigned loaded=generation.Load(argv[1],argv[2]);
        printf("{\"event\":\"shared_texture_interposer_init\",\"result\":%u,\"no_presentation\":true,\"game_started\":false}\n",loaded);
        if(loaded)return 20;
        createFactory=reinterpret_cast<Factory>(generation.GraphicsEntry("CreateDXGIFactory1"));
        createDevice=reinterpret_cast<Device>(generation.GraphicsEntry("D3D12CreateDevice"));
        if(!createFactory || !createDevice)return 21;
    }
    ComPtr<IDXGIFactory6> factory; ComPtr<IDXGIAdapter1> adapter; ComPtr<ID3D12Device> d12;
    HRESULT hr=createFactory(IID_PPV_ARGS(&factory)); if(FAILED(hr)) return 2;
    for(UINT index=0;factory->EnumAdapterByGpuPreference(index,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter))!=DXGI_ERROR_NOT_FOUND;++index) {
        DXGI_ADAPTER_DESC1 desc{}; adapter->GetDesc1(&desc);
        if(desc.VendorId==0x10de && SUCCEEDED(createDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&d12)))) break; adapter.Reset();
    }
    if(!d12) return 3;
    ComPtr<ID3D11Device> d11; ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL requested[]={D3D_FEATURE_LEVEL_11_1,D3D_FEATURE_LEVEL_11_0},obtained{};
    if(FAILED(D3D11CreateDevice(adapter.Get(),D3D_DRIVER_TYPE_UNKNOWN,nullptr,0,requested,2,D3D11_SDK_VERSION,&d11,&obtained,&context))) return 4;
    ComPtr<ID3D12CommandQueue> queue; D3D12_COMMAND_QUEUE_DESC queueDesc{};
    if(FAILED(d12->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&queue)))) return 5;
    TwinKartGraphics::SharedTextureBridge bridge; hr=bridge.Initialize(d11.Get(),d12.Get(),queue.Get(),64,32,DXGI_FORMAT_R10G10B10A2_UNORM);
    printf("{\"event\":\"shared_texture_init\",\"hresult\":%u,\"cpu_texture_copy\":false}\n",static_cast<unsigned>(hr)); if(FAILED(hr)) return 6;
    ComPtr<ID3D11Texture2D> source,target,readSource,readTarget; D3D11_TEXTURE2D_DESC desc{};
    desc.Width=64; desc.Height=32; desc.MipLevels=1; desc.ArraySize=1; desc.Format=DXGI_FORMAT_R10G10B10A2_UNORM; desc.SampleDesc.Count=1; desc.BindFlags=D3D11_BIND_RENDER_TARGET;
    if(FAILED(d11->CreateTexture2D(&desc,nullptr,&source)) || FAILED(d11->CreateTexture2D(&desc,nullptr,&target))) return 7;
    ComPtr<ID3D11RenderTargetView> view; if(FAILED(d11->CreateRenderTargetView(source.Get(),nullptr,&view))) return 8;
    const float color[]={0.2f,0.4f,0.6f,1}; context->ClearRenderTargetView(view.Get(),color);
    if(FAILED(bridge.CopyTo12(source.Get())) || FAILED(bridge.ReturnTo11(target.Get())) || FAILED(bridge.Drain())) return 9;
    ComPtr<ID3D12CommandAllocator> allocator; ComPtr<ID3D12GraphicsCommandList> commands; ComPtr<ID3D12DescriptorHeap> gpuHeap,cpuHeap;
    if(FAILED(d12->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))) || FAILED(d12->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&commands)))) return 12;
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{}; heapDesc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; heapDesc.NumDescriptors=1; heapDesc.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if(FAILED(d12->CreateDescriptorHeap(&heapDesc,IID_PPV_ARGS(&gpuHeap)))) return 13; heapDesc.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    if(FAILED(d12->CreateDescriptorHeap(&heapDesc,IID_PPV_ARGS(&cpuHeap)))) return 13;
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{}; uav.Format=DXGI_FORMAT_R10G10B10A2_UNORM; uav.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;
    auto gpuCpu=gpuHeap->GetCPUDescriptorHandleForHeapStart(); auto cpu=cpuHeap->GetCPUDescriptorHandleForHeapStart();
    d12->CreateUnorderedAccessView(bridge.Texture12(),nullptr,&uav,gpuCpu); d12->CreateUnorderedAccessView(bridge.Texture12(),nullptr,&uav,cpu);
    if(FAILED(bridge.CopyTo12(source.Get()))) return 14;
    D3D12_RESOURCE_BARRIER barrier{}; barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; barrier.Transition.pResource=bridge.Texture12();
    barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_COMMON; barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_UNORDERED_ACCESS; barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; commands->ResourceBarrier(1,&barrier);
    ID3D12DescriptorHeap* heaps[]={gpuHeap.Get()}; commands->SetDescriptorHeaps(1,heaps);
    const float changedColor[]={0.6f,0.2f,0.4f,1}; commands->ClearUnorderedAccessViewFloat(gpuHeap->GetGPUDescriptorHandleForHeapStart(),cpu,bridge.Texture12(),changedColor,0,nullptr);
    barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_UNORDERED_ACCESS; barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_COMMON; commands->ResourceBarrier(1,&barrier);
    if(FAILED(commands->Close())) return 15; ID3D12CommandList* lists[]={commands.Get()}; queue->ExecuteCommandLists(1,lists);
    if(FAILED(bridge.ReturnTo11(target.Get())) || FAILED(bridge.Drain())) return 16;
    context->ClearRenderTargetView(view.Get(),changedColor);
    desc.Usage=D3D11_USAGE_STAGING; desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ; desc.BindFlags=0;
    if(FAILED(d11->CreateTexture2D(&desc,nullptr,&readSource)) || FAILED(d11->CreateTexture2D(&desc,nullptr,&readTarget))) return 10;
    context->CopyResource(readSource.Get(),source.Get()); context->CopyResource(readTarget.Get(),target.Get());
    D3D11_MAPPED_SUBRESOURCE original{},copied{};
    if(FAILED(context->Map(readSource.Get(),0,D3D11_MAP_READ,0,&original)) || FAILED(context->Map(readTarget.Get(),0,D3D11_MAP_READ,0,&copied))) return 11;
    bool equal=true; for(unsigned y=0;y<32;++y) if(memcmp(static_cast<const BYTE*>(original.pData)+y*original.RowPitch,static_cast<const BYTE*>(copied.pData)+y*copied.RowPitch,64*4)) equal=false;
    context->Unmap(readSource.Get(),0); context->Unmap(readTarget.Get(),0);
    printf("{\"event\":\"shared_texture_round_trip\",\"all_pixels_equal\":%s,\"d3d12_write_reached_d3d11\":%s,\"cross_api_gpu_fence\":true,\"pixels\":2048,\"game_started\":false}\n",equal?"true":"false",equal?"true":"false"); return equal?0:1;
}
