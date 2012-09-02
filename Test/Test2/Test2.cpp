#pragma warning( disable : 4996 ) // _s ‚¶‚á‚È‚¢ CRT ŠÖ”Žg‚¤‚Æo‚é‚â‚Â

#include "Test2.h"

//--------------------------------------------------------------------------------------
// Structures
//--------------------------------------------------------------------------------------
struct SimpleVertex
{
    XMFLOAT3 Pos;
    XMFLOAT3 Normal;
};

struct CBChangesEveryFrame
{
    XMMATRIX ViewProjection;
    XMFLOAT4 CameraPos;

    XMFLOAT4 LightPos;
    XMFLOAT4 LightColor;

    XMFLOAT4 MeshColor;
    FLOAT    MeshShininess;
    FLOAT    padding[3];
};


class PerspectiveCamera
{
private:
    XMMATRIX m_viewproj;
    XMMATRIX m_view;
    XMMATRIX m_proj;
    XMVECTOR m_eye;
    XMVECTOR m_focus;
    XMVECTOR m_up;
    FLOAT m_fovy;
    FLOAT m_aspect;
    FLOAT m_near;
    FLOAT m_far;

public:
    PerspectiveCamera() {}

    const XMMATRIX& getViewProjectionMatrix() const { return m_viewproj; }
    const XMMATRIX& getViewMatrix() const           { return m_view; }
    const XMMATRIX& getProjectionMatrix() const     { return m_proj; }
    XMVECTOR getEye() const     { return m_eye; }
    XMVECTOR getFocus() const   { return m_focus; }
    XMVECTOR getUp() const      { return m_up; }
    FLOAT getFovy() const       { return m_fovy; }
    FLOAT getAspect() const     { return m_aspect; }
    FLOAT getNear() const       { return m_near; }
    FLOAT getFar() const        { return m_far; }

    void setEye(XMVECTOR v) { m_eye=v; }

    void setView(XMVECTOR eye, XMVECTOR focus, XMVECTOR up)
    {
        m_eye   = eye;
        m_focus = focus;
        m_up    = up;
    }

    void setProjection(FLOAT fovy, FLOAT aspect, FLOAT _near, FLOAT _far)
    {
        m_fovy  = fovy;
        m_aspect= aspect;
        m_near  = _near;
        m_far   = _far;
    }

    void updateMatrix()
    {
        m_view      = XMMatrixLookAtLH( m_eye, m_focus, m_up );
        m_proj      = XMMatrixPerspectiveFovLH( m_fovy, m_aspect, m_near, m_far );
        m_viewproj  = XMMatrixMultiply( m_view, m_proj );
    }
};

class PerformanceCounter
{
private:
    LARGE_INTEGER m_start;
    LARGE_INTEGER m_end;

public:
    PerformanceCounter()
    {
        reset();
    }

    void reset()
    {
        ::QueryPerformanceCounter( &m_start );
    }

    float getElapsedSecond()
    {
        LARGE_INTEGER freq;
        ::QueryPerformanceCounter( &m_end );
        ::QueryPerformanceFrequency( &freq );
        return ((float)(m_end.QuadPart - m_start.QuadPart) / (float)freq.QuadPart);
    }

    float getElapsedMillisecond()
    {
        return getElapsedSecond()*1000.0f;
    }
};


//--------------------------------------------------------------------------------------
// Global Variables
//--------------------------------------------------------------------------------------
const int NUM_CUBE_INSTANCES = 512;

HINSTANCE                           g_hInst = NULL;
HWND                                g_hWnd = NULL;
D3D_DRIVER_TYPE                     g_driverType = D3D_DRIVER_TYPE_NULL;
D3D_FEATURE_LEVEL                   g_featureLevel = D3D_FEATURE_LEVEL_11_0;
ID3D11Device*                       g_pd3dDevice = NULL;
ID3D11DeviceContext*                g_pImmediateContext = NULL;
IDXGISwapChain*                     g_pSwapChain = NULL;
ID3D11RenderTargetView*             g_pRenderTargetView = NULL;
ID3D11Texture2D*                    g_pDepthStencil = NULL;
ID3D11DepthStencilView*             g_pDepthStencilView = NULL;

