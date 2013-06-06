// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicPatcher

#include <windows.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dcompiler.h>
#include <xnamath.h>
#include <cstdio>
#include "DynamicPatcher.h"

#define MAX_PARTICLES 6144

struct Particle
{
    XMFLOAT3 position;
    XMFLOAT3 velocity;
    float radius;
};
extern int g_num_particles;
extern float g_pradius;
extern float g_accel;
extern float g_deccel;
extern float g_gravity;
extern XMFLOAT3 g_gravity_center;
extern Particle g_particles[MAX_PARTICLES];
extern int g_num_particles;

dpPatch void InitializeParticles();
dpPatch void FinalizeParticles();
dpPatch void UpdateParticles();
float GenRand();

