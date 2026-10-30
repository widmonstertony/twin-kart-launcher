#include "../native/NativeTemporalPass.cpp"
#include <stdexcept>
#include <thread>
using namespace TwinKartGraphics;
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
unsigned evaluations=0;
bool reject=false;
bool refuseDrain=true;
unsigned drains=0;
bool FixtureDrain(unsigned owner,ID3D11DeviceContext* context) {
    Need(owner==runtime.thread && context==runtime.context.Get(),"Retirement lost exact old owner/context");
    ++drains;return !refuseDrain;
}
unsigned FixtureEvaluate(const TwinKartTemporalFrame* f) {
    ++evaluations;
    Need(f && f->hdr && f->invertedDepth && f->guidesValidated && f->hudlessValidated &&
        f->preExposure==2 && f->jitterX==-0.25f && f->jitterY==0.125f && f->motionScaleX==1 && f->motionScaleY==1,
        "Actual temporal consumer request lost guides/exposure/jitter");
    Need(f->reset==(evaluations==1),"Continuous temporal frames did not preserve/reset history correctly");
    if(reject)return 0xf0000001;
    ComPtr<ID3D11Device> device;f->context->GetDevice(&device);ComPtr<ID3D11UnorderedAccessView> target;
    Need(SUCCEEDED(device->CreateUnorderedAccessView(f->output,nullptr,&target)),"Fixture output view");
    float red[4]={1,0,0,1};f->context->ClearUnorderedAccessViewFloat(target.Get(),red);return 1;
}
int main(){try{
    ComPtr<ID3D11Device> device;ComPtr<ID3D11DeviceContext> context;
    Need(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&device,nullptr,&context)),"Temporal WARP device");
    Need(InitializeContext(context.Get()),"Actual conversion/context isolation initialization");runtime.evaluate=FixtureEvaluate;
    D3D11_TEXTURE2D_DESC desc{};desc.Width=672;desc.Height=384;desc.MipLevels=desc.ArraySize=desc.SampleDesc.Count=1;desc.BindFlags=D3D11_BIND_SHADER_RESOURCE;
    ComPtr<ID3D11Texture2D> color,depth,motion;ComPtr<ID3D11ShaderResourceView> colorView,depthView,motionView;
    auto read=[&](DXGI_FORMAT format,DXGI_FORMAT binding,ComPtr<ID3D11Texture2D>& texture,ComPtr<ID3D11ShaderResourceView>& view){
        desc.Format=format;Need(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&texture)),"Temporal input allocation");
        D3D11_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=binding;srv.ViewDimension=D3D11_SRV_DIMENSION_TEXTURE2D;srv.Texture2D.MipLevels=1;
        Need(SUCCEEDED(device->CreateShaderResourceView(texture.Get(),&srv,&view)),"Temporal input view");
    };
    read(DXGI_FORMAT_R11G11B10_FLOAT,DXGI_FORMAT_R11G11B10_FLOAT,color,colorView);
    desc.Width=768;desc.Height=416;
    read(DXGI_FORMAT_R32G8X24_TYPELESS,DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS,depth,depthView);
    desc.Width=704;desc.Height=400;
    read(DXGI_FORMAT_R16G16B16A16_UNORM,DXGI_FORMAT_R16G16B16A16_UNORM,motion,motionView);
    desc.Width=1312;desc.Height=736;desc.Format=DXGI_FORMAT_R16G16B16A16_FLOAT;desc.BindFlags=D3D11_BIND_UNORDERED_ACCESS;
    ComPtr<ID3D11Texture2D> output;ComPtr<ID3D11UnorderedAccessView> outputView;
    Need(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&output)) && SUCCEEDED(device->CreateUnorderedAccessView(output.Get(),nullptr,&outputView)),"Padded native output");
    float black[4]{};context->ClearUnorderedAccessViewFloat(outputView.Get(),black);
    float p[1024]{},v[2048]{};NativeRect in{8,16,648,376},out{4,2,1284,722},inclusive{8,16,647,375};memcpy(p+80,&inclusive,sizeof(inclusive));
    p[96]=4;p[97]=2;p[98]=1284;p[99]=722;p[60]=640.0f/672;p[61]=360.0f/384;p[62]=8.0f/672;p[63]=16.0f/384;
    p[76]=672;p[77]=384;p[78]=1.0f/672;p[79]=1.0f/384;p[92]=1280;p[93]=720;p[94]=1.0f/1280;p[95]=1.0f/720;
    v[536]=640;v[537]=360;v[538]=1.0f/640;v[539]=1.0f/360;v[520]=0.5f/640;v[521]=0.25f/360;v[557]=2;v[558]=0.5f;
    float projection[16]={1.125f,0,0,0,0,2,0,0,0,0,0,1,0,0,10,0};memcpy(v+144,projection,64);memcpy(v+32,projection,64);
    v[504]=v[509]=v[514]=v[519]=1;
    for(unsigned r=0;r<4;++r){v[32+r*4]+=v[35+r*4]*v[520];v[33+r*4]+=v[35+r*4]*v[521];}
    D3D11_BUFFER_DESC cb{};cb.ByteWidth=sizeof(p);cb.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
    D3D11_SUBRESOURCE_DATA data{p,0,0};ComPtr<ID3D11Buffer> params,view;
    Need(SUCCEEDED(device->CreateBuffer(&cb,&data,&params)),"Native temporal params");cb.ByteWidth=sizeof(v);data.pSysMem=v;
    Need(SUCCEEDED(device->CreateBuffer(&cb,&data,&view)),"Native temporal view");
    const char source[]="RWTexture2D<float4> output:register(u0);[numthreads(8,4,1)]void main(uint3 p:SV_DispatchThreadID){output[p.xy]=float4(0,1,0,1);}";
    ComPtr<ID3DBlob> code;ComPtr<ID3D11ComputeShader> shader;
    Need(SUCCEEDED(D3DCompile(source,sizeof(source),nullptr,nullptr,nullptr,"main","cs_5_0",0,0,&code,nullptr)) &&
        SUCCEEDED(device->CreateComputeShader(code->GetBufferPointer(),code->GetBufferSize(),nullptr,&shader)) &&
        TwinKartRender::TagShader(shader.Get(),code->GetBufferPointer(),code->GetBufferSize()),"Original compute fixture and reflected work groups");
    ID3D11Buffer* constants[]={params.Get(),view.Get()};context->CSSetConstantBuffers(0,2,constants);
    ID3D11ShaderResourceView* reads[5]={nullptr,colorView.Get(),nullptr,depthView.Get(),motionView.Get()};context->CSSetShaderResources(0,5,reads);
    ID3D11UnorderedAccessView* write=outputView.Get();context->CSSetUnorderedAccessViews(0,1,&write,nullptr);context->CSSetShader(shader.Get(),nullptr,0);
    Need(!ProcessQualifiedTemporalPass(context.Get(),shader.Get(),10,160,90,1) && evaluations==0,"Wrong native group count accepted");
    Need(ProcessQualifiedTemporalPass(context.Get(),shader.Get(),10,160,180,1) && evaluations==1,"Qualified pre-AA SR replacement failed");
    ComPtr<ID3D11ComputeShader> restored;ComPtr<ID3D11ShaderResourceView> restoredColor;ComPtr<ID3D11UnorderedAccessView> restoredOutput;
    context->CSGetShader(&restored,nullptr,nullptr);context->CSGetShaderResources(1,1,&restoredColor);context->CSGetUnorderedAccessViews(0,1,&restoredOutput);
    Need(restored.Get()==shader.Get() && restoredColor.Get()==colorView.Get() && restoredOutput.Get()==outputView.Get(),"Native dispatch state lost after actual conversion/copy");
    desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;ComPtr<ID3D11Texture2D> copy;
    Need(SUCCEEDED(device->CreateTexture2D(&desc,nullptr,&copy)),"Padded output readback");context->CopyResource(copy.Get(),output.Get());
    D3D11_MAPPED_SUBRESOURCE mapped{};Need(SUCCEEDED(context->Map(copy.Get(),0,D3D11_MAP_READ,0,&mapped)),"GPU output map");bool correct=true;
    for(unsigned y=0;y<736;++y)for(unsigned x=0;x<1312;++x) {
        auto pixel=reinterpret_cast<const unsigned short*>(static_cast<const BYTE*>(mapped.pData)+y*mapped.RowPitch)+x*4;
        bool inside=x>=4 && x<1284 && y>=2 && y<722;if(pixel[0]!=(inside?0x3c00:0))correct=false;
    }
    context->Unmap(copy.Get(),0);Need(correct,"SR copy missed native output rectangle or overwrote padding");
    reject=true;Need(!ProcessQualifiedTemporalPass(context.Get(),shader.Get(),11,160,180,1) && evaluations==2 && !NativeTemporalSuperResolutionRan(11),"Consumer failure falsely suppressed native fallback");
    context->CSGetShader(&restored,nullptr,nullptr);Need(restored.Get()==shader.Get(),"Consumer failure did not restore native state");
    runtime.shutdownDrained=FixtureDrain;
    Need(RetireNativeTemporalForRenderThread(context.Get()) && drains==0 && runtime.device.Get()==device.Get(),"Same owner unnecessarily destroyed temporal feature");
    ComPtr<ID3D11DeviceContext> deferred;Need(SUCCEEDED(device->CreateDeferredContext(0,&deferred)),"Deferred retirement fixture");
    Need(!RetireNativeTemporalForRenderThread(deferred.Get()) && drains==0,"Deferred context permitted retirement");
    bool ok=false;
    std::thread foreign([&]{
        ok=!ProcessQualifiedTemporalPass(context.Get(),shader.Get(),12,160,180,1) && evaluations==2 &&
            !RetireNativeTemporalForRenderThread(context.Get()) && runtime.lastOutput==10;
    });foreign.join();Need(ok && drains==1,"Foreign draw adopted ownership or failed retirement dropped history");
    AcquireSRWLockExclusive(&runtimeLock);
    std::thread busy([&]{ok=!RetireNativeTemporalForRenderThread(context.Get());});busy.join();
    ReleaseSRWLockExclusive(&runtimeLock);Need(ok && drains==1,"Concurrent temporal operation permitted retirement");
    refuseDrain=false;
    std::thread retire([&]{ok=RetireNativeTemporalForRenderThread(context.Get());});retire.join();
    Need(ok && drains==2 && !runtime.device && !runtime.context && !runtime.lastOutput && !failed,"Drained retirement retained stale temporal state");
    reject=false;Need(InitializeContext(context.Get()),"New owner context rebuild failed");runtime.evaluate=FixtureEvaluate;
    evaluations=0;
    Need(ProcessQualifiedTemporalPass(context.Get(),shader.Get(),12,160,180,1) && evaluations==1,"Rebuilt temporal owner did not reset history");
    context->ClearState();
    puts("PASS actual WARP temporal boundary: native group qualification, HDR/crop conversion, consumer request, exact output subrect/padding, full state restoration, failure fallback, foreign callback rejection, serialized drained retirement and history rebuild. Fixture consumer only; no DLSS or game acceptance.");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
