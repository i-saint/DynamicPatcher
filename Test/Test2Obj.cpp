#include "stdafx.h"
#include "Test2.h"

inline float GenRand()
{
    return float((rand()-(RAND_MAX/2))*2) / (float)RAND_MAX;
}

inline XMFLOAT3& operator+=(XMFLOAT3 &l, const XMFLOAT3 &r) { l.x+=r.x; l.y+=r.y; l.z+=r.z; return l; }

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
        for(size_t si=0; si<num_particles; ++si) {
            Particle &sp = particles[si];
        }
    }

    for(size_t ri=0; ri<num_particles; ++ri) {
        Particle &rp = particles[ri];
        rp.position += rp.velocity;
    }
}
