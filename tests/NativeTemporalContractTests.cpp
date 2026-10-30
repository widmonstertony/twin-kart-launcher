#include "../native/NativeTemporalContract.h"
#include <cstdio>
#include <stdexcept>
#include <limits>
#include <fstream>
using namespace TwinKartGraphics;
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void Set(float* p,unsigned index,const NativeRect& rect){memcpy(p+index,&rect,sizeof(rect));}
int main(int argc,char** argv){try{
    float p[NativeTemporalParameterFloats]{},view[576]{};
    Set(p,80,{8,16,647,375}); // Inclusive native sample maxima.
    p[96]=4;p[97]=2;p[98]=1284;p[99]=722;
    p[60]=640.0f/672;p[61]=360.0f/384;p[62]=8.0f/672;p[63]=16.0f/384;
    p[76]=672;p[77]=384;p[78]=1.0f/672;p[79]=1.0f/384;
    p[92]=1280;p[93]=720;p[94]=1.0f/1280;p[95]=1.0f/720;
    view[536]=640;view[537]=360;view[538]=1.0f/640;view[539]=1.0f/360;view[520]=0.5f/640;view[521]=0.25f/360;
    view[557]=2;view[558]=0.5f;
    NativeTemporalContract c;
    auto decode=[&](){return DecodeNativeTemporalContract(p,sizeof(p),view,672,384,1312,736,c);};
    Need(decode() && c.input.Width()==640 && c.output.Width()==1280 && c.input.left==8 && c.output.top==2,"Padded native subrect contract rejected");
    Need(c.jitterX==-0.25f && c.jitterY==0.125f && c.preExposure==2,"Raw temporal jitter/pre-exposure contract changed");
    p[92]=640;Need(!decode(),"Mismatched output rect constants accepted");p[92]=1280;
    view[536]=672;Need(!decode(),"Padded allocation substituted for input viewport");view[536]=640;
    view[558]=1;Need(!decode(),"Inconsistent pre-exposure reciprocal accepted");view[558]=0.5f;
    p[60]=1;Need(!decode(),"Wrong native input UV mapping accepted");p[60]=640.0f/672;
    Set(p,80,{-1,16,638,375});Need(!decode(),"Negative source rectangle accepted");Set(p,80,{8,16,647,375});
    p[96]=4.5f;Need(!decode(),"Fractional output origin accepted");p[96]=4;
    p[94]=std::numeric_limits<float>::quiet_NaN();Need(!decode(),"Nonfinite live native parameters accepted");p[94]=1.0f/1280;
    view[520]=8;Need(!decode(),"Invalid View jitter accepted");view[520]=0.5f/640;
    Need(!DecodeNativeTemporalContract(p,96,view,672,384,1312,736,c),"Old incomplete parameter prefix accepted");
    for(unsigned i=4;i<40;++i)p[i]=std::numeric_limits<float>::quiet_NaN();
    Need(decode(),"Unused loose-buffer gaps were treated as live parameters");
    if(argc==3) {
        float capturedView[2048]{},capturedParams[1024]{};
        auto read=[](const char* path,void* data,std::size_t size){std::ifstream file(path,std::ios::binary);file.read(static_cast<char*>(data),size);return file.gcount()==static_cast<std::streamsize>(size) && file.peek()==std::char_traits<char>::eof();};
        Need(read(argv[1],capturedParams,sizeof(capturedParams)) && read(argv[2],capturedView,sizeof(capturedView)),"Bounded native captures expected");
        Need(DecodeNativeTemporalLayout(capturedParams,sizeof(capturedParams),capturedView,8192,8192,8192,8192,c),"Actual native live-register layout rejected");
        unsigned iw=static_cast<unsigned>(capturedParams[76]),ih=static_cast<unsigned>(capturedParams[77]),ow=c.output.right,oh=c.output.bottom;
        Need(c.input.Width()==c.output.Width() && c.input.Height()==c.output.Height(),"Expected bounded 1:1 native capture");
        Need(DecodeNativeTemporalLayout(capturedParams,NativeTemporalParameterFloats*sizeof(float),capturedView,iw,ih,ow,oh,c),"Production parameter prefix differs from full buffer");
        Need(!DecodeNativeTemporalContract(capturedParams,sizeof(capturedParams),capturedView,iw,ih,ow,oh,c),"Captured 1:1 TAA incorrectly claimed as super resolution");
        puts("PASS previous native capture geometry accepted and explicitly rejected as 1:1 AA, not SR.");
    } else Need(argc==1,"Expected paired external parameter and View captures");
    puts("PASS temporal contract: verified live registers, inclusive bounds, float output rect, native UV mapping, View jitter/exposure, unused-gap independence and invalid-input rejection. No game started.");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
