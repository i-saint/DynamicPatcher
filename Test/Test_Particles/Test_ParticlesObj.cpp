// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include "Test_Particles.h"
#include <algorithm>

#pragma warning(disable:4018)

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

float GetParticleRadius()
{
    return g_pradius;
}

dpPatch void SetParticleRadius(float r)
{
    g_pradius = r;
    Particle *particles = g_particles;
    size_t num_particles = _countof(g_particles);
    for(size_t i=0; i<num_particles; ++i) {
        particles[i].radius = r;
    }
}

dpPatch void InitializeParticles()
{
    g_num_particles = MAX_PARTICLES/2;
    g_pradius = 0.015f;
    g_deccel = 0.998f;
    g_accel = 0.0022f;
    g_gravity = 0.002f;
    g_gravity_center = XMFLOAT3(0.0f, 1.0f, 0.0f);

    Particle *particles = g_particles;
    size_t num_particles = _countof(g_particles);
    for(size_t i=0; i<num_particles; ++i) {
        particles[i].position = XMFLOAT3(GenRand()*3.0f, GenRand()*3.0f, GenRand()*3.0f);
        particles[i].velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
        particles[i].radius = g_pradius;
    }
}

dpPatch void FinalizeParticles()
{

}

dpPatch void UpdateParticles()
{
    // 相互に押し返し
    Particle *particles = g_particles;
    size_t num_particles = g_num_particles;
    #pragma omp parallel for
    for(int ri=0; ri<num_particles; ++ri) {
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
        #pragma omp parallel for
        for(int ri=0; ri<num_particles; ++ri) {
            Particle &rp = particles[ri];
            XMFLOAT3 dir = Normalize(g_gravity_center - rp.position);
            rp.velocity += dir * g_accel;
        }
    }

    // 床とのバウンド

    #pragma omp parallel for
    for(int ri=0; ri<num_particles; ++ri) {
        Particle &rp = particles[ri];
        rp.velocity.y -= g_gravity;
        float bottom = rp.position.y - rp.radius;
        float d = bottom + 3.0f;
        rp.velocity.y += std::min<float>(0.0f, d) * -0.2f;
    }

    // 速度を適用
    #pragma omp parallel for
    for(int ri=0; ri<num_particles; ++ri) {
        Particle &rp = particles[ri];
        rp.position += rp.velocity;
        rp.velocity *= g_deccel;
    }
}

dpOnLoad(
    Particle *particles = g_particles;
    size_t num_particles = _countof(g_particles);
    for(size_t i=0; i<num_particles; ++i) {
        particles[i].position = XMFLOAT3(GenRand()*3.0f, GenRand()*3.0f, GenRand()*3.0f);
        particles[i].velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
        particles[i].radius = 0.2f;
    }
    ::OutputDebugStringA("dpOnLoad()\n");
)

