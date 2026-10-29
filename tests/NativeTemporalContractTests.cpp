#include "../native/NativeTemporalContract.h"
#include <cstdio>
#include <stdexcept>
#include <limits>
#include <fstream>
using namespace TwinKartGraphics;
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void Set(float* p,unsigned index,const NativeRect& rect){memcpy(p+index,&rect,sizeof(rect));}
int main(int argc,char** argv){try{
    float p[24]{},view[576]{};
    Set(p,4,{8,16,648,376});Set(p,12,{4,2,1284,722});
    p[8]=0.25f;p[9]=-0.125f;p[10]=2.0f/640;p[11]=2.0f/360;
    p[16]=1280;p[17]=720;p[18]=1-1.0f/1280;p[19]=1-1.0f/720;
    p[20]=0.5f/1280;p[21]=0.5f/720;p[22]=1-0.5f/1280;p[23]=1-0.5f/720;
    view[536]=640;view[537]=360;view[520]=0.5f/640;view[521]=0.25f/360;
    view[557]=2;view[558]=0.5f;
    NativeTemporalContract c;
    auto decode=[&](){return DecodeNativeTemporalContract(p,sizeof(p),view,672,384,1312,736,c);};
    Need(decode() && c.input.Width()==640 && c.output.Width()==1280 && c.input.left==8 && c.output.top==2,"Padded native subrect contract rejected");
    Need(c.jitterX==-0.25f && c.jitterY==0.125f && c.preExposure==2,"Raw temporal jitter/pre-exposure contract changed");
    p[16]=640;Need(!decode(),"Mismatched output rect constants accepted");p[16]=1280;
    view[536]=672;Need(!decode(),"Padded allocation substituted for input viewport");view[536]=640;
    view[558]=1;Need(!decode(),"Inconsistent pre-exposure reciprocal accepted");view[558]=0.5f;
    p[8]=-0.25f;Need(!decode(),"Mismatched native jitter sign accepted");p[8]=0.25f;
    Set(p,4,{-1,16,639,376});Need(!decode(),"Negative source rectangle accepted");Set(p,4,{8,16,648,376});
    p[20]=std::numeric_limits<float>::quiet_NaN();Need(!decode(),"Nonfinite native parameters accepted");p[20]=0.5f/1280;
    Need(!DecodeNativeTemporalContract(p,80,view,672,384,1312,736,c),"Truncated constant buffer accepted");
    if(argc==3) {
        float capturedView[2048]{},capturedParams[1024]{};
        auto read=[](const char* path,void* data,std::size_t size){std::ifstream file(path,std::ios::binary);file.read(static_cast<char*>(data),size);return file.gcount()==static_cast<std::streamsize>(size) && file.peek()==std::char_traits<char>::eof();};
        Need(read(argv[1],capturedParams,sizeof(capturedParams)) && read(argv[2],capturedView,sizeof(capturedView)),"Bounded native captures expected");
        Need(!DecodeNativeTemporalContract(capturedParams,sizeof(capturedParams),capturedView,3840,2400,3840,2400,c),"Captured 1:1 TAA incorrectly claimed as super resolution");
        puts("PASS previous native capture explicitly rejected as 1:1 AA, not SR.");
    } else Need(argc==1,"Expected paired external parameter and View captures");
    puts("PASS temporal SR contract: padded crop rectangles, real upsample ratio, jitter sign, reciprocal exposure, constants and invalid-input rejection. No game started.");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
