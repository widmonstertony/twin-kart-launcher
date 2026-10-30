#pragma once
namespace TwinKartGraphics {
// Original conversion for the exact native post-resolve, pre-HUD color path.
// Sample jittered native guides at the corresponding unjittered color position.
inline constexpr char NativeGuideShader[]=R"(
Texture2D<float> NativeDepth:register(t0);
Texture2D<float4> NativeVelocity:register(t1);
Texture2D<float4> NativeColor:register(t2);
RWTexture2D<float> OutDepth:register(u0);
RWTexture2D<float2> OutMotion:register(u1);
RWTexture2D<float4> OutColor:register(u2);
cbuffer NativeView:register(b1) { float4 v[512]; };
cbuffer NativeRect:register(b3) { uint2 inputMin; uint2 inputSize; uint2 colorMin; uint2 colorSize; };
[numthreads(8,8,1)] void main(uint3 id:SV_DispatchThreadID) {
 uint w,h;OutDepth.GetDimensions(w,h);if(id.x>=w||id.y>=h)return;
 uint gw,gh;NativeDepth.GetDimensions(gw,gh);
 float2 uv=(float2(id.xy)+0.5)/float2(w,h);
 float2 jitterUv=v[130].xy*float2(0.5,-0.5);
 // The native viewport may occupy a subrectangle of a pooled allocation.
 // Clamp in viewport coordinates before adding its allocation-space origin.
 int2 maximum=int2(inputSize)-1;
#ifdef TK_TEMPORAL_INPUT
 int2 samplePoint=int2(inputMin+id.xy);
#else
 int2 samplePoint=int2(inputMin)+clamp(int2(floor((uv+jitterUv)*inputSize)),int2(0,0),maximum);
#endif
 uint vw,vh;NativeVelocity.GetDimensions(vw,vh);
 int2 velocityPoint=(vw==1 && vh==1)?int2(0,0):samplePoint;
 float z=NativeDepth.Load(int3(samplePoint,0));float4 packed=NativeVelocity.Load(int3(velocityPoint,0));
 float2 ndc=uv*float2(2,-2)+float2(-1,1);
#ifdef TK_TEMPORAL_INPUT
 ndc-=v[130].xy;
#endif
 float4 previous=mul(float4(ndc,z,1),float4x4(v[126],v[127],v[128],v[129]));
 float2 delta=abs(previous.w)>1e-8?previous.xy/previous.w-ndc:float2(0,0);
 if(packed.x>0)delta=-(packed.xy-(32767.0/65535.0))/(0.499*0.5);
 // Dimensions are uints: negating h before converting wraps modulo 2^32.
 // Convert both axes first so vertical pixel motion keeps its native sign.
 OutDepth[id.xy]=z;OutMotion[id.xy]=delta*(float2(w,h)*float2(0.5,-0.5));
 uint cw,ch;NativeColor.GetDimensions(cw,ch);
#ifdef TK_TEMPORAL_INPUT
 OutColor[id.xy]=NativeColor.Load(int3(samplePoint,0));
#else
 if(all(colorSize==uint2(w,h)) && cw>=colorMin.x+w && ch>=colorMin.y+h)
  OutColor[id.xy]=NativeColor.Load(int3(colorMin+id.xy,0));
#endif
})";
}
