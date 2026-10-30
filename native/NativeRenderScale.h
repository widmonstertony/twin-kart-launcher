#pragma once
#include <cmath>
#include <cwchar>
namespace TwinKartGraphics {
// Process-local rendering request. Only the original engine may propagate the
// cvar to its render thread; never write its two thread copies directly.
class NativeRenderScale {
    float percentage=0;
public:
    enum class Result {Disabled,NotReady,Applied,AlreadyApplied,Overridden};
    using SetFloat=void(__fastcall*)(void*,float,unsigned);
    using GetFraction=float(__fastcall*)();
    static constexpr unsigned SetByCode=0x08000000;
    bool Configure(const wchar_t* value) {
        percentage=0;if(!value || !*value || wcslen(value)>16)return false;
        for(auto p=value;*p;++p)if((*p<L'0' || *p>L'9') && *p!=L'.')return false;
        wchar_t* end=nullptr;double number=wcstod(value,&end);
        if(!end || *end || !std::isfinite(number) || number<25 || number>=100)return false;
        percentage=static_cast<float>(number);return true;
    }
    bool Enabled() const{return percentage>0;}
    float Requested() const{return percentage;}
    Result Apply(void* variable,SetFloat set,GetFraction read,float& observed) const {
        observed=0;if(!Enabled())return Result::Disabled;
        if(!variable || !set || !read)return Result::NotReady;
        observed=read();
        auto matches=[&]{return std::isfinite(observed) && std::fabs(observed*100-percentage)<=0.001f;};
        if(matches())return Result::AlreadyApplied;
        set(variable,percentage,SetByCode);observed=read();
        return matches()?Result::Applied:Result::Overridden;
    }
};
}
