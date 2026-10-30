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
inline constexpr unsigned NativeTemporalParameterFloats=100;
// Independently mapped exact-build parameter layout. Resource extents may be
// padded: native input/output RECTs, not allocation sizes, define the work.
inline bool DecodeNativeTemporalLayout(const void* parameters,std::size_t bytes,const float* view,
    unsigned inputWidth,unsigned inputHeight,unsigned outputWidth,unsigned outputHeight,NativeTemporalContract& out) {
    if(!parameters || bytes<NativeTemporalParameterFloats*sizeof(float) || !view)return false;
    float fields[NativeTemporalParameterFloats];memcpy(fields,parameters,sizeof(fields));
    NativeTemporalContract c;
    // Exact shader reads cb0[20] as inclusive integer sample bounds and
    // cb0[24] as floating output min/end coordinates. cb0[1..9] are unused
    // gaps in this permutation; stale data there is not an input contract.
    memcpy(&c.input,fields+80,sizeof(c.input));
    if(c.input.right<0 || c.input.bottom<0 || c.input.right>=8192 || c.input.bottom>=8192)return false;
    ++c.input.right;++c.input.bottom;
    auto coordinate=[](float value,int& out){if(!std::isfinite(value) || value<0 || value>8192 || std::floor(value)!=value)return false;out=static_cast<int>(value);return true;};
    if(!coordinate(fields[96],c.output.left) || !coordinate(fields[97],c.output.top) ||
       !coordinate(fields[98],c.output.right) || !coordinate(fields[99],c.output.bottom))return false;
    if(!c.input.Fits(inputWidth,inputHeight) || !c.output.Fits(outputWidth,outputHeight) ||
       c.input.Width()<128 || c.input.Height()<128 || c.output.Width()>8192 || c.output.Height()>8192 ||
       c.input.Width()>c.output.Width() || c.input.Height()>c.output.Height())return false;
    const unsigned w=c.input.Width(),h=c.input.Height(),ow=c.output.Width(),oh=c.output.Height();
    auto close=[](float a,float b){return std::isfinite(a) && std::fabs(a-b)<=0.0001f*(1+std::fabs(b));};
    int allocationWidth=0,allocationHeight=0;
    if(!coordinate(fields[76],allocationWidth) || !coordinate(fields[77],allocationHeight) ||
       !c.input.Fits(allocationWidth,allocationHeight) || static_cast<unsigned>(allocationWidth)>inputWidth || static_cast<unsigned>(allocationHeight)>inputHeight)return false;
    if(!close(view[536],static_cast<float>(w)) || !close(view[537],static_cast<float>(h)) ||
       !close(view[538]*w,1) || !close(view[539]*h,1) ||
       !close(fields[78]*allocationWidth,1) || !close(fields[79]*allocationHeight,1) ||
       !close(fields[60]*allocationWidth,static_cast<float>(w)) || !close(fields[61]*allocationHeight,static_cast<float>(h)) ||
       !close(fields[62]*allocationWidth,static_cast<float>(c.input.left)) || !close(fields[63]*allocationHeight,static_cast<float>(c.input.top)) ||
       !close(fields[92],static_cast<float>(ow)) || !close(fields[93],static_cast<float>(oh)) ||
       !close(fields[94]*ow,1) || !close(fields[95]*oh,1) ||
       std::fabs(static_cast<double>(w)*oh-static_cast<double>(h)*ow)>ow)return false;
    float jitterX=-view[520]*w*0.5f,jitterY=view[521]*h*0.5f;
    if(!std::isfinite(jitterX) || !std::isfinite(jitterY) || std::fabs(jitterX)>2 || std::fabs(jitterY)>2 ||
       !std::isfinite(view[557]) || view[557]<=0 || !std::isfinite(view[558]) || view[558]<=0 ||
       !close(view[557]*view[558],1))return false;
    c.jitterX=jitterX;c.jitterY=jitterY;c.preExposure=view[557];out=c;return true;
}
// Valid 1:1 native AA still supplies depth/camera guides, but is never SR.
inline bool DecodeNativeTemporalContract(const void* parameters,std::size_t bytes,const float* view,
    unsigned inputWidth,unsigned inputHeight,unsigned outputWidth,unsigned outputHeight,NativeTemporalContract& out) {
    NativeTemporalContract c;
    if(!DecodeNativeTemporalLayout(parameters,bytes,view,inputWidth,inputHeight,outputWidth,outputHeight,c) ||
       c.input.Width()>=c.output.Width() || c.input.Height()>=c.output.Height())return false;
    out=c;return true;
}
}
