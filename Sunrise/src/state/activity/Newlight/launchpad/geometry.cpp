#include "controller.h"
#include <cmath>
namespace sunrise::state::activity::newlight::launchpad {
bool near_first_vandal(Point from,Point to) noexcept {
    // Source8153C0B0's authored origin. This gates the reveal, never places the actor.
    constexpr Point origin{452.798736F,-806.943420F,17.701751F};
    const double dx=double(to.x)-from.x,dy=double(to.y)-from.y,dz=double(to.z)-from.z;
    const double length=dx*dx+dy*dy+dz*dz;
    const double t=length?std::clamp(((origin.x-from.x)*dx+(origin.y-from.y)*dy+(origin.z-from.z)*dz)/length,0.,1.):0.;
    const double x=from.x+t*dx-origin.x,y=from.y+t*dy-origin.y,z=from.z+t*dz-origin.z;
    return x*x+y*y+z*z<=20.*20.;
}
bool contains(const Volume& v,Point p) noexcept {
    if(!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z) || v.vertices.size()<3
        || p.x<v.min.x || p.x>v.max.x || p.y<v.min.y || p.y>v.max.y || p.z<v.min.z || p.z>v.max.z) {return false;}
    bool inside{};
    for(std::size_t i=0,j=v.vertices.size()-1;i<v.vertices.size();j=i++) {
        const auto a=v.vertices[j],b=v.vertices[i];const double dx=double(b.x)-a.x,dy=double(b.y)-a.y,px=double(p.x)-a.x,py=double(p.y)-a.y;
        if(dx==0 && dy==0) {continue;}
        if(std::abs(dx*py-dy*px)<0.00001 && px*dx+py*dy>=0 && px*dx+py*dy<=dx*dx+dy*dy) {return true;}
        if((a.y>p.y)!=(b.y>p.y) && p.x<dx*py/dy+a.x) {inside=!inside;}
    }
    return inside;
}
bool crosses(const Volume& v,Point from,Point to) noexcept {
    if(contains(v,from) || contains(v,to)) {return true;}
    if(v.vertices.size()<3) {return false;}
    double low=0,high=1;
    for(auto axis:{std::array<double,4>{from.x,to.x,v.min.x,v.max.x},
        std::array<double,4>{from.y,to.y,v.min.y,v.max.y},std::array<double,4>{from.z,to.z,v.min.z,v.max.z}}) {
        const auto delta=axis[1]-axis[0];
        if(std::abs(delta)<1e-8) {if(axis[0]<axis[2] || axis[0]>axis[3]) {return false;}continue;}
        auto a=(axis[2]-axis[0])/delta,b=(axis[3]-axis[0])/delta;if(a>b) {std::swap(a,b);}
        low=(std::max)(low,a);high=(std::min)(high,b);if(low>high) {return false;}
    }
    const auto at=[&](double t) {return Point{float(from.x+(to.x-from.x)*t),float(from.y+(to.y-from.y)*t),float(from.z+(to.z-from.z)*t)};};
    const auto a=at(low),b=at(high);if(contains(v,a) || contains(v,b)) {return true;}
    const double dx=double(b.x)-a.x,dy=double(b.y)-a.y;
    for(std::size_t i=0,j=v.vertices.size()-1;i<v.vertices.size();j=i++) {
        const auto p=v.vertices[j],q=v.vertices[i];const double ex=double(q.x)-p.x,ey=double(q.y)-p.y;
        const auto determinant=dx*ey-dy*ex;if(std::abs(determinant)<1e-8) {continue;}
        const double px=double(p.x)-a.x,py=double(p.y)-a.y;
        const auto t=(px*ey-py*ex)/determinant,u=(px*dy-py*dx)/determinant;
        if(t>=0 && t<=1 && u>=0 && u<=1) {return true;}
    }
    return false;
}
}
