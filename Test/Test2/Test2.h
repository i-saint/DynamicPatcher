// created by i-saint
// distributed under Creative Commons Attribution (CC BY) license.
// https://github.com/i-saint/DynamicObjLoader

#include <windows.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dcompiler.h>
#include <xnamath.h>
#include <cstdio>
#include "DynamicObjLoader.h"

#define MAX_PARTICLES 2048

struct Particle
{
    XMFLOAT3 position;
    XMFLOAT3 velocity;
    float radius;
};
extern Particle g_particles[MAX_PARTICLES];

DOL_Fixate float GenRand();
