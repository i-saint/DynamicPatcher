// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "stdafx.h"
#include "Test2.h"
#include <algorithm>

DOL_Module

inline XMFLOAT3& operator+=(XMFLOAT3 &l, const XMFLOAT3 &r) { l.x+=r.x; l.y+=r.y; l.z+=r.z; return l; }
inline XMFLOAT3& operator*=(XMFLOAT3 &l, float r) { l.x*=r; l.y*=r; l.z*=r; return l; }
inline XMFLOAT3 operator-(const XMFLOAT3 &l, const XMFLOAT3 &r) { return XMFLOAT3(l.x-r.x, l.y-r.y, l.z-r.z); }
inline XMFLOAT3 operator*(const XMFLOAT3 &l, float r) { return XMFLOAT3(l.x*r, l.y*r, l.z*r); }
inline XMFLOAT3 operator/(const XMFLOAT3 &l, float r) { return XMFLOAT3(l.x/r, l.y/r, l.z/r); }

inline float Dot(const XMFLOAT3 a, const XMFLOAT3 b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

inline float Len(const XMFLOAT3 a)
{
    return sqrt(Dot(a, a));
}

inline XMFLOAT3 Normalize(XMFLOAT3 a)
{
    return a / Len(a);
}

DOL_Export void InitializeParticles(Particle *particles, size_t num_particles)
{
    for(size_t i=0; i<num_particles; ++i) {
        particles[i].position = XMFLOAT3(GenRand()*3.0f, GenRand()*3.0f, GenRand()*3.0f);
        particles[i].velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
        particles[i].radius = 0.015f;
    }
}

DOL_Export void UpdateParticles(Particle *particles, size_t num_particles)
{
    // 相互に押し返し
    for(size_t ri=0; ri<num_particles; ++ri) {
        Particle &rp = particles[ri];
        XMFLOAT3 rpos = rp.position;
        float rradius = rp.radius;
        for(size_t si=0; si<num_particles; ++si) {
            Particle &sp = particles[si];
            float uradius = rradius + sp.radius;
            XMFLOAT3 diff = sp.position - rpos;
            float len = Len(diff);
            if(len==0.0f) { continue; } // 自分自身との衝突なので無視
            float d = len - uradius;
            if(d < 0.0f) {
                XMFLOAT3 dir = diff / len;
                rp.velocity += dir * (d * 0.2f);
            }
        }
    }

    // 中心に吸い寄せる
    {
        const XMFLOAT3 gravity_center = XMFLOAT3(0.0f, 1.0f, 0.0f);
        for(size_t ri=0; ri<num_particles; ++ri) {
            Particle &rp = particles[ri];
            XMFLOAT3 dir = Normalize(gravity_center - rp.position);
            rp.velocity += dir * 0.0022f;
        }
    }

    // 床とのバウンド
    for(size_t ri=0; ri<num_particles; ++ri) {
        Particle &rp = particles[ri];
        rp.velocity.y -= 0.002f;
        float bottom = rp.position.y - rp.radius;
        float d = bottom + 3.0f;
        rp.velocity.y += std::min<float>(0.0f, d) * -0.2f;
    }

    // 速度を適用
    for(size_t ri=0; ri<num_particles; ++ri) {
        Particle &rp = particles[ri];
        rp.position += rp.velocity;
        rp.velocity *= 0.998f;
    }
}

DOL_OnLoad({
    Particle *particles = g_particles;
    size_t num_particles = MAX_PARTICLES;
    InitializeParticles(particles, num_particles);
})
