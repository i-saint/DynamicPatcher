#include "stdafx.h"
#include "Test2.h"

inline XMFLOAT3& operator+=(XMFLOAT3 &l, const XMFLOAT3 &r) { l.x+=r.x; l.y+=r.y; l.z+=r.z; return l; }
inline XMFLOAT3 operator-(XMFLOAT3 &l, const XMFLOAT3 &r) { return XMFLOAT3(l.x-r.x, l.y-r.y, l.z-r.z); }
inline XMFLOAT3 operator*(XMFLOAT3 &l, float r) { return XMFLOAT3(l.x*r, l.y*r, l.z*r); }
inline XMFLOAT3 operator/(XMFLOAT3 &l, float r) { return XMFLOAT3(l.x/r, l.y/r, l.z/r); }

inline float Dot(const XMFLOAT3 a, const XMFLOAT3 b)
{
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

inline float Len(const XMFLOAT3 a)
{
    return sqrt(Dot(a, a));
}

DOL_Export void InitializeParticles(Particle *particles, size_t num_particles)
{
    for(size_t i=0; i<num_particles; ++i) {
        particles[i].position = XMFLOAT3(GenRand()*4.0f, GenRand()*4.0f, GenRand()*4.0f);
        particles[i].radius = 0.05f;
        particles[i].velocity = XMFLOAT3(0.0f, 0.0f, 0.0f);
    }
}

DOL_Export void UpdateParticles(Particle *particles, size_t num_particles)
{
    for(size_t ri=0; ri<num_particles; ++ri) {
        Particle &rp = particles[ri];
        XMFLOAT3 rpos = rp.position;
        float rradius = rp.radius;
        for(size_t si=0; si<num_particles; ++si) {
            Particle &sp = particles[si];
            float uradius = rradius - sp.radius;
            XMFLOAT3 diff = rpos - sp.position;
            float len = Len(diff);
            float d = len - uradius;
            if(d < 0.0f) {
                XMFLOAT3 dir = diff / len;
                rp.velocity += dir * d;
            }
        }
    }

    for(size_t ri=0; ri<num_particles; ++ri) {
        Particle &rp = particles[ri];
        rp.velocity.z += 0.001f;
        rp.position += rp.velocity;
        if(rp.position.z > 2.5f) {
            rp.velocity.z *= -1.0f;
        }
    }
}
