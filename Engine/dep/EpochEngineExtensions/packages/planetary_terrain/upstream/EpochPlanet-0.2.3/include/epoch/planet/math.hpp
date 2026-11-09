#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <concepts>
#include <numbers>

namespace epoch::planet {
template<std::floating_point T> struct Vec3 {
    T x{}, y{}, z{};
    constexpr Vec3() = default;
    constexpr Vec3(T a, T b, T c) : x(a), y(b), z(c) {}
    template<std::floating_point U> explicit constexpr Vec3(Vec3<U> p)
        : x(static_cast<T>(p.x)), y(static_cast<T>(p.y)), z(static_cast<T>(p.z)) {}
    constexpr Vec3 operator+(Vec3 b) const { return {x+b.x,y+b.y,z+b.z}; }
    constexpr Vec3 operator-(Vec3 b) const { return {x-b.x,y-b.y,z-b.z}; }
    constexpr Vec3 operator-() const { return {-x,-y,-z}; }
    constexpr Vec3 operator*(T s) const { return {x*s,y*s,z*s}; }
    constexpr Vec3 operator/(T s) const { return {x/s,y/s,z/s}; }
    constexpr Vec3& operator+=(Vec3 b) { *this=*this+b; return *this; }
    constexpr Vec3& operator-=(Vec3 b) { *this=*this-b; return *this; }
    friend constexpr bool operator==(Vec3,Vec3) = default;
};
using DVec3=Vec3<double>;
using FVec3=Vec3<float>;
template<std::floating_point T> constexpr T dot(Vec3<T> a,Vec3<T> b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
template<std::floating_point T> constexpr Vec3<T> cross(Vec3<T> a,Vec3<T> b) {
    return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};
}
template<std::floating_point T> T length(Vec3<T> v) { return std::sqrt(dot(v,v)); }
template<std::floating_point T> Vec3<T> normalized(Vec3<T> v,Vec3<T> fallback={T(0),T(1),T(0)}) {
    const T l=length(v); return l>T(1e-15) && std::isfinite(l) ? v/l : fallback;
}
template<std::floating_point T> bool finite(Vec3<T> v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
template<std::floating_point T> Vec3<T> mix(Vec3<T> a,Vec3<T> b,T t) { return a+(b-a)*t; }
inline double smooth(double a,double b,double x) { const auto t=std::clamp((x-a)/(b-a),0.0,1.0); return t*t*(3.0-2.0*t); }
inline DVec3 rotate(DVec3 v,DVec3 axis,double angle) {
    axis=normalized(axis); const auto c=std::cos(angle),s=std::sin(angle);
    return v*c+cross(axis,v)*s+axis*(dot(axis,v)*(1.0-c));
}
inline DVec3 tangent(DVec3 up) {
    return normalized(cross(std::abs(up.y)<0.9?DVec3{0,1,0}:DVec3{1,0,0},up));
}
inline constexpr double pi=std::numbers::pi_v<double>;
using Mat4=std::array<float,16>;
inline Mat4 multiply(const Mat4& a,const Mat4& b) {
    Mat4 r{};
    for(int c=0;c<4;++c) for(int row=0;row<4;++row) for(int k=0;k<4;++k)
        r[static_cast<std::size_t>(c*4+row)]+=a[static_cast<std::size_t>(k*4+row)]*b[static_cast<std::size_t>(c*4+k)];
    return r;
}
// Camera-relative, right-handed, infinite-far reversed-Z projection. Vulkan Y is inverted.
inline Mat4 view_projection(DVec3 right,DVec3 up,DVec3 forward,double aspect,double fov,double near_plane=0.1) {
    const auto f=static_cast<float>(1.0/std::tan(fov*0.5));
    Mat4 p{}; p[0]=f/static_cast<float>(aspect);p[5]=-f;p[11]=-1;p[14]=static_cast<float>(near_plane);
    Mat4 v{static_cast<float>(right.x),static_cast<float>(up.x),static_cast<float>(-forward.x),0,
           static_cast<float>(right.y),static_cast<float>(up.y),static_cast<float>(-forward.y),0,
           static_cast<float>(right.z),static_cast<float>(up.z),static_cast<float>(-forward.z),0,0,0,0,1};
    return multiply(p,v);
}
// Conservative sphere/frustum test. Radius must enclose fine AND morphed positions.
inline bool sphere_visible(DVec3 relative,double radius,DVec3 forward,DVec3 right,DVec3 up,double aspect,double tan_half) {
    const auto z=dot(relative,forward),x=dot(relative,right),y=dot(relative,up);
    const auto horizontal=tan_half*aspect;
    return z+radius>=0.1&&std::abs(x)-z*horizontal<=radius*std::sqrt(1.0+horizontal*horizontal)&&
        std::abs(y)-z*tan_half<=radius*std::sqrt(1.0+tan_half*tan_half);
}
// Ray direction must have unit length. Stable roots also work from inside a sphere.
inline bool ray_sphere(DVec3 origin,DVec3 direction,double radius,double& near_t,double& far_t) {
    const double b=dot(origin,direction),c=dot(origin,origin)-radius*radius,disc=b*b-c;
    if(disc<0.0) return false;
    const double root=std::sqrt(disc),q=-b-std::copysign(root,b);
    if(std::abs(q)<1e-20) { near_t=far_t=-b; return far_t>=0; }
    near_t=q;far_t=c/q;if(near_t>far_t) std::swap(near_t,far_t);
    return far_t>=0;
}
} // namespace epoch::planet
