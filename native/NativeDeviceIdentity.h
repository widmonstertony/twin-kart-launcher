#pragma once
#include <unknwn.h>
#include <wrl/client.h>
namespace TwinKartGraphics {
// Documented Streamline native-interface QI GUID (Programming Guide 2.16).
// Compare COM identities after unwrapping, never just adapter LUIDs.
inline bool SameNativeGraphicsDevice(IUnknown* first,IUnknown* second) {
    if(!first || !second)return false;
    const GUID nativeId{0xadec44e2,0x61f0,0x45c3,{0xad,0x9f,0x1b,0x37,0x37,0x92,0x84,0xff}};
    auto identity=[&](IUnknown* input,Microsoft::WRL::ComPtr<IUnknown>& result) {
        Microsoft::WRL::ComPtr<IUnknown> native;
        HRESULT unwrapped=input->QueryInterface(nativeId,reinterpret_cast<void**>(native.GetAddressOf()));
        if(FAILED(unwrapped) && unwrapped!=E_NOINTERFACE)return false;
        if(SUCCEEDED(unwrapped) && !native)return false;
        return SUCCEEDED((native?native.Get():input)->QueryInterface(IID_PPV_ARGS(&result)));
    };
    Microsoft::WRL::ComPtr<IUnknown> a,b;
    return identity(first,a) && identity(second,b) && a.Get()==b.Get();
}
}
