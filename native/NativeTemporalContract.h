#pragma once
#include <cmath>
#include <cstring>
#include <cstddef>
namespace TwinKartGraphics {
struct NativeRect {
    int left,top,right,bottom;
    unsigned Width() const {return static_cast<unsigned>(right-left);}
    unsigned Height() const {return static_cast<unsigned>(bottom-top);}
    bool Fits(unsigned w,unsigned h) const {
        return left>=0 && top>=0 && right>left && bottom>top &&
            static_cast<unsigned>(right)<=w && static_cast<unsigned>(bottom)<=h;
    }
};
struct NativeTemporalContract {
    NativeRect input{},output{};
    float jitterX=0,jitterY=0,preExposure=0;
};
// Independently mapped exact-build parameter layout. Resource extents may be
// padded: native input/output RECTs, not allocation sizes, define the work.
inline bool DecodeNativeTemporalContract(const void* parameters,std::size_t bytes,const float* view,
    unsigned inputWidth,unsigned inputHeight,unsigned outputWidth,unsigned outputHeight,NativeTemporalContract& out) {
    if(!parameters || bytes<96 || !view)return false;
    float fields[24];memcpy(fields,parameters,sizeof(fields));
    NativeTemporalContract c;
    memcpy(&c.input,static_cast<const char*>(parameters)+16,16);
    memcpy(&c.output,static_cast<const char*>(parameters)+48,16);
    if(!c.input.Fits(inputWidth,inputHeight) || !c.output.Fits(outputWidth,outputHeight) ||
       c.input.Width()<128 || c.input.Height()<128 || c.output.Width()>8192 || c.output.Height()>8192 ||
       c.input.Width()>=c.output.Width() || c.input.Height()>=c.output.Height())return false;
    const unsigned w=c.input.Width(),h=c.input.Height(),ow=c.output.Width(),oh=c.output.Height();
    auto close=[](float a,float b){return std::isfinite(a) && std::fabs(a-b)<=0.0001f*(1+std::fabs(b));};
    if(!close(view[536],static_cast<float>(w)) || !close(view[537],static_cast<float>(h)) ||
       !close(fields[10],2.0f/w) || !close(fields[11],2.0f/h) ||
       !close(fields[16],static_cast<float>(ow)) || !close(fields[17],static_cast<float>(oh)) ||
       !close(fields[18],1-1.0f/ow) || !close(fields[19],1-1.0f/oh) ||
       !close(fields[20],0.5f/ow) || !close(fields[21],0.5f/oh) ||
       !close(fields[22],1-0.5f/ow) || !close(fields[23],1-0.5f/oh) ||
       std::fabs(static_cast<double>(w)*oh-static_cast<double>(h)*ow)>ow)return false;
    if(!std::isfinite(fields[8]) || !std::isfinite(fields[9]) || std::fabs(fields[8])>2 || std::fabs(fields[9])>2 ||
       !close(fields[8],view[520]*w*0.5f) || !close(fields[9],-view[521]*h*0.5f) ||
       !std::isfinite(view[557]) || view[557]<=0 || !std::isfinite(view[558]) || view[558]<=0 ||
       !close(view[557]*view[558],1))return false;
    c.jitterX=-fields[8];c.jitterY=-fields[9];c.preExposure=view[557];out=c;return true;
}
}
