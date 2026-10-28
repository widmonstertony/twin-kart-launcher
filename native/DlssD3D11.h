#pragma once
#include <d3d11.h>
#ifdef TWIN_KART_DLSS_BUILD
#define TK_DLSS extern "C" __declspec(dllexport)
#else
#define TK_DLSS extern "C" __declspec(dllimport)
#endif

// Frame resources must be supplied by a verified pre-HUD temporal reconstruction
// adapter on the owning immediate-context thread. This is not a Present filter.
struct TwinKartTemporalFrame {
    unsigned size;
    ID3D11DeviceContext* context;
    ID3D11Texture2D* color;
    ID3D11Texture2D* depth;
    ID3D11Texture2D* motion;
    ID3D11Texture2D* output;
    float jitterX, jitterY, motionScaleX, motionScaleY, frameMilliseconds;
    float preExposure, exposureScale;
    bool reset, hdr, invertedDepth;
    bool guidesValidated, hudlessValidated;
};
struct TwinKartDlssState {
    unsigned size;
    unsigned initializeResult, lastEvaluateResult;
    unsigned inputWidth, inputHeight, outputWidth, outputHeight;
    unsigned long long successfulEvaluations;
    bool srAvailable, finalCompositionVerified;
};
TK_DLSS unsigned TwinKartDlssInitialize(ID3D11Device* device, const wchar_t* cacheDirectory, const wchar_t* providerDirectory);
TK_DLSS unsigned TwinKartDlssEvaluate(const TwinKartTemporalFrame* frame);
TK_DLSS bool TwinKartDlssGetState(TwinKartDlssState* state);
TK_DLSS void TwinKartDlssShutdown();
