#pragma once
#include "FrameGenerationD3D12.h"
#include <cmath>
#include <cfloat>
#include <cstring>
#include <limits>
namespace TwinKartGraphics {
inline bool InvertNativeMatrix(const float* source,float* target) {
    double a[4][8]{};
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c) {
        if(!std::isfinite(source[r*4+c]))return false;
        a[r][c]=source[r*4+c];a[r][c+4]=r==c?1:0;
    }
    for(unsigned c=0;c<4;++c) {
        unsigned pivot=c;
        for(unsigned r=c+1;r<4;++r)if(fabs(a[r][c])>fabs(a[pivot][c]))pivot=r;
        if(fabs(a[pivot][c])<1e-15)return false;
        for(unsigned k=0;k<8;++k){double v=a[c][k];a[c][k]=a[pivot][k];a[pivot][k]=v;}
        double scale=a[c][c];for(unsigned k=0;k<8;++k)a[c][k]/=scale;
        for(unsigned r=0;r<4;++r)if(r!=c) {
            double factor=a[r][c];for(unsigned k=0;k<8;++k)a[r][k]-=factor*a[c][k];
        }
    }
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c) {
        double value=a[r][c+4];if(!std::isfinite(value) || fabs(value)>FLT_MAX)return false;
        target[r*4+c]=static_cast<float>(value);
    }
    return true;
}
inline void MultiplyNativeMatrix(const float* a,const float* b,float* out) {
    float product[16]{};
    for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
        for(unsigned k=0;k<4;++k)product[r*4+c]+=a[r*4+k]*b[k*4+c];
    memcpy(out,product,sizeof(product));
}
// Exact-build offsets are checked by the caller before this function is used.
// Matrices are row-vector native transforms, not independently invented views.
inline bool DecodeNativeCamera(const float* view,unsigned width,unsigned height,bool reset,CameraFrame& out) {
    if(!view || !width || !height || view[536]!=width || view[537]!=height)return false;
    CameraFrame camera{};
    memcpy(camera.viewToClip,view+144,64);
    memcpy(camera.clipToPrevious,view+504,64);
    if(!InvertNativeMatrix(camera.viewToClip,camera.clipToView) ||
       !InvertNativeMatrix(camera.clipToPrevious,camera.previousToClip))return false;
    float worldToClip[16],clipToWorld[16],viewToWorld[16];
    memcpy(worldToClip,view+32,64);
    for(unsigned r=0;r<4;++r) {
        worldToClip[r*4]-=worldToClip[r*4+3]*view[520];
        worldToClip[r*4+1]-=worldToClip[r*4+3]*view[521];
    }
    if(!InvertNativeMatrix(worldToClip,clipToWorld))return false;
    MultiplyNativeMatrix(camera.viewToClip,clipToWorld,viewToWorld);
    if(fabsf(viewToWorld[15])<1e-8f)return false;
    for(unsigned k=0;k<3;++k)camera.position[k]=viewToWorld[12+k]/viewToWorld[15];
    float* axes[]{camera.right,camera.up,camera.forward};
    for(unsigned r=0;r<3;++r) {
        float length=0;
        for(unsigned k=0;k<3;++k)length+=viewToWorld[r*4+k]*viewToWorld[r*4+k];
        length=sqrtf(length);if(!std::isfinite(length) || length<1e-8f)return false;
        for(unsigned k=0;k<3;++k)axes[r][k]=viewToWorld[r*4+k]/length;
    }
    if(camera.viewToClip[0]<=0 || camera.viewToClip[5]<=0 || camera.viewToClip[11]!=1 || camera.viewToClip[15]!=0)return false;
    // This supported native projection is inverted-Z with an infinite far
    // plane. Positive infinity preserves its bound. FLT_MAX is Streamline's
    // INVALID_FLOAT sentinel, so it must not be submitted as cameraFar.
    if(camera.viewToClip[10]!=0 || camera.viewToClip[14]<=0)return false;
    camera.nearPlane=camera.viewToClip[14];camera.farPlane=std::numeric_limits<float>::infinity();
    camera.fieldOfView=2*atanf(1/camera.viewToClip[5]);
    camera.aspect=camera.viewToClip[5]/camera.viewToClip[0];
    camera.jitterX=-view[520]*width*0.5f;camera.jitterY=view[521]*height*0.5f;
    camera.motionScaleX=1.0f/width;camera.motionScaleY=1.0f/height;
    camera.invertedDepth=true;camera.reset=reset;
    for(float value:camera.position)if(!std::isfinite(value))return false;
    out=camera;return true;
}
}
