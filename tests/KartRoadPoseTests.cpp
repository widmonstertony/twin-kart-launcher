#include <cstdio>
#include <stdexcept>
#include "../native/KartRoadPose.h"
namespace R=TwinKartRace;namespace P=R::KartRoadPoseDetail;
void Need(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool Near(float a,float b){return std::fabs(a-b)<0.002f;}
bool Near(const R::ArenaPoint& a,const R::ArenaPoint& b){return Near(a.x,b.x) && Near(a.y,b.y) && Near(a.z,b.z);}
float Orientation(const R::ComponentRotation& a,const R::ComponentRotation& b){return std::fabs(a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w);}
float Distance3(const R::ArenaPoint& a,const R::ArenaPoint& b){return std::hypot(std::hypot(a.x-b.x,a.y-b.y),a.z-b.z);}
R::KartContactSnapshot Contacts(float slopeX=0,float slopeY=0) {
    static int supports[4]{};R::KartContactSnapshot contacts;
    const float x[]{-100,-100,100,100},y[]{-60,60,-60,60};
    for(unsigned i=0;i<4;++i)contacts.wheels[i]={{x[i],y[i],x[i]*slopeX+y[i]*slopeY},&supports[i]};return contacts;
}
int main(){try {
    auto contacts=Contacts();R::ArenaPoint root{10,20,87},position{7,8,9};R::ComponentRotation rotation{},mapped{};
    R::KartRoadCalibration calibration;
    Need(R::CalibrateKartRoad(contacts,root,rotation,calibration) && Near(calibration.roadOrigin,{10,20,0}),
         "Actual wheel-contact plane did not calibrate the original body origin");
    Need(R::MapKartRoadPose(calibration,calibration.roadOrigin,calibration.roadNormal,calibration.forward,position,mapped) &&
         Near(position,root) && Orientation(mapped,rotation)>0.99999f,"Source road/body pose did not round-trip");
    Need(R::MapKartRoadPose(calibration,{300,400,50},{0,0,1},{1,0,0},position,mapped) && Near(position,{300,400,137}),
         "Mapped body used a guessed height instead of its observed native contact offset");
    // A native model pivot may lie below its road. It must not be silently
    // clamped to a visually convenient positive kart height.
    R::KartRoadCalibration below;Need(R::CalibrateKartRoad(contacts,{0,0,-40},rotation,below) &&
         R::MapKartRoadPose(below,{100,200,10},{0,0,1},{0,1,0},position,mapped) && Near(position,{100,200,-30}),
         "Native negative pivot/contact offset was replaced by a fixed floor lift");
    auto sloped=Contacts(0.4f,0.1f);R::ArenaPoint nativeNormal{};P::Unit({-0.4f,-0.1f,1},nativeNormal);
    R::ComponentRotation nativeFrame;Need(P::FrameRotation(nativeNormal,{1,0,0},nativeFrame),"Native slope fixture frame");
    // Retain actual chassis roll relative to the contact road, rather than
    // replacing it with the road normal. Fifteen degrees around local X.
    R::ComponentRotation roll{std::sin(0.130899694f),0,0,std::cos(0.130899694f)};
    auto nativeRotation=P::Multiply(nativeFrame,roll);R::ArenaPoint slopedRoot{0,0,83};
    Need(R::KartSameAttitude(nativeRotation,nativeRotation) &&
         R::KartSameAttitude(nativeRotation,{-nativeRotation.x,-nativeRotation.y,-nativeRotation.z,-nativeRotation.w}) &&
         !R::KartSameAttitude(rotation,roll) && !R::KartSameAttitude(rotation,{0,0,0,0}),
         "Fixed-attitude translation gate accepted an angular change/invalid quaternion or rejected quaternion sign equivalence");
    Need(R::CalibrateKartRoad(sloped,slopedRoot,nativeRotation,calibration) &&
         R::MapKartRoadPose(calibration,calibration.roadOrigin,nativeNormal,calibration.forward,position,mapped) &&
         Near(position,slopedRoot) && Orientation(mapped,nativeRotation)>0.99999f,
         "Banked-road calibration discarded original chassis roll or changed the source body pose");
    R::ArenaPoint otherNormal{0,-0.3f,std::sqrt(0.91f)},otherPoint{1000,200,70};
    Need(R::MapKartRoadPose(calibration,otherPoint,otherNormal,{0,1,0},position,mapped) &&
         Near(Distance3(position,otherPoint),Distance3(slopedRoot,calibration.roadOrigin)) &&
         Near(P::Dot(P::Rotate(mapped,{0,0,1}),otherNormal),P::Dot(P::Rotate(nativeRotation,{0,0,1}),nativeNormal)),
         "Road pose mapping changed the observed body offset length or chassis tilt relative to its surface");
    for(float yaw:{0.f,1.570796327f,3.141592654f,4.71238898f}) {
        Need(R::MapKartRoadPose(below,{0,0,0},{0,0,1},{std::cos(yaw),std::sin(yaw),0},position,mapped),"Surface frame quaternion lost a matrix branch");
        auto forward=P::Rotate(mapped,{1,0,0});Need(Near(forward,{std::cos(yaw),std::sin(yaw),0}),"Surface frame turned a native kart in the wrong direction");
    }
    auto reject=[](const R::KartContactSnapshot& source,R::ComponentRotation q={}) {
        R::KartRoadCalibration output;output.roadOrigin={7,8,9};
        Need(!R::CalibrateKartRoad(source,{0,0,80},q,output) && output.roadOrigin.z==9 && !output.valid,
             "Invalid original contact set published a partial calibration");
    };
    auto missing=contacts;missing.wheels[2].component=nullptr;reject(missing);
    auto collinear=contacts;for(unsigned i=0;i<4;++i)collinear.wheels[i].point={static_cast<float>(i),0,0};reject(collinear);
    auto warped=contacts;warped.wheels[3].point.z=40;reject(warped);
    reject(Contacts(2,0));reject(contacts,{1,0,0,0});reject(contacts,{0,0,0,0});
    auto invalid=contacts;invalid.wheels[1].point.z=(std::numeric_limits<float>::quiet_NaN)();reject(invalid);
    position={7,8,9};mapped={};
    Need(!R::MapKartRoadPose({},otherPoint,otherNormal,{1,0,0},position,mapped) && Near(position,{7,8,9}) &&
         !R::MapKartRoadPose(below,otherPoint,{0,0,0},{1,0,0},position,mapped) &&
         !R::MapKartRoadPose(below,otherPoint,{0,0,1},{0,0,1},position,mapped),
         "Invalid/degenerated road frame changed caller output");
    puts("PASS original-contact road/body pose geometry: source round-trip, observed positive/negative native pivot, slope/roll preservation, changed road heading/normal, all quaternion branches and malformed/mixed-support rejection. Mathematical fixtures only; no native body sweep or physical route accepted.");return 0;
}catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());return 1;}}
