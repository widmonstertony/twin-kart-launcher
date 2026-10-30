#define WIN32_LEAN_AND_MEAN
#define TWIN_KART_DLSS_BUILD
#include <windows.h>
#include <d3d11_1.h>
#include <cmath>
#include "DlssD3D11.h"
// These headers and the NGX import library are external NVIDIA SDK dependencies.
// No NVIDIA SDK implementation or model is copied into this project's source.
#include <nvsdk_ngx_helpers_d3d.h>

namespace {
ID3D11Device* device = nullptr;
ID3D11Device1* device1 = nullptr;
ID3DDeviceContextState* isolatedState = nullptr;
NVSDK_NGX_Parameter* parameters = nullptr;
NVSDK_NGX_Handle* feature = nullptr;
TwinKartDlssState status{sizeof(TwinKartDlssState)};
DWORD ownerThread = 0;
bool initialized = false;
int featureFlags = 0;
SRWLOCK apiLock = SRWLOCK_INIT;
struct ConsumerCall {
    bool entered = TryAcquireSRWLockExclusive(&apiLock) != FALSE;
    ~ConsumerCall() { if (entered) ReleaseSRWLockExclusive(&apiLock); }
};
constexpr unsigned InvalidFrame = 0xF0000001, WrongThread = 0xF0000002, MissingGuides = 0xF0000003;
bool SameDevice(ID3D11DeviceChild* resource) {
    ID3D11Device* found = nullptr; resource->GetDevice(&found);
    bool same = found == device; if (found) found->Release(); return same;
}
NVSDK_NGX_Result DropFeature() {
    if (feature) {
        auto result = NVSDK_NGX_D3D11_ReleaseFeature(feature);
        if (NVSDK_NGX_FAILED(result)) return result;
        feature = nullptr;
    }
    return NVSDK_NGX_Result_Success;
}
}

unsigned TwinKartDlssInitialize(ID3D11Device* selected, const wchar_t* cache, const wchar_t* provider) {
    ConsumerCall call; if (!call.entered) return InvalidFrame;
    if (device || !selected || !cache || !cache[0] || !provider || !provider[0]) return InvalidFrame;
    ownerThread = GetCurrentThreadId();
    device = selected; device->AddRef();
    const wchar_t* paths[] = {provider};
    NVSDK_NGX_FeatureCommonInfo common{};
    common.PathListInfo.Path = paths; common.PathListInfo.Length = 1;
    NVSDK_NGX_Result result = NVSDK_NGX_D3D11_Init_with_ProjectID(
        "45d35067-277b-4e3f-b2f6-e7dd197c609e", NVSDK_NGX_ENGINE_TYPE_CUSTOM,
        "TwinKart-Drift-D3D11", cache, device, &common);
    status.initializeResult = static_cast<unsigned>(result);
    if (NVSDK_NGX_FAILED(result)) return status.initializeResult;
    initialized = true;
    result = NVSDK_NGX_D3D11_GetCapabilityParameters(&parameters);
    if (NVSDK_NGX_FAILED(result)) { status.initializeResult = static_cast<unsigned>(result); return status.initializeResult; }
    int available = 0;
    parameters->Get(NVSDK_NGX_Parameter_SuperSampling_Available, &available);
    status.srAvailable = available != 0;
    if (FAILED(device->QueryInterface(__uuidof(ID3D11Device1), reinterpret_cast<void**>(&device1)))) return InvalidFrame;
    D3D_FEATURE_LEVEL levels[] = { device->GetFeatureLevel() };
    D3D_FEATURE_LEVEL chosen;
    HRESULT hr = device1->CreateDeviceContextState(0, levels, 1, D3D11_SDK_VERSION,
        __uuidof(ID3D11Device), &chosen, &isolatedState);
    if (FAILED(hr)) return static_cast<unsigned>(hr);
    return static_cast<unsigned>(result);
}