ID3D11VertexShader*                 g_pCubeVertexShader = NULL;
ID3D11InputLayout*                  g_pCubeVertexLayout = NULL;
ID3D11PixelShader*                  g_pCubePixelShader = NULL;
ID3D11Buffer*                       g_pCubeVertexBuffer = NULL;
ID3D11Buffer*                       g_pCubeInstanceBuffer = NULL;
ID3D11Buffer*                       g_pCubeIndexBuffer = NULL;
ID3D11Buffer*                       g_pCBChangesEveryFrame = NULL;
XMFLOAT4                            g_vMeshColor( 0.7f, 0.7f, 0.7f, 1.0f );
PerspectiveCamera                   g_camera;

#define MAX_PARTICLES 2048
Particle g_particles[MAX_PARTICLES];


DOL_ImportFunction(void, InitializeParticles, (Particle *particles, size_t num_particles));
DOL_ImportFunction(void, UpdateParticles, (Particle *particles, size_t num_particles));

DOL_Fixate float GenRand()
{
    return float((rand()-(RAND_MAX/2))*2) / (float)RAND_MAX;
}


//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow );
HRESULT InitDevice();
void CleanupDevice();
LRESULT CALLBACK    WndProc( HWND, UINT, WPARAM, LPARAM );
void Render();


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
{
    UNREFERENCED_PARAMETER( hPrevInstance );
    UNREFERENCED_PARAMETER( lpCmdLine );

    DOL_AddSourceDirectory(".\\");
#ifdef _WIN64
    DOL_StartAutoRecompile("/m /p:Configuration=Release;Platform=x64", true);
    DOL_LoadObjDirectory("x64\\Release");
#else // _WIN64
    DOL_StartAutoRecompile("/m /p:Configuration=Release;Platform=Win32", true);
    DOL_LoadObjDirectory("Release");
#endif // _WIN64
    DOL_Link();

    if( FAILED( InitWindow( hInstance, nCmdShow ) ) ) {
        return 0;
    }

    if( FAILED( InitDevice() ) )
    {
        CleanupDevice();
        return 0;
    }

    InitializeParticles(g_particles, _countof(g_particles));

    // Main message loop
    MSG msg = {0};
    while( WM_QUIT != msg.message )
    {
        if( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
        {
            TranslateMessage( &msg );
            DispatchMessage( &msg );
        }
        else
        {
            UpdateParticles(g_particles, _countof(g_particles));
            Render();
            DOL_ReloadAndLink();
        }
    }

    CleanupDevice();

    return ( int )msg.wParam;
}


//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow )
{
    // Register class
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof( WNDCLASSEX );
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = 0;
    wcex.hCursor = LoadCursor( NULL, IDC_ARROW );
    wcex.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"ispc_SPH_class";
    wcex.hIconSm = 0;
    if( !RegisterClassEx( &wcex ) )
        return E_FAIL;

    // Create window
    g_hInst = hInstance;
    RECT rc = { 0, 0, 1024, 768 };
    AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );
    g_hWnd = CreateWindow( L"ispc_SPH_class", L"ispc test", WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance,
                           NULL );
    if( !g_hWnd )
        return E_FAIL;

    ShowWindow( g_hWnd, nCmdShow );

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Helper for compiling shaders with D3DX11
//--------------------------------------------------------------------------------------
HRESULT CompileShaderFromFile( WCHAR* szFileName, LPCSTR szEntryPoint, LPCSTR szShaderModel, ID3DBlob** ppBlobOut )
{
    HRESULT hr = S_OK;

    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
    // Set the D3DCOMPILE_DEBUG flag to embed debug information in the shaders.
    // Setting this flag improves the shader debugging experience, but still allows 
    // the shaders to be optimized and to run exactly the way they will run in 
    // the release configuration of this program.
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

    ID3DBlob* pErrorBlob;
    hr = D3DX11CompileFromFile( szFileName, NULL, NULL, szEntryPoint, szShaderModel, 
        dwShaderFlags, 0, NULL, ppBlobOut, &pErrorBlob, NULL );
    if( FAILED(hr) )
    {
        if( pErrorBlob != NULL )
            OutputDebugStringA( (char*)pErrorBlob->GetBufferPointer() );
        if( pErrorBlob ) pErrorBlob->Release();
        return hr;
    }
    if( pErrorBlob ) pErrorBlob->Release();

    return S_OK;
}


ID3D11Buffer* CreateVertexBuffer(const void *data, UINT size)
{
    D3D11_BUFFER_DESC bd;
    ZeroMemory( &bd, sizeof(bd) );
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.ByteWidth = size;
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA InitData;
    ZeroMemory( &InitData, sizeof(InitData) );
    InitData.pSysMem = data;

    ID3D11Buffer *buffer;
    HRESULT hr = g_pd3dDevice->CreateBuffer( &bd, &InitData, &buffer );
    if( FAILED( hr ) ) {
        return NULL;
    }
    return buffer;
}



//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
HRESULT InitDevice()
{
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect( g_hWnd, &rc );
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_DRIVER_TYPE driverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
    };
    UINT numDriverTypes = ARRAYSIZE( driverTypes );

    D3D_FEATURE_LEVEL featureLevels[] =
    {
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    UINT numFeatureLevels = ARRAYSIZE( featureLevels );

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory( &sd, sizeof( sd ) );
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 1;
    sd.BufferDesc.RefreshRate.Denominator = 60;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

    for( UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++ )
    {
        g_driverType = driverTypes[driverTypeIndex];
        hr = D3D11CreateDeviceAndSwapChain( NULL, g_driverType, NULL, createDeviceFlags, featureLevels, numFeatureLevels,
                                            D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext );
        if( SUCCEEDED( hr ) )
            break;
    }
    if( FAILED( hr ) )
        return hr;

    // Create a render target view
    ID3D11Texture2D* pBackBuffer = NULL;
    hr = g_pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), ( LPVOID* )&pBackBuffer );
    if( FAILED( hr ) )
        return hr;

    hr = g_pd3dDevice->CreateRenderTargetView( pBackBuffer, NULL, &g_pRenderTargetView );
    pBackBuffer->Release();
    if( FAILED( hr ) )
        return hr;

    {
        // Create depth stencil texture
        D3D11_TEXTURE2D_DESC descDepth;
        ZeroMemory( &descDepth, sizeof(descDepth) );
        descDepth.Width = width;
        descDepth.Height = height;
        descDepth.MipLevels = 1;
        descDepth.ArraySize = 1;
        descDepth.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        descDepth.SampleDesc.Count = 1;
        descDepth.SampleDesc.Quality = 0;
        descDepth.Usage = D3D11_USAGE_DEFAULT;
        descDepth.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        descDepth.CPUAccessFlags = 0;
        descDepth.MiscFlags = 0;
        hr = g_pd3dDevice->CreateTexture2D( &descDepth, NULL, &g_pDepthStencil );
        if( FAILED( hr ) )
            return hr;

        // Create the depth stencil view
        D3D11_DEPTH_STENCIL_VIEW_DESC descDSV;
        ZeroMemory( &descDSV, sizeof(descDSV) );
        descDSV.Format = descDepth.Format;
        descDSV.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        descDSV.Texture2D.MipSlice = 0;
        hr = g_pd3dDevice->CreateDepthStencilView( g_pDepthStencil, &descDSV, &g_pDepthStencilView );
        if( FAILED( hr ) )
            return hr;
    }

    g_pImmediateContext->OMSetRenderTargets( 1, &g_pRenderTargetView, g_pDepthStencilView );

    // Setup the viewport
    {
        D3D11_VIEWPORT vp;
        vp.Width = (FLOAT)width;
        vp.Height = (FLOAT)height;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        g_pImmediateContext->RSSetViewports( 1, &vp );
    }

    {
        // Compile the vertex shader
        ID3DBlob* pVSBlob = NULL;
        hr = CompileShaderFromFile( L"Test2.fx", "VS", "vs_4_0", &pVSBlob );
        if( FAILED( hr ) )
        {
            MessageBox( NULL,
                        L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK );
            return hr;
        }

        // Create the vertex shader
        hr = g_pd3dDevice->CreateVertexShader( pVSBlob->GetBufferPointer(), pVSBlob->GetBufferSize(), NULL, &g_pCubeVertexShader );
        if( FAILED( hr ) )
        {    
            pVSBlob->Release();
            return hr;
        }

        // Create the input layout
        D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "INSTANCE_POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 1,  0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
        };
        UINT numElements = ARRAYSIZE( layout );

        hr = g_pd3dDevice->CreateInputLayout( layout, numElements, pVSBlob->GetBufferPointer(),
            pVSBlob->GetBufferSize(), &g_pCubeVertexLayout );
        pVSBlob->Release();
        if( FAILED( hr ) )
            return hr;
    }

    {
        // Compile the pixel shader
        ID3DBlob* pPSBlob = NULL;
        hr = CompileShaderFromFile( L"Test2.fx", "PS", "ps_4_0", &pPSBlob );
        if( FAILED( hr ) )
        {
            MessageBox( NULL,
                L"The FX file cannot be compiled.  Please run this executable from the directory that contains the FX file.", L"Error", MB_OK );
            return hr;
        }

        // Create the pixel shader
        hr = g_pd3dDevice->CreatePixelShader( pPSBlob->GetBufferPointer(), pPSBlob->GetBufferSize(), NULL, &g_pCubePixelShader );
        pPSBlob->Release();
        if( FAILED( hr ) )
            return hr;
    }

    // Create vertex buffer
    {
        SimpleVertex vertices[] =
        {
            { XMFLOAT3(-1.0f, 1.0f,-1.0f),  XMFLOAT3( 0.0f, 1.0f, 0.0f) },
            { XMFLOAT3( 1.0f, 1.0f,-1.0f),  XMFLOAT3( 0.0f, 1.0f, 0.0f) },
            { XMFLOAT3( 1.0f, 1.0f, 1.0f),  XMFLOAT3( 0.0f, 1.0f, 0.0f) },
            { XMFLOAT3(-1.0f, 1.0f, 1.0f),  XMFLOAT3( 0.0f, 1.0f, 0.0f) },

            { XMFLOAT3(-1.0f,-1.0f,-1.0f),  XMFLOAT3( 0.0f,-1.0f, 0.0f) },
            { XMFLOAT3( 1.0f,-1.0f,-1.0f),  XMFLOAT3( 0.0f,-1.0f, 0.0f) },
            { XMFLOAT3( 1.0f,-1.0f, 1.0f),  XMFLOAT3( 0.0f,-1.0f, 0.0f) },
            { XMFLOAT3(-1.0f,-1.0f, 1.0f),  XMFLOAT3( 0.0f,-1.0f, 0.0f) },

            { XMFLOAT3(-1.0f,-1.0f, 1.0f),  XMFLOAT3(-1.0f, 0.0f, 0.0f) },
            { XMFLOAT3(-1.0f,-1.0f,-1.0f),  XMFLOAT3(-1.0f, 0.0f, 0.0f) },
            { XMFLOAT3(-1.0f, 1.0f,-1.0f),  XMFLOAT3(-1.0f, 0.0f, 0.0f) },
            { XMFLOAT3(-1.0f, 1.0f, 1.0f),  XMFLOAT3(-1.0f, 0.0f, 0.0f) },

            { XMFLOAT3( 1.0f,-1.0f, 1.0f),  XMFLOAT3( 1.0f, 0.0f, 0.0f) },
            { XMFLOAT3( 1.0f,-1.0f,-1.0f),  XMFLOAT3( 1.0f, 0.0f, 0.0f) },
            { XMFLOAT3( 1.0f, 1.0f,-1.0f),  XMFLOAT3( 1.0f, 0.0f, 0.0f) },
            { XMFLOAT3( 1.0f, 1.0f, 1.0f),  XMFLOAT3( 1.0f, 0.0f, 0.0f) },

            { XMFLOAT3(-1.0f,-1.0f,-1.0f),  XMFLOAT3( 0.0f, 0.0f,-1.0f ) },
            { XMFLOAT3( 1.0f,-1.0f,-1.0f),  XMFLOAT3( 0.0f, 0.0f,-1.0f ) },
            { XMFLOAT3( 1.0f, 1.0f,-1.0f),  XMFLOAT3( 0.0f, 0.0f,-1.0f ) },
            { XMFLOAT3(-1.0f, 1.0f,-1.0f),  XMFLOAT3( 0.0f, 0.0f,-1.0f ) },

            { XMFLOAT3(-1.0f,-1.0f, 1.0f),  XMFLOAT3( 0.0f, 0.0f, 1.0f ) },
            { XMFLOAT3( 1.0f,-1.0f, 1.0f),  XMFLOAT3( 0.0f, 0.0f, 1.0f ) },
            { XMFLOAT3( 1.0f, 1.0f, 1.0f),  XMFLOAT3( 0.0f, 0.0f, 1.0f ) },
            { XMFLOAT3(-1.0f, 1.0f, 1.0f),  XMFLOAT3( 0.0f, 0.0f, 1.0f ) },
        };
        g_pCubeVertexBuffer = CreateVertexBuffer(vertices, sizeof(SimpleVertex)*ARRAYSIZE(vertices));
    }
    {
        g_pCubeInstanceBuffer = CreateVertexBuffer(g_particles, sizeof(Particle) * MAX_PARTICLES);
    }

    // Create index buffer
    {
        WORD indices[] =
        {
            3,1,0, 2,1,3,
            6,4,5, 7,4,6,
            11,9,8, 10,9,11,
            14,12,13, 15,12,14,
            19,17,16, 18,17,19,
            22,20,21, 23,20,22,
        };

        D3D11_BUFFER_DESC bd;
        ZeroMemory( &bd, sizeof(bd) );
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof( WORD ) * 36;
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        bd.CPUAccessFlags = 0;

        D3D11_SUBRESOURCE_DATA InitData;
        ZeroMemory( &InitData, sizeof(InitData) );
        InitData.pSysMem = indices;
        hr = g_pd3dDevice->CreateBuffer( &bd, &InitData, &g_pCubeIndexBuffer );
        if( FAILED( hr ) )
            return hr;
    }

    // Create the constant buffers
    {
        D3D11_BUFFER_DESC bd;
        ZeroMemory( &bd, sizeof(bd) );
        bd.Usage = D3D11_USAGE_DEFAULT;
        bd.ByteWidth = sizeof(CBChangesEveryFrame);
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.CPUAccessFlags = 0;
        hr = g_pd3dDevice->CreateBuffer( &bd, NULL, &g_pCBChangesEveryFrame );
        if( FAILED( hr ) )
            return hr;
    }

    // camera
    g_camera.setProjection( XMConvertToRadians(45.0f), width / (FLOAT)height, 0.1f, 100.0f );
    g_camera.setView(XMVectorSet( 0.0f, 10.0f, -12.5f, 0.0f ), XMVectorSet( 0.0f, 0.0f, 0.0f, 0.0f ), XMVectorSet( 0.0f, 0.0f, -1.0f, 0.0f ));

    return S_OK;
}


