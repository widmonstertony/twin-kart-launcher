#pragma once
namespace TwinKartGraphics {
// Exact same-frame final/pre-UI comparison used by native FG presentation.
// The conservative binary mask keeps changed UI pixels out of interpolation;
// it cannot reconstruct transparent UI layers or prove visual quality.
inline constexpr char NativeHudAlphaShader[]=R"(
Texture2D<float4> FinalColor:register(t0);Texture2D<float4> SceneColor:register(t1);RWTexture2D<float> Alpha:register(u0);
[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){uint w,h;Alpha.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;
float3 difference=abs(FinalColor.Load(int3(id.xy,0)).rgb-SceneColor.Load(int3(id.xy,0)).rgb);
Alpha[id.xy]=max(difference.r,max(difference.g,difference.b))>0.0005?1:0;})";
}
