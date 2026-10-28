#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <cstdio>
#include <stdexcept>
#include <cstring>
#include "../native/NeuralD3D12.h"
using Microsoft::WRL::ComPtr;
void RuntimeLog(const char* message,unsigned,unsigned) { if(message) fprintf(stderr,"%.*s\n",16384,message); }
// Standalone interface/GPU fixture. Synthetic resources are explicitly labeled
// and never counted as game guides, a moving scene or final composition.
void Need(HRESULT result) { if(FAILED(result)) throw std::runtime_error("D3D12 fixture operation failed"); }
bool Submit(ID3D12GraphicsCommandList* commands,ID3D12CommandQueue* queue,ID3D12Fence* fence,UINT64 value) {
    if(FAILED(commands->Close())) return false;
    ID3D12CommandList* lists[]={commands}; queue->ExecuteCommandLists(1,lists);
    if(FAILED(queue->Signal(fence,value))) return false;
    HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr); if(!event) return false;
    HRESULT signaled=fence->SetEventOnCompletion(value,event);
    DWORD waited=SUCCEEDED(signaled)?WaitForSingleObject(event,10000):WAIT_FAILED; CloseHandle(event);
    return waited==WAIT_OBJECT_0;
}
unsigned EvaluateFixture(ID3D12Device* device,ID3D12CommandQueue* queue,ID3D12CommandAllocator* allocator,ID3D12GraphicsCommandList* commands,ID3D12Fence* fence,bool superResolution=false,UINT64 fenceValue=2) {
    Need(allocator->Reset()); Need(commands->Reset(allocator,nullptr));
    if(superResolution) {
        unsigned created=TwinKartSuperResolutionCreate(commands,640,360,1280,720,0,false,false);
        printf("{\"event\":\"sr_fixture_create\",\"result\":%u,\"input\":[640,360],\"output\":[1280,720]}\n",created); fflush(stdout);
        if(created!=1) {
            if(!Submit(commands,queue,fence,fenceValue)) throw std::runtime_error("SR creation GPU fence failed");
            return created;
        }
    }
    ComPtr<ID3D12DescriptorHeap> gpuHeap,cpuHeap;
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc{}; heapDesc.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; heapDesc.NumDescriptors=4; heapDesc.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    Need(device->CreateDescriptorHeap(&heapDesc,IID_PPV_ARGS(&gpuHeap))); heapDesc.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    Need(device->CreateDescriptorHeap(&heapDesc,IID_PPV_ARGS(&cpuHeap)));
    ComPtr<ID3D12Resource> textures[4],readback;
    const DXGI_FORMAT formats[]={DXGI_FORMAT_R10G10B10A2_UNORM,DXGI_FORMAT_R32_FLOAT,DXGI_FORMAT_R16G16_FLOAT,DXGI_FORMAT_R10G10B10A2_UNORM};
    const float clears[][4]={{0.4f,0.6f,0.2f,1},{0.5f,0,0,0},{0,0,0,0},{1,0,1,1}};
    D3D12_HEAP_PROPERTIES properties{}; properties.Type=D3D12_HEAP_TYPE_DEFAULT;
    auto gpuCpu=gpuHeap->GetCPUDescriptorHandleForHeapStart(); auto cpu=cpuHeap->GetCPUDescriptorHandleForHeapStart(); auto gpu=gpuHeap->GetGPUDescriptorHandleForHeapStart();
    unsigned increment=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    ID3D12DescriptorHeap* heaps[]={gpuHeap.Get()}; commands->SetDescriptorHeaps(1,heaps);
    for(unsigned index=0;index<4;++index) {
        D3D12_RESOURCE_DESC desc{}; desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D; desc.Width=superResolution && index!=3?640:1280; desc.Height=superResolution && index!=3?360:720; desc.DepthOrArraySize=1; desc.MipLevels=1; desc.Format=formats[index]; desc.SampleDesc.Count=1; desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        Need(device->CreateCommittedResource(&properties,D3D12_HEAP_FLAG_NONE,&desc,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&textures[index])));
        D3D12_UNORDERED_ACCESS_VIEW_DESC view{}; view.Format=formats[index]; view.ViewDimension=D3D12_UAV_DIMENSION_TEXTURE2D;
        device->CreateUnorderedAccessView(textures[index].Get(),nullptr,&view,gpuCpu); device->CreateUnorderedAccessView(textures[index].Get(),nullptr,&view,cpu);
        commands->ClearUnorderedAccessViewFloat(gpu,cpu,textures[index].Get(),clears[index],0,nullptr);
        D3D12_RESOURCE_BARRIER barrier{}; barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV; barrier.UAV.pResource=textures[index].Get(); commands->ResourceBarrier(1,&barrier);
        if(index!=3) { barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; barrier.Transition.pResource=textures[index].Get(); barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_UNORDERED_ACCESS; barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE; barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; commands->ResourceBarrier(1,&barrier); }
        gpu.ptr+=increment; gpuCpu.ptr+=increment; cpu.ptr+=increment;
    }
    unsigned result=0;
    if(superResolution) {
        TwinKartSuperResolutionFrame frame{sizeof(TwinKartSuperResolutionFrame),commands,textures[0].Get(),textures[1].Get(),textures[2].Get(),textures[3].Get(),0.25f,0.25f,1,1,16.67f,1,1,true,true,true};
        result=TwinKartSuperResolutionEvaluate(&frame);
    } else {
        TwinKartNeuralFrame frame{sizeof(TwinKartNeuralFrame),commands,textures[0].Get(),textures[1].Get(),textures[2].Get(),textures[3].Get(),1,1,1,true,false,true};
        result=TwinKartNeuralEvaluate(&frame);
    }
    printf("{\"event\":\"%s_fixture_evaluate\",\"result\":%u,\"synthetic_inputs\":true,\"game_guides_verified\":false,\"final_composition_verified\":false}\n",superResolution?"sr":"neural",result); fflush(stdout);
    D3D12_RESOURCE_BARRIER barrier{}; barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; barrier.Transition.pResource=textures[3].Get(); barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_UNORDERED_ACCESS; barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE; barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    commands->ResourceBarrier(1,&barrier);
    auto outputDesc=textures[3]->GetDesc(); D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{}; UINT64 total=0; UINT rows=0; UINT64 rowBytes=0;
    device->GetCopyableFootprints(&outputDesc,0,1,0,&footprint,&rows,&rowBytes,&total);
    D3D12_RESOURCE_DESC buffer{}; buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER; buffer.Width=total; buffer.Height=1; buffer.DepthOrArraySize=1; buffer.MipLevels=1; buffer.SampleDesc.Count=1; buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR; properties.Type=D3D12_HEAP_TYPE_READBACK;
    Need(device->CreateCommittedResource(&properties,D3D12_HEAP_FLAG_NONE,&buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,IID_PPV_ARGS(&readback)));
    D3D12_TEXTURE_COPY_LOCATION destination{}; destination.pResource=readback.Get(); destination.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT; destination.PlacedFootprint=footprint;
    D3D12_TEXTURE_COPY_LOCATION source{}; source.pResource=textures[3].Get(); source.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX; commands->CopyTextureRegion(&destination,0,0,0,&source,nullptr);
    if(!Submit(commands,queue,fence,fenceValue)) throw std::runtime_error("Neural fixture GPU work did not complete");
    D3D12_RANGE range{0,static_cast<SIZE_T>(total)}; void* mapped=nullptr; Need(readback->Map(0,&range,&mapped));
    unsigned long long checksum=14695981039346656037ull; unsigned changed=0;
    const unsigned sentinel=0xfff003ff;
    for(unsigned y=0;y<rows;++y) for(unsigned x=0;x<1280;++x) {
        auto pixel=static_cast<const BYTE*>(mapped)+footprint.Offset+static_cast<size_t>(y)*footprint.Footprint.RowPitch+x*4;
        unsigned value=0; memcpy(&value,pixel,4); if(value!=sentinel) ++changed;
        for(unsigned byte=0;byte<4;++byte) { checksum^=pixel[byte]; checksum*=1099511628211ull; }
    }
    D3D12_RANGE written{0,0}; readback->Unmap(0,&written);
    printf("{\"event\":\"%s_fixture_gpu_readback\",\"gpu_completed\":true,\"changed_from_sentinel_pixels\":%u,\"pixels\":921600,\"checksum\":\"%016llx\",\"game_started\":false,\"game_acceptance\":false}\n",superResolution?"sr":"neural",changed,checksum); fflush(stdout);
    return result;
}
int wmain(int argc,wchar_t** argv) {
    if(argc<3 || argc>5 || (argc>=4 && wcscmp(argv[3],L"--private-model")) || (argc==5 && wcscmp(argv[4],L"--evaluate-fixture") && wcscmp(argv[4],L"--evaluate-both"))) { puts("Usage: ProbeNeuralRuntime.exe <external-provider-directory> <existing-private-cache-directory> [--private-model [--evaluate-fixture|--evaluate-both]]"); return 2; }
    ComPtr<IDXGIFactory6> factory; ComPtr<IDXGIAdapter1> adapter; ComPtr<ID3D12Device> device;
    if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) return 3;
    for(UINT index=0;factory->EnumAdapterByGpuPreference(index,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter))!=DXGI_ERROR_NOT_FOUND;++index) {
        DXGI_ADAPTER_DESC1 desc{}; adapter->GetDesc1(&desc);
        if(desc.VendorId==0x10de && !(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE) && SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)))) break;
        adapter.Reset();
    }
    if(!device) { puts("{\"event\":\"neural_probe_no_nvidia_d3d12_device\"}"); return 4; }
    ComPtr<ID3D12CommandQueue> queue; D3D12_COMMAND_QUEUE_DESC queueDesc{};
    ComPtr<ID3D12CommandAllocator> allocator; ComPtr<ID3D12GraphicsCommandList> commands; ComPtr<ID3D12Fence> fence;
    if(FAILED(device->CreateCommandQueue(&queueDesc,IID_PPV_ARGS(&queue))) || FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator))) ||
       FAILED(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&commands))) || FAILED(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)))) return 5;
    unsigned init=TwinKartNeuralInitialize(device.Get(),argv[2],argv[1],RuntimeLog,argc>=4); printf("{\"event\":\"neural_probe_init\",\"result\":%u}\n",init); fflush(stdout);
    unsigned create=0;
    if(init==1) { create=TwinKartNeuralCreate(commands.Get(),1280,720); printf("{\"event\":\"neural_probe_create\",\"feature\":18,\"result\":%u,\"game_started\":false,\"evaluate_called\":false,\"visible_output_verified\":false}\n",create); fflush(stdout); }
    TwinKartNeuralState state{sizeof(TwinKartNeuralState)};
    if(TwinKartNeuralGetState(&state)) printf("{\"event\":\"neural_probe_provider\",\"private_provider\":%s,\"private_init\":%u,\"private_populate\":%u}\n",state.privateProvider?"true":"false",state.privateInitializeResult,state.privatePopulateResult);
    if(!Submit(commands.Get(),queue.Get(),fence.Get(),1)) { puts("{\"event\":\"neural_probe_gpu_timeout\"}"); return 9; }
    if(create==1 && argc==5) {
        try {
            bool both=!wcscmp(argv[4],L"--evaluate-both");
            unsigned sr=both?EvaluateFixture(device.Get(),queue.Get(),allocator.Get(),commands.Get(),fence.Get(),true,2):1;
            create=EvaluateFixture(device.Get(),queue.Get(),allocator.Get(),commands.Get(),fence.Get(),false,both?3:2);
            if(sr!=1) create=sr;
        }
        catch(const std::exception& error) { fprintf(stderr,"%s\n",error.what()); return 10; }
    }
    TwinKartNeuralShutdown(); return create==1?0:1;
}