unsigned TwinKartDlssEvaluate(const TwinKartTemporalFrame* frame) {
    ConsumerCall call; if (!call.entered) return InvalidFrame;
    if (GetCurrentThreadId() != ownerThread) return WrongThread;
    if (!frame || frame->size != sizeof(*frame) || !initialized || !status.srAvailable || !parameters || !isolatedState ||
        !frame->context || !frame->color || !frame->depth || !frame->motion || !frame->output) return InvalidFrame;
    if (!frame->guidesValidated || !frame->hudlessValidated) return MissingGuides;
    if (frame->context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE ||
        !SameDevice(frame->context) || !SameDevice(frame->color) || !SameDevice(frame->depth) ||
        !SameDevice(frame->motion) || !SameDevice(frame->output)) return InvalidFrame;
    if (!std::isfinite(frame->jitterX) || !std::isfinite(frame->jitterY) ||
        !std::isfinite(frame->motionScaleX) || !std::isfinite(frame->motionScaleY) ||
        frame->motionScaleX == 0 || frame->motionScaleY == 0 ||
        !std::isfinite(frame->frameMilliseconds) || frame->frameMilliseconds <= 0 ||
        !std::isfinite(frame->preExposure) || frame->preExposure <= 0 ||
        !std::isfinite(frame->exposureScale) || frame->exposureScale <= 0) return InvalidFrame;
    D3D11_TEXTURE2D_DESC color{}, depth{}, motion{}, output{};
    frame->color->GetDesc(&color); frame->depth->GetDesc(&depth);
    frame->motion->GetDesc(&motion); frame->output->GetDesc(&output);
    if (!color.Width || !color.Height || color.Width >= output.Width || color.Height >= output.Height ||
        depth.Width != color.Width || depth.Height != color.Height || motion.Width != color.Width || motion.Height != color.Height ||
        color.SampleDesc.Count != 1 || depth.SampleDesc.Count != 1 || motion.SampleDesc.Count != 1 || output.SampleDesc.Count != 1 ||
        !(output.BindFlags & D3D11_BIND_UNORDERED_ACCESS) || frame->color == frame->output || frame->depth == frame->output || frame->motion == frame->output)
        return InvalidFrame;
    ID3D11DeviceContext1* context = nullptr;
    if (FAILED(frame->context->QueryInterface(__uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(&context)))) return InvalidFrame;
    ID3DDeviceContextState* saved = nullptr;
    context->SwapDeviceContextState(isolatedState, &saved);
    int flags = NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
    flags |= NVSDK_NGX_DLSS_Feature_Flags_AutoExposure;
    if (frame->hdr) flags |= NVSDK_NGX_DLSS_Feature_Flags_IsHDR;
    if (frame->invertedDepth) flags |= NVSDK_NGX_DLSS_Feature_Flags_DepthInverted;
    NVSDK_NGX_Result result = NVSDK_NGX_Result_Success;
    bool recreated = false;
    if (!feature || status.inputWidth != color.Width || status.inputHeight != color.Height ||
        status.outputWidth != output.Width || status.outputHeight != output.Height || flags != featureFlags) {
        result = DropFeature();
        NVSDK_NGX_DLSS_Create_Params create{};
        create.Feature.InWidth = color.Width; create.Feature.InHeight = color.Height;
        create.Feature.InTargetWidth = output.Width; create.Feature.InTargetHeight = output.Height;
        create.Feature.InPerfQualityValue = NVSDK_NGX_PerfQuality_Value_MaxQuality;
        create.InFeatureCreateFlags = flags;
        if (NVSDK_NGX_SUCCEED(result)) result = NGX_D3D11_CREATE_DLSS_EXT(context, &feature, parameters, &create);
        if (NVSDK_NGX_SUCCEED(result) && feature) {
            recreated = true; featureFlags = flags;
            status.inputWidth = color.Width; status.inputHeight = color.Height;
            status.outputWidth = output.Width; status.outputHeight = output.Height;
        }
    }
    if (NVSDK_NGX_SUCCEED(result) && feature) {
        NVSDK_NGX_D3D11_DLSS_Eval_Params evaluate{};
        evaluate.Feature.pInColor = frame->color; evaluate.Feature.pInOutput = frame->output;
        evaluate.pInDepth = frame->depth; evaluate.pInMotionVectors = frame->motion;
        evaluate.InJitterOffsetX = frame->jitterX; evaluate.InJitterOffsetY = frame->jitterY;
        evaluate.InMVScaleX = frame->motionScaleX; evaluate.InMVScaleY = frame->motionScaleY;
        evaluate.InRenderSubrectDimensions.Width = color.Width; evaluate.InRenderSubrectDimensions.Height = color.Height;
        evaluate.InReset = frame->reset || recreated || status.successfulEvaluations == 0;
        evaluate.InPreExposure = frame->preExposure; evaluate.InExposureScale = frame->exposureScale;
        evaluate.InFrameTimeDeltaInMsec = frame->frameMilliseconds;
        result = NGX_D3D11_EVALUATE_DLSS_EXT(context, feature, parameters, &evaluate);
        if (NVSDK_NGX_SUCCEED(result)) ++status.successfulEvaluations;
    }
    context->SwapDeviceContextState(saved, nullptr);
    if (saved) saved->Release(); context->Release();
    status.lastEvaluateResult = static_cast<unsigned>(result);
    // Compositing is owned by the game-specific render adapter. Vendor success
    // alone cannot set finalCompositionVerified.
    return status.lastEvaluateResult;
}
bool TwinKartDlssGetState(TwinKartDlssState* target) {
    ConsumerCall call; if (!call.entered) return false;
    if (!target || target->size != sizeof(*target) || GetCurrentThreadId() != ownerThread) return false;
    *target = status; return true;
}
namespace {
bool ShutdownUnchecked() {
    if (!device) return true;
    if (NVSDK_NGX_FAILED(DropFeature())) return false;
    if (parameters) { if (NVSDK_NGX_FAILED(NVSDK_NGX_D3D11_DestroyParameters(parameters))) return false; parameters = nullptr; }
    if (initialized) { if (NVSDK_NGX_FAILED(NVSDK_NGX_D3D11_Shutdown1(device))) return false; initialized = false; }
    if (isolatedState) { isolatedState->Release(); isolatedState = nullptr; }
    if (device1) { device1->Release(); device1 = nullptr; }
    device->Release(); device = nullptr;
    status = TwinKartDlssState{sizeof(TwinKartDlssState)}; ownerThread = 0; featureFlags = 0;
    return true;
}
}
void TwinKartDlssShutdown() {
    ConsumerCall call;
    if (!call.entered || !device || GetCurrentThreadId() != ownerThread) return;
    ShutdownUnchecked();
}
bool TwinKartDlssShutdownDrained(unsigned expectedOwnerThread, ID3D11DeviceContext* context) {
    ConsumerCall call;
    if (!call.entered || !device || !expectedOwnerThread || expectedOwnerThread != ownerThread || !context ||
        context->GetType() != D3D11_DEVICE_CONTEXT_IMMEDIATE || !SameDevice(context) || FAILED(device->GetDeviceRemovedReason())) return false;
    ID3D11DeviceContext* immediate = nullptr; device->GetImmediateContext(&immediate);
    bool exact = immediate == context; if (immediate) immediate->Release(); if (!exact) return false;
    D3D11_QUERY_DESC desc{D3D11_QUERY_EVENT, 0}; ID3D11Query* query = nullptr;
    if (FAILED(device->CreateQuery(&desc, &query))) return false;
    context->End(query); context->Flush();
    ULONGLONG started = GetTickCount64(); BOOL complete = FALSE; HRESULT result;
    do {
        result = context->GetData(query, &complete, sizeof(complete), D3D11_ASYNC_GETDATA_DONOTFLUSH);
        if (result != S_FALSE) break;
        Sleep(1);
    } while (GetTickCount64() - started < 10000);
    query->Release();
    if (result != S_OK || !complete || FAILED(device->GetDeviceRemovedReason())) return false;
    return ShutdownUnchecked();
}
