#include "../native/NativeViewMath.h"
#include "../native/TemporalHistory.h"
#include <cstdio>
#include <stdexcept>
#include <limits>
#include <fstream>
using namespace TwinKartGraphics;
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void Near(float a,float b){Need(fabsf(a-b)<0.0001f,"Native camera matrix/coordinate mismatch");}
int main(int argc,char** argv){try{
    float view[576]{};view[536]=640;view[537]=360;
    float projection[16]={1.125f,0,0,0,0,2,0,0,0,0,0,1,0,0,10,0};
    memcpy(view+144,projection,64);
    const float identity[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    memcpy(view+504,identity,64);
    float worldToView[16];memcpy(worldToView,identity,64);worldToView[12]=-12;worldToView[13]=34;worldToView[14]=-56;
    float noAA[16];MultiplyNativeMatrix(worldToView,projection,noAA);memcpy(view+32,noAA,64);
    view[520]=0.001f;view[521]=-0.002f;
    for(unsigned r=0;r<4;++r){view[32+r*4]+=view[32+r*4+3]*view[520];view[33+r*4]+=view[32+r*4+3]*view[521];}
    CameraFrame camera{};Need(DecodeNativeCamera(view,640,360,true,camera),"Known inverted native perspective rejected");
    Near(camera.position[0],12);Near(camera.position[1],-34);Near(camera.position[2],56);
    Near(camera.right[0],1);Near(camera.up[1],1);Near(camera.forward[2],1);
    Near(camera.jitterX,-0.32f);Near(camera.jitterY,-0.36f);Near(camera.nearPlane,10);
    Near(camera.aspect,640.0f/360);Need(camera.reset && camera.invertedDepth && camera.farPlane==FLT_MAX,"Infinite reversed-Z plane or reset lost");
    float product[16];MultiplyNativeMatrix(camera.viewToClip,camera.clipToView,product);
    for(unsigned i=0;i<16;++i)Near(product[i],identity[i]);
    CameraFrame resolved=ResolvedPresentationCamera(camera,1280,720);
    Near(resolved.jitterX,0);Near(resolved.jitterY,0);Near(resolved.motionScaleX,1.0f/1280);Near(resolved.motionScaleY,1.0f/720);
    Near(camera.jitterX,-0.32f);Near(camera.jitterY,-0.36f);
    TemporalHistory history;
    camera.reset=false;
    Need(history.NeedsReset(1,1,camera),"First temporal frame did not reset");history.Commit(1,1,camera);
    Need(!history.NeedsReset(2,1,camera),"Continuous camera reset unnecessarily");
    Need(history.NeedsReset(2,2,camera),"Cockpit control revision did not reset history");
    Need(history.NeedsReset(3,1,camera),"One rejected or missing frame reused old history");
    CameraFrame cut=camera;cut.position[0]+=1200;
    Need(history.NeedsReset(2,1,cut),"Respawn/teleport did not reset history");
    cut=camera;cut.forward[2]=-1;
    Need(history.NeedsReset(2,1,cut),"Camera orientation cut did not reset history");
    cut=camera;cut.fieldOfView+=0.1f;
    Need(history.NeedsReset(2,1,cut),"FOV change did not reset history");
    cut=camera;cut.position[0]+=20;Need(!history.NeedsReset(2,1,cut),"Normal movement reset history");
    history.Commit(2,1,cut);Need(!history.NeedsReset(3,1,cut),"Successful history commit did not advance");
    Need(!DecodeNativeCamera(view,1280,720,false,camera),"Display dimensions accepted for low-res guides");
    view[144]=0;Need(!DecodeNativeCamera(view,640,360,false,camera),"Singular projection accepted");
    view[144]=std::numeric_limits<float>::quiet_NaN();Need(!DecodeNativeCamera(view,640,360,false,camera),"Nonfinite native matrix accepted");
    if(argc==2) {
        float captured[2048]{};std::ifstream file(argv[1],std::ios::binary);file.read(reinterpret_cast<char*>(captured),sizeof(captured));
        Need(file.gcount()==sizeof(captured) && file.peek()==std::char_traits<char>::eof(),"Expected exact bounded native View capture");
        Need(std::isfinite(captured[536]) && std::isfinite(captured[537]) && captured[536]>=128 && captured[536]<=4096 && captured[537]>=128 && captured[537]<=4096,"Invalid captured viewport");
        Need(DecodeNativeCamera(captured,static_cast<unsigned>(captured[536]),static_cast<unsigned>(captured[537]),true,camera),"Actual captured native camera contract rejected");
        puts("PASS previously captured native camera layout; no live process read and no visual/FG acceptance inferred.");
    } else Need(argc==1,"Expected at most one external camera capture");
    puts("PASS native camera: jitter removal, translated camera position, inverse matrices, dense-motion scale, infinite reversed-Z, mismatched viewport and invalid-matrix rejection. No game started.");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
