#include <windows.h>
#include <d3d11.h>
#include <d3dx11.h>
#include <d3dcompiler.h>
#include <xnamath.h>
#include <cstdio>
#include "DynamicObjLoader.h"

struct Particle
{
    XMFLOAT3 position;
    XMFLOAT3 velocity;
    float radius;
};