//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
void CleanupDevice()
{
    if( g_pImmediateContext ) g_pImmediateContext->ClearState();

    if( g_pCBChangesEveryFrame ) g_pCBChangesEveryFrame->Release();
    if( g_pCubeInstanceBuffer ) g_pCubeInstanceBuffer->Release();
    if( g_pCubeVertexBuffer ) g_pCubeVertexBuffer->Release();
    if( g_pCubeIndexBuffer ) g_pCubeIndexBuffer->Release();
    if( g_pCubeVertexLayout ) g_pCubeVertexLayout->Release();
    if( g_pCubeVertexShader ) g_pCubeVertexShader->Release();
    if( g_pCubePixelShader ) g_pCubePixelShader->Release();
    if( g_pDepthStencil ) g_pDepthStencil->Release();
    if( g_pDepthStencilView ) g_pDepthStencilView->Release();
    if( g_pRenderTargetView ) g_pRenderTargetView->Release();
    if( g_pSwapChain ) g_pSwapChain->Release();
    if( g_pImmediateContext ) g_pImmediateContext->Release();
    if( g_pd3dDevice ) g_pd3dDevice->Release();
}


//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
    PAINTSTRUCT ps;
    HDC hdc;

    switch( message )
    {
        case WM_PAINT:
            hdc = BeginPaint( hWnd, &ps );
            EndPaint( hWnd, &ps );
            break;

        case WM_DESTROY:
            PostQuitMessage( 0 );
            break;

        default:
            return DefWindowProc( hWnd, message, wParam, lParam );
    }

    return 0;
}


