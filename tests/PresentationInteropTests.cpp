#define TWIN_KART_PRESENTATION_TEST 1
#include "../native/NativePresentation.cpp"
#include <stdexcept>
using namespace TwinKartGraphics;
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){HWND window=nullptr;try{
    WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"TwinKartPresentationInteropFixture";
    Need(RegisterClassW(&wc)!=0,"Fixture class");
    window=CreateWindowExW(0,wc.lpszClassName,L"",WS_POPUP,0,0,256,128,nullptr,nullptr,wc.hInstance,nullptr);
    Need(window!=nullptr,"Hidden fixture window");
    DXGI_SWAP_CHAIN_DESC desc{};desc.BufferDesc.Width=256;desc.BufferDesc.Height=128;desc.BufferDesc.Format=DXGI_FORMAT_R10G10B10A2_UNORM;
    desc.BufferCount=1;desc.SampleDesc.Count=1;desc.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;desc.OutputWindow=window;desc.Windowed=TRUE;
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;ComPtr<IDXGISwapChain> original;
    Need(SUCCEEDED(D3D11CreateDeviceAndSwapChain(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&desc,&original,&device,nullptr,&context)),"WARP fixture swapchain");
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
    context->ClearState();DestroyWindow(window);window=nullptr;UnregisterClassW(wc.lpszClassName,wc.hInstance);
    puts("PASS hidden WARP native presentation boundary: COM identity, D3D11 device/buffer routing, private display isolation, descriptor contract, held-resource resize rejection. No game or NVIDIA FG runtime started.");return 0;
}catch(const std::exception& e){if(window)DestroyWindow(window);fprintf(stderr,"%s\n",e.what());return 1;}}
