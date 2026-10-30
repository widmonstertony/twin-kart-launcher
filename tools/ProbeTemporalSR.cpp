#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdio>
#include <vector>
#include <stdexcept>
#include <thread>
#include "../native/DlssD3D11.h"
#include "../native/ClientBuildGuard.h"
using Microsoft::WRL::ComPtr;
void Need(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int wmain(int argc,wchar_t** argv){HMODULE module=nullptr;decltype(&TwinKartDlssShutdown) shutdown=nullptr;
try {
    Need(argc==5,"Expected original module, lowercase SHA-256, external provider directory and private cache directory.");
    char hash[65]{};Need(wcslen(argv[2])==64,"Expected exact original module hash");
    for(unsigned i=0;i<64;++i){Need((argv[2][i]>=L'0' && argv[2][i]<=L'9') || (argv[2][i]>=L'a' && argv[2][i]<=L'f'),"Invalid module hash");hash[i]=static_cast<char>(argv[2][i]);}
    Need(TwinKartClient::FileHasHash(argv[1],hash),"Original SR module hash mismatch");
    module=LoadLibraryExW(argv[1],nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);Need(module!=nullptr,"Original SR module load failed");
    auto init=reinterpret_cast<decltype(&TwinKartDlssInitialize)>(GetProcAddress(module,"TwinKartDlssInitialize"));
    auto evaluate=reinterpret_cast<decltype(&TwinKartDlssEvaluate)>(GetProcAddress(module,"TwinKartDlssEvaluate"));
    auto getState=reinterpret_cast<decltype(&TwinKartDlssGetState)>(GetProcAddress(module,"TwinKartDlssGetState"));
    shutdown=reinterpret_cast<decltype(&TwinKartDlssShutdown)>(GetProcAddress(module,"TwinKartDlssShutdown"));
    auto drained=reinterpret_cast<decltype(&TwinKartDlssShutdownDrained)>(GetProcAddress(module,"TwinKartDlssShutdownDrained"));
    Need(init && evaluate && getState && shutdown && drained,"Original SR exports unavailable");
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Need(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_HARDWARE,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"Hardware D3D11 device failed");
    unsigned initialized=init(device.Get(),argv[4],argv[3]);
    printf("{\"event\":\"offscreen_temporal_sr_initialize\",\"result\":%u,\"game_started\":false}\n",initialized);
    Need(initialized==1,"Hardware SR initialize failed");
    constexpr unsigned w=640,h=360,ow=1280,oh=720;
    std::vector<unsigned short> colors(w*h*4),velocities(w*h*2);std::vector<float> depths(w*h,0.5f);
    for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){unsigned p=(y*w+x)*4;bool light=((x/16)^(y/16))&1;
        colors[p]=light?0x3c00:0x3400;colors[p+1]=light?0x3800:0x3000;colors[p+2]=0x3400;colors[p+3]=0x3c00;}
    D3D11_TEXTURE2D_DESC desc{};desc.Width=w;desc.Height=h;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> color,depth,motion,output;
    auto create=[&](DXGI_FORMAT format,const void* bytes,unsigned pitch,ComPtr<ID3D11Texture2D>& texture){desc.Format=format;D3D11_SUBRESOURCE_DATA data{bytes,pitch,0};return SUCCEEDED(device->CreateTexture2D(&desc,bytes?&data:nullptr,&texture));};
    Need(create(DXGI_FORMAT_R16G16B16A16_FLOAT,colors.data(),w*8,color) && create(DXGI_FORMAT_R32_FLOAT,depths.data(),w*4,depth) &&
        create(DXGI_FORMAT_R16G16_FLOAT,velocities.data(),w*4,motion),"Synthetic HDR guide resources failed");
    desc.Width=ow;desc.Height=oh;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE|D3D11_BIND_UNORDERED_ACCESS;
    Need(create(DXGI_FORMAT_R16G16B16A16_FLOAT,nullptr,0,output),"SR output resource failed");
    ComPtr<ID3D11UnorderedAccessView> write;Need(SUCCEEDED(device->CreateUnorderedAccessView(output.Get(),nullptr,&write)),"Output clear view failed");
    float empty[4]{};context->ClearUnorderedAccessViewFloat(write.Get(),empty);
    unsigned sentinel[4]={1,2,3,4};D3D11_BUFFER_DESC cb{};cb.ByteWidth=16;cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;D3D11_SUBRESOURCE_DATA data{sentinel,0,0};
    ComPtr<ID3D11Buffer> constants;Need(SUCCEEDED(device->CreateBuffer(&cb,&data,&constants)),"Native state fixture buffer");
    ID3D11Buffer* bound=constants.Get();context->PSSetConstantBuffers(13,1,&bound);
    for(unsigned frame=0;frame<2;++frame) {
        TwinKartTemporalFrame request{sizeof(TwinKartTemporalFrame),context.Get(),color.Get(),depth.Get(),motion.Get(),output.Get(),
            frame?0.25f:0,frame?-0.125f:0,1,1,16.666667f,1,1,frame==0,true,true,true,true};
        unsigned result=evaluate(&request);printf("{\"event\":\"offscreen_temporal_sr_evaluate\",\"frame\":%u,\"result\":%u,\"synthetic_guides\":true}\n",frame,result);
        Need(result==1,"Hardware SR evaluation failed");
        ComPtr<ID3D11Buffer> restored;context->PSGetConstantBuffers(13,1,&restored);Need(restored.Get()==constants.Get(),"SR consumer failed to restore existing native state");
    }
    D3D11_QUERY_DESC queryDesc{D3D11_QUERY_EVENT,0};ComPtr<ID3D11Query> completion;Need(SUCCEEDED(device->CreateQuery(&queryDesc,&completion)),"GPU completion query");
    context->End(completion.Get());context->Flush();ULONGLONG start=GetTickCount64();BOOL complete=FALSE;HRESULT hr;
    do {hr=context->GetData(completion.Get(),&complete,sizeof(complete),0);Need(SUCCEEDED(hr),"GPU completion failed");if(hr==S_FALSE)Sleep(1);}while(hr==S_FALSE && GetTickCount64()-start<10000);
    Need(hr==S_OK && complete,"Bounded GPU completion timeout");
    output->GetDesc(&desc);desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;
    ComPtr<ID3D11Texture2D> copy;Need(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&copy)),"Output readback allocation");context->CopyResource(copy.Get(),output.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};Need(SUCCEEDED(context->Map(copy.Get(),0,D3D11_MAP_READ,0,&mapped)),"Hardware output GPU readback failed");
    unsigned changed=0,nonfinite=0;unsigned long long digest=14695981039346656037ull;
    for(unsigned y=0;y<oh;++y)for(unsigned x=0;x<ow;++x) {
        auto pixel=reinterpret_cast<const unsigned short*>(static_cast<const BYTE*>(mapped.pData)+y*mapped.RowPitch)+x*4;
        if(pixel[0] || pixel[1] || pixel[2])++changed;
        for(unsigned c=0;c<3;++c){if((pixel[c]&0x7c00)==0x7c00)++nonfinite;digest=(digest^pixel[c])*1099511628211ull;}
    }
    context->Unmap(copy.Get(),0);Need(changed==ow*oh && nonfinite==0,"SR did not produce complete finite HDR output");
    TwinKartDlssState state{sizeof(TwinKartDlssState)};Need(getState(&state) && state.successfulEvaluations==2,"Actual SR evaluation state mismatch");
    printf("{\"event\":\"offscreen_temporal_sr_readback\",\"render_width\":640,\"render_height\":360,\"output_width\":1280,\"output_height\":720,\"changed_pixels\":%u,\"nonfinite_channels\":%u,\"output_digest\":\"%016llx\",\"hardware_gpu_completed\":true,\"native_state_restored\":true,\"synthetic_guides\":true,\"game_started\":false,\"in_game_sr_verified\":false}\n",changed,nonfinite,digest);
    DWORD firstOwner=GetCurrentThreadId();
    ComPtr<ID3D11DeviceContext> deferred;Need(SUCCEEDED(device->CreateDeferredContext(0,&deferred)),"Deferred context negative fixture");
    Need(!drained(0,context.Get()) && !drained(firstOwner+1,context.Get()) && !drained(firstOwner,nullptr) && !drained(firstOwner,deferred.Get()),"Invalid temporal retirement accepted");
    bool workerOk=false;DWORD workerOwner=0;
    std::thread worker([&]{
        workerOwner=GetCurrentThreadId();
        TwinKartTemporalFrame request{sizeof(TwinKartTemporalFrame),context.Get(),color.Get(),depth.Get(),motion.Get(),output.Get(),
            0,0,1,1,16.666667f,1,1,true,true,true,true,true};
        TwinKartDlssState other{sizeof(TwinKartDlssState)};
        workerOk=evaluate(&request)==0xf0000002 && !getState(&other) && drained(firstOwner,context.Get()) &&
            init(device.Get(),argv[4],argv[3])==1 && evaluate(&request)==1 && getState(&other) && other.successfulEvaluations==1;
    });worker.join();Need(workerOk && workerOwner!=firstOwner,"Explicit drained temporal ownership rebuild failed");
    Need(drained(workerOwner,context.Get()) && init(device.Get(),argv[4],argv[3])==1,"Return to original temporal render owner failed");
    TwinKartTemporalFrame rebuilt{sizeof(TwinKartTemporalFrame),context.Get(),color.Get(),depth.Get(),motion.Get(),output.Get(),
        0,0,1,1,16.666667f,1,1,true,true,true,true,true};
    Need(evaluate(&rebuilt)==1 && getState(&state) && state.successfulEvaluations==1,"Returned owner temporal feature failed");
    printf("{\"event\":\"offscreen_temporal_sr_drained_rebuild\",\"first_owner\":%lu,\"new_owner\":%lu,\"rebuilds\":2,\"evaluations\":4,\"invalid_retirement_rejected\":true,\"foreign_evaluate_rejected\":true,\"game_started\":false}\n",firstOwner,workerOwner);
    Need(drained(firstOwner,context.Get()),"Final temporal GPU drain failed");
    context->ClearState();shutdown=nullptr;FreeLibrary(module);return 0;
}catch(const std::exception& e){if(shutdown)shutdown();if(module)FreeLibrary(module);fprintf(stderr,"%s\n",e.what());return 1;}}