//--------------------------------------------------------------------------------------
// Render a frame
//--------------------------------------------------------------------------------------
void Render()
{
    // Update our time
    static float t = 0.0f;
    float delta_t = 0.0f;
    {
        static DWORD dwTimeStart = 0;
        DWORD dwTimeCur = GetTickCount();
        if( dwTimeStart == 0 )
            dwTimeStart = dwTimeCur;
        float old_t = t;
        t = ( dwTimeCur - dwTimeStart ) / 1000.0f;
        delta_t = t-old_t;
    }

    {
        static PerformanceCounter s_timer;
        static float s_prev = 0.0f;
        PerformanceCounter timer;

        g_pImmediateContext->UpdateSubresource( g_pCubeInstanceBuffer, 0, NULL, &g_particles, 0, 0 );
    }

    {
        CBChangesEveryFrame cb;
        XMVECTOR eye = g_camera.getEye();
        {
            XMMATRIX rot = XMMatrixRotationZ(XMConvertToRadians(0.1f));
            eye = XMVector4Transform(eye, rot);
        }
        g_camera.setEye(eye);
        g_camera.updateMatrix();
        XMMATRIX vp = g_camera.getViewProjectionMatrix();

        cb.ViewProjection   = XMMatrixTranspose( vp );
        cb.CameraPos        = (FLOAT*)&eye;

        cb.LightPos         = XMFLOAT4(10.0f, 10.0f, -10.0f, 1.0f);
        cb.LightColor       = XMFLOAT4(0.9f, 0.9f, 0.9f, 1.0f);

        cb.MeshShininess    = 200.0f;
        g_pImmediateContext->UpdateSubresource( g_pCBChangesEveryFrame, 0, NULL, &cb, 0, 0 );
    }


    float ClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f }; // red, green, blue, alpha
    g_pImmediateContext->ClearRenderTargetView( g_pRenderTargetView, ClearColor );
    g_pImmediateContext->ClearDepthStencilView( g_pDepthStencilView, D3D11_CLEAR_DEPTH, 1.0f, 0 );

    {
        ID3D11Buffer *buffers[] = {g_pCubeVertexBuffer, g_pCubeInstanceBuffer};
        UINT strides[] = {sizeof(SimpleVertex), sizeof(Particle), };
        UINT offsets[] = {0, 0};
        g_pImmediateContext->IASetVertexBuffers( 0, ARRAYSIZE(buffers), buffers, strides, offsets );
    }
    g_pImmediateContext->IASetInputLayout( g_pCubeVertexLayout );
    g_pImmediateContext->IASetIndexBuffer( g_pCubeIndexBuffer, DXGI_FORMAT_R16_UINT, 0 );
    g_pImmediateContext->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    // Render the cube
    g_pImmediateContext->VSSetShader( g_pCubeVertexShader, NULL, 0 );
    g_pImmediateContext->VSSetConstantBuffers( 0, 1, &g_pCBChangesEveryFrame );
    g_pImmediateContext->PSSetShader( g_pCubePixelShader, NULL, 0 );
    g_pImmediateContext->PSSetConstantBuffers( 0, 1, &g_pCBChangesEveryFrame );

    g_pImmediateContext->DrawIndexedInstanced( 36, (UINT)MAX_PARTICLES, 0, 0, 0 );

    // Present our back buffer to our front buffer
    g_pSwapChain->Present( 1, 0 ); // vsync on
    //g_pSwapChain->Present( 0, 0 ); // vsync off
}
