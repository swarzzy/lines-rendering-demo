#include "RendererAPI.h"
#include "../Logging.h"
#include "../core/CoreAPI.h"

#include "d3d11.h"
#include "d3dcompiler.h"

#include <windows.h>

#define Win32Call(xx) do { if (FAILED((xx))) {BreakDebug();}} while(false)

typedef struct
{
    ID3D11Texture2D* colorTexture;
    ID3D11Texture2D* depthStencilTexture;
    ID3D11RenderTargetView* renderTargetView;
    ID3D11DepthStencilView* depthStencilView;
    ID3D11ShaderResourceView* colorSrv;
} RenderTargetData;

typedef struct
{
    CoreState* core;
    IDXGIAdapter* adapter;
    IDXGIOutput* output;
    ID3D11Device* device;
    ID3D11DeviceContext* deviceContext;
    HWND windowHandle;
    UINT syncInterval;
    DXGI_MODE_DESC desktopDisplayMode;
    DXGI_MODE_DESC currentBackBufferParams;
    ID3D11RenderTargetView* renderTargetView;
    ID3D11Texture2D* depthStencilTexture;
    ID3D11DepthStencilView* depthStencilView;
    IDXGISwapChain* swapChain;

    ID3D11Buffer* immediateVertexBuffer;

    RenderTargetData screenRenderTarget;

    ID3D11VertexShader* quadVertexShader;
    ID3D11PixelShader* quadPixelShader;
    ID3D11InputLayout* quadShaderVertLayout;
    ID3D11Buffer* quadCbuffer;

    ID3D11VertexShader* sdfVertexShader;
    ID3D11PixelShader* sdfPixelShader;
    ID3D11InputLayout* sdfShaderVertLayout;
    ID3D11Buffer* sdfCbuffer;
    ID3D11BlendState* sdfBlendState;

    ID3D11VertexShader* blitVertexShader;
    ID3D11PixelShader* blitPixelShader;
    ID3D11InputLayout* blitShaderVertLayout;
    ID3D11Buffer* blitVertexBuffer;
    ID3D11SamplerState* blitSampler;
    ID3D11DepthStencilState* blitDepthStencilState;
    ID3D11RasterizerState* blitRasterizerState;

    ID3D11RasterizerState* rasterizerState;
    ID3D11DepthStencilState* depthStencilState;

    RenderCommandEntry* lastMaterialCommand;

} RendererContext;

typedef struct
{
    Vector3 position;
    Vector2 uv;
} BlitVertex;

static int Initialized = 0;
static RendererAPI GlobalApi;
static RendererContext GlobalRendererContext;

SamplerDescriptor CreateSampler(TextureSamplerSettings sampler);

RendererContext* GetRendererContext()
{
    return &GlobalRendererContext;
}

void _WriteLog(CoreLogLevel logLevel, const char* tags, u32 tagsCount, const char* format, ...)
{
    va_list args;
    va_start(args, format);
    RendererContext* context = GetRendererContext();
    context->core->coreAPI.WriteLog(logLevel, tags, tagsCount, format, args);
    va_end(args);
}

#define Log_Trace(format, ...)
#define Log_Error(format, ...) _WriteLog(CoreLogLevel_Error, "D3D11", 0, format, ##__VA_ARGS__)
#define Log_Info(format, ...) _WriteLog(CoreLogLevel_Info, "D3D11", 0, format, ##__VA_ARGS__)
#define Assert(x) assert(x) // TODO: Core assert

struct QuadConstantBufferLayout
{ 
    Matrix4x4 transform;
};

struct TextSdfConstantBufferLayout
{ 
    Matrix4x4 transform;
    Vector4 params;
};

void InitializeApi(RendererAPI*);

void CreateDeviceD3D11()
{
    RendererContext* renderer = GetRendererContext();

    IDXGIFactory* factory = nullptr;
    auto createDXGIFactoryResult = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&factory);
    Debug_Assert(createDXGIFactoryResult == S_OK);

    Debug_LogInfo("Available adapters:\n");
    IDXGIAdapter* adapter = nullptr;
    IDXGIAdapter* defaultAdapter = nullptr;
    UINT adapterIndex = 0;
    while (factory->EnumAdapters(adapterIndex, &adapter) != DXGI_ERROR_NOT_FOUND)
    {
        adapterIndex++;
        DXGI_ADAPTER_DESC desc;
        if (adapter->GetDesc(&desc) == S_OK)
        {
            Debug_LogInfo("%S\n", desc.Description);
        }

        if (!defaultAdapter)
        {
            defaultAdapter = adapter;
        }
        else
        {
            adapter->Release();
        }
    }

    factory->Release();

    IDXGIOutput* output = nullptr;
    // NOTE: Primary output
    Win32Call(defaultAdapter->EnumOutputs(0, &output));

    renderer->output = output;
    renderer->adapter = defaultAdapter;

    D3D_FEATURE_LEVEL featureLevel;
    ID3D11Device* device;
    ID3D11DeviceContext* context;
    UINT flags = D3D11_CREATE_DEVICE_DEBUG;
    auto createDeviceResult = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, flags, 0, 0, D3D11_SDK_VERSION, &device, &featureLevel, &context);

    Debug_Assert(!FAILED(createDeviceResult));
    Debug_Assert(featureLevel == D3D_FEATURE_LEVEL_11_0);

    renderer->device = device;
    renderer->deviceContext = context;
}

ID3D11RenderTargetView* CreateSwapChainRenderTragetView()
{
    RendererContext* renderer = GetRendererContext();

    ID3D11RenderTargetView* result = nullptr;
    ID3D11Texture2D* backBuffer;
    if (!FAILED(renderer->swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer)))
    {
        ID3D11RenderTargetView* renderTargetView;
        if (!FAILED(renderer->device->CreateRenderTargetView(backBuffer, 0, &renderTargetView)))
        {
            result = renderTargetView;
        }
        backBuffer->Release();
    }
    return result;
}

struct CreateSwapChainDepthStencilTragetResult
{
    ID3D11DepthStencilView* view;
    ID3D11Texture2D* texture;
};

CreateSwapChainDepthStencilTragetResult CreateSwapChainDepthStencilTraget(DXGI_MODE_DESC displayMode)
{
    RendererContext* renderer = GetRendererContext();

    D3D11_TEXTURE2D_DESC depthStencilDesc{};
    depthStencilDesc.Width = displayMode.Width;
    depthStencilDesc.Height = displayMode.Height;
    depthStencilDesc.MipLevels = 1;
    depthStencilDesc.ArraySize = 1;
    depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
    depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthStencilDesc.CPUAccessFlags = 0;
    depthStencilDesc.MiscFlags = 0;
    depthStencilDesc.SampleDesc.Count = 1;
    depthStencilDesc.SampleDesc.Quality = 0;

    ID3D11Texture2D* depthStencilBuffer = nullptr;
    ID3D11DepthStencilView* depthStencilView = nullptr;
    if (!FAILED(renderer->device->CreateTexture2D(&depthStencilDesc, 0, &depthStencilBuffer)))
    {
        if (!FAILED(renderer->device->CreateDepthStencilView(depthStencilBuffer, 0, &depthStencilView)))
        {
        }
    }

    return {depthStencilView, depthStencilBuffer};
}

struct CompiledShader
{
    ID3D11VertexShader* vertexShader;
    ID3D11PixelShader* pixelShader;
    ID3DBlob* vertexShaderBinary;
    ID3DBlob* pixelShaderBinary;
    ID3DBlob* vertexCompilationLog;
    ID3DBlob* pixelCompilationLog;
};

CompiledShader CreateShaderFromFile(RendererContext* renderer, LPCWSTR filename, const char* vertexName, const char* pixelName)
{
    ID3D11VertexShader* vertexShader;
    ID3D11PixelShader* pixelShader;
    ID3DBlob* vertCompilationLog;
    ID3DBlob* pixelCompilationLog;

    ID3DBlob* vertexShaderBinary;
    D3DCompileFromFile(filename, NULL, NULL, vertexName, "vs_5_0", 0, 0, &vertexShaderBinary, &vertCompilationLog);
    if (vertexShaderBinary != NULL)
    {
        HRESULT vertResult = renderer->device->CreateVertexShader(vertexShaderBinary->GetBufferPointer(), vertexShaderBinary->GetBufferSize(), NULL, &vertexShader);
    }

    ID3DBlob* pixelShaderBinary;
    D3DCompileFromFile(filename, NULL, NULL, pixelName, "ps_5_0", 0, 0, &pixelShaderBinary, &pixelCompilationLog);
    if (pixelShaderBinary != NULL)
    {
        HRESULT pixelResult = renderer->device->CreatePixelShader(pixelShaderBinary->GetBufferPointer(), pixelShaderBinary->GetBufferSize(), NULL, &pixelShader);
    }

    CompiledShader result {};
    if (vertexShader != NULL && pixelShader != NULL)
    {
        result.vertexShaderBinary = vertexShaderBinary;
        result.pixelShaderBinary = pixelShaderBinary;
        result.vertexShader = vertexShader;
        result.pixelShader = pixelShader;
    }

    result.vertexCompilationLog = vertCompilationLog;
    result.pixelCompilationLog = pixelCompilationLog;

    return result;
}

void PrintShaderLog(ID3DBlob* logBlob)
{
    if (logBlob != NULL)
    {
        char* log = (char*)logBlob->GetBufferPointer();

        if (log != NULL)
        {
            Log_Error(log);
        }
    }
}

void ReleaseRenderTarget(RenderTargetData* rt)
{
    if (rt->colorTexture != NULL)
    {
        rt->colorTexture->Release();
    }

    if (rt->depthStencilTexture != NULL)
    {
        rt->depthStencilTexture->Release();
    }

    if (rt->renderTargetView != NULL)
    {
        rt->renderTargetView->Release();
    }

    if (rt->depthStencilView != NULL)
    {
        rt->depthStencilView->Release();
    }

    if (rt->colorSrv != NULL)
    {
        rt->colorSrv->Release();
    }
}

RenderTargetData CreateScreenRT(RendererContext* renderer, u32 width, u32 height)
{
    ID3D11Texture2D* colorTexture;
    ID3D11Texture2D* depthStencilTexture;

    ID3D11RenderTargetView* renderTargetView;
    ID3D11ShaderResourceView* renderTargetSrv;
    ID3D11DepthStencilView* depthStencilView;


    D3D11_TEXTURE2D_DESC rtDesc {};
    D3D11_TEXTURE2D_DESC depthDesc {};
    D3D11_RENDER_TARGET_VIEW_DESC rtViewDesc {};
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    D3D11_DEPTH_STENCIL_VIEW_DESC depthViewDesc {};

    rtDesc.Width = width;
    rtDesc.Height = height;
    rtDesc.MipLevels = 1;
    rtDesc.ArraySize = 1;
    rtDesc.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
    rtDesc.SampleDesc.Count = 1;
    rtDesc.Usage = D3D11_USAGE_DEFAULT;
    rtDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    rtDesc.CPUAccessFlags = 0;
    rtDesc.MiscFlags = 0;

    Win32Call(renderer->device->CreateTexture2D(&rtDesc, NULL, &colorTexture));

    depthDesc.Width = width;
    depthDesc.Height = height;
    depthDesc.MipLevels = 1;
    depthDesc.ArraySize = 1;
    depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthDesc.SampleDesc.Count = 1;
    depthDesc.Usage = D3D11_USAGE_DEFAULT;
    depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depthDesc.CPUAccessFlags = 0;
    depthDesc.MiscFlags = 0;

    Win32Call(renderer->device->CreateTexture2D(&depthDesc, NULL, &depthStencilTexture));

    rtViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    rtViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtViewDesc.Texture2D.MipSlice = 0;

    Win32Call(renderer->device->CreateRenderTargetView(colorTexture, &rtViewDesc, &renderTargetView));

    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;

    Win32Call(renderer->device->CreateShaderResourceView(colorTexture, &srvDesc, &renderTargetSrv));

    depthViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthViewDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    depthViewDesc.Texture2D.MipSlice = 0;

    Win32Call(renderer->device->CreateDepthStencilView(depthStencilTexture, &depthViewDesc, &depthStencilView));

    RenderTargetData result {};
    result.colorTexture = colorTexture;
    result.depthStencilTexture = depthStencilTexture;
    result.renderTargetView = renderTargetView;
    result.depthStencilView = depthStencilView;
    result.colorSrv = renderTargetSrv;

    return result;
}

DisplayParams CreateContextD3D11(HWND hwnd, DisplayParams intialDisplayParams)
{
    RendererContext* renderer = GetRendererContext();

    DEVMODEA devmode;
    devmode.dmSize = sizeof(devmode);
    devmode.dmDriverExtra = 0;

    BOOL displaySettingsResult = EnumDisplaySettingsA(nullptr, ENUM_CURRENT_SETTINGS, &devmode);
    Debug_Assert(displaySettingsResult != 0); // TODO: Handle failure. Probably set some default hardcoded values on fail.

    renderer->desktopDisplayMode.Width = devmode.dmPelsWidth;
    renderer->desktopDisplayMode.Height = devmode.dmPelsWidth;
    renderer->desktopDisplayMode.RefreshRate.Numerator = devmode.dmDisplayFrequency;
    renderer->desktopDisplayMode.RefreshRate.Denominator = 1;

    DXGI_MODE_DESC desiredDisplayMode {};
    desiredDisplayMode.Width = intialDisplayParams.width;
    desiredDisplayMode.Height = intialDisplayParams.height;
    desiredDisplayMode.RefreshRate = renderer->desktopDisplayMode.RefreshRate;
    desiredDisplayMode.Scaling = DXGI_MODE_SCALING_STRETCHED;
    desiredDisplayMode.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    DXGI_MODE_DESC displayMode {};
    Win32Call(renderer->output->FindClosestMatchingMode(&desiredDisplayMode, &displayMode, renderer->device));
    Debug_LogInfo("[D3D11] Initializing swap chain with display mode: %lu x %lu @%luHz\n", displayMode.Width, displayMode.Height, displayMode.RefreshRate.Numerator / displayMode.RefreshRate.Denominator);

    renderer->currentBackBufferParams = displayMode;

    renderer->windowHandle = hwnd;

    DXGI_SWAP_CHAIN_DESC swapChainDesc {};
    swapChainDesc.BufferDesc = displayMode;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 1;
    swapChainDesc.OutputWindow = hwnd;
    swapChainDesc.Windowed = true;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    IDXGIDevice* dxgiDevice = nullptr;
    auto queryDxgiDeviceResult = renderer->device->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice);
    Debug_Assert(queryDxgiDeviceResult == 0);

    IDXGIAdapter* dxgiAdapter = nullptr;
    auto queryDxgiAdapterResult = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&dxgiAdapter);
    Debug_Assert(queryDxgiAdapterResult == 0);

    IDXGIFactory* dxgiFactory = nullptr;
    auto queryDxgiFactoryResult = dxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&dxgiFactory);
    Debug_Assert(queryDxgiFactoryResult == 0);

    IDXGISwapChain* swapChain = nullptr;
    auto swapChainCreateResult = dxgiFactory->CreateSwapChain(renderer->device, &swapChainDesc, &swapChain);
    Debug_Assert(swapChainCreateResult == 0);
    renderer->swapChain = swapChain;

    dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    dxgiDevice->Release();
    dxgiAdapter->Release();
    dxgiFactory->Release();

    renderer->renderTargetView = CreateSwapChainRenderTragetView();
    auto swapChainResult = CreateSwapChainDepthStencilTraget(displayMode);
    renderer->depthStencilView = swapChainResult.view;
    renderer->depthStencilTexture = swapChainResult.texture;

    renderer->deviceContext->OMSetRenderTargets(1, &renderer->renderTargetView, renderer->depthStencilView);

    DisplayParams actualDisplayParams{};
    actualDisplayParams.width = displayMode.Width;
    actualDisplayParams.height = displayMode.Height;

    renderer->screenRenderTarget = CreateScreenRT(renderer, displayMode.Width, displayMode.Height);

    CompiledShader quadShader = CreateShaderFromFile(renderer, L"../../assets/shaders/d3d11/Quad.hlsl", "Vertex", "Pixel");
    PrintShaderLog(quadShader.vertexCompilationLog);
    PrintShaderLog(quadShader.pixelCompilationLog);

    if (quadShader.vertexShader == NULL || quadShader.pixelShader == NULL)
    {
        Assert(false);
    }

    renderer->quadVertexShader = quadShader.vertexShader;
    renderer->quadPixelShader = quadShader.pixelShader;

    CompiledShader sdfShader = CreateShaderFromFile(renderer, L"../../assets/shaders/d3d11/TextSDF.hlsl", "Vertex", "Pixel");
    PrintShaderLog(sdfShader.vertexCompilationLog);
    PrintShaderLog(sdfShader.pixelCompilationLog);

    if (sdfShader.vertexShader == NULL || sdfShader.pixelShader == NULL)
    {
        Assert(false);
    }

    renderer->sdfVertexShader = sdfShader.vertexShader;
    renderer->sdfPixelShader = sdfShader.pixelShader;

    CompiledShader blitShader = CreateShaderFromFile(renderer, L"../../assets/shaders/d3d11/Blit.hlsl", "Vertex", "Pixel");
    PrintShaderLog(blitShader.vertexCompilationLog);
    PrintShaderLog(blitShader.pixelCompilationLog);

    if (blitShader.vertexShader == NULL || blitShader.pixelShader == NULL)
    {
        Assert(false);
    }

    renderer->blitVertexShader = blitShader.vertexShader;
    renderer->blitPixelShader = blitShader.pixelShader;


    D3D11_INPUT_ELEMENT_DESC layoutDesc[] =
        {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };

    Win32Call(renderer->device->CreateInputLayout(layoutDesc, ArrayCount(layoutDesc), quadShader.vertexShaderBinary->GetBufferPointer(), quadShader.vertexShaderBinary->GetBufferSize(), &(renderer->quadShaderVertLayout)));

    Win32Call(renderer->device->CreateInputLayout(layoutDesc, ArrayCount(layoutDesc), sdfShader.vertexShaderBinary->GetBufferPointer(), sdfShader.vertexShaderBinary->GetBufferSize(), &(renderer->sdfShaderVertLayout)));

    D3D11_INPUT_ELEMENT_DESC blitLayoutDesc[] =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    Win32Call(renderer->device->CreateInputLayout(blitLayoutDesc, ArrayCount(blitLayoutDesc), blitShader.vertexShaderBinary->GetBufferPointer(), blitShader.vertexShaderBinary->GetBufferSize(), &(renderer->blitShaderVertLayout)));

    D3D11_BLEND_DESC sdfBlendDesc = {};
    sdfBlendDesc.RenderTarget[0].BlendEnable = true;
    sdfBlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    sdfBlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    sdfBlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    sdfBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    sdfBlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    sdfBlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    sdfBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    Win32Call(renderer->device->CreateBlendState(&sdfBlendDesc, &renderer->sdfBlendState));

    D3D11_BUFFER_DESC cbufferDesc = {};
    cbufferDesc.ByteWidth = sizeof(QuadConstantBufferLayout);
    cbufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    Win32Call(renderer->device->CreateBuffer(&cbufferDesc, NULL, &renderer->quadCbuffer));

    cbufferDesc = {};
    cbufferDesc.ByteWidth = sizeof(TextSdfConstantBufferLayout);
    cbufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    cbufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    Win32Call(renderer->device->CreateBuffer(&cbufferDesc, NULL, &renderer->sdfCbuffer));

    D3D11_RASTERIZER_DESC rasterizerDesc = {};
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_NONE;
    Win32Call(renderer->device->CreateRasterizerState(&rasterizerDesc, &renderer->rasterizerState));

    Win32Call(renderer->device->CreateRasterizerState(&rasterizerDesc, &renderer->blitRasterizerState));

    D3D11_DEPTH_STENCIL_DESC depthStencilDesc = {};
    depthStencilDesc.DepthEnable = TRUE;
    depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    depthStencilDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    Win32Call(renderer->device->CreateDepthStencilState(&depthStencilDesc, &renderer->depthStencilState));

    D3D11_DEPTH_STENCIL_DESC blitDepthStencilDesc = {};
    blitDepthStencilDesc.DepthEnable = 0;
    blitDepthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    blitDepthStencilDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
    Win32Call(renderer->device->CreateDepthStencilState(&depthStencilDesc, &renderer->blitDepthStencilState));

    BlitVertex blitQuad[6];
    blitQuad[0].position = MakeVector3(-1.0f, -1.0f, 0.0f);
    blitQuad[0].uv = MakeVector2(0.0f, 1.0f);
    blitQuad[1].position = MakeVector3(1.0f, -1.0f, 0.0f);
    blitQuad[1].uv = MakeVector2(1.0f, 1.0f);
    blitQuad[2].position = MakeVector3(1.0f, 1.0f, 0.0f);
    blitQuad[2].uv = MakeVector2(1.0f, 0.0f);
    blitQuad[3].position = MakeVector3(1.0f, 1.0f, 0.0f);
    blitQuad[3].uv = MakeVector2(1.0f, 0.0f);
    blitQuad[4].position = MakeVector3(-1.0f, 1.0f, 0.0f);
    blitQuad[4].uv = MakeVector2(0.0f, 0.0f);
    blitQuad[5].position = MakeVector3(-1.0f, -1.0f, 0.0f);
    blitQuad[5].uv = MakeVector2(0.0f, 1.0f);

    D3D11_BUFFER_DESC blitVertBufferDesc = {0};
    blitVertBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    blitVertBufferDesc.ByteWidth = ArrayCount(blitQuad) * sizeof(BlitVertex);
    blitVertBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    blitVertBufferDesc.CPUAccessFlags = 0;
    blitVertBufferDesc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA blitVertDataDesc = {};
    blitVertDataDesc.pSysMem = &blitQuad;
    blitVertDataDesc.SysMemPitch = 0;
    blitVertDataDesc.SysMemSlicePitch = 0;

    Win32Call(renderer->device->CreateBuffer(&blitVertBufferDesc, &blitVertDataDesc, &renderer->blitVertexBuffer));

    TextureSamplerSettings samplerDesc {};
    samplerDesc.filtering = TextureFiltering_Point;
    renderer->blitSampler = (ID3D11SamplerState*)CreateSampler(samplerDesc).data0;

    return actualDisplayParams;
}

void Initialize(uptr hwnd, DisplayParams desiredDisplayParams, DisplayParams* actualDisplayParams)
{
    CreateDeviceD3D11();
    auto createdParams = CreateContextD3D11((HWND)hwnd, desiredDisplayParams);
    (*actualDisplayParams) = createdParams;
}

RendererAPI* InitializeRenderer_D3D11(CoreState* coreContext, uptr hwnd, DisplayParams desiredDisplayParams, DisplayParams* actualDisplayParams, struct ID3D11Device** device, struct ID3D11DeviceContext** deviceContext)
{
    if (Initialized == 0)
    {
        RendererContext* context = GetRendererContext();
        context->core = coreContext;
        InitializeApi(&GlobalApi);
        Initialize(hwnd, desiredDisplayParams, actualDisplayParams);
        Initialized = 1;
    }

    RendererContext* renderer = GetRendererContext();
    *device = renderer->device;
    *deviceContext = renderer->deviceContext;
    return &GlobalApi;
}

DisplayParams SetDisplayParams(DisplayParams displayParams)
{
    RendererContext* renderer = GetRendererContext();

    DXGI_MODE_DESC desiredDisplayMode{};
    desiredDisplayMode.Width = displayParams.width;
    desiredDisplayMode.Height = displayParams.height;
    desiredDisplayMode.RefreshRate = renderer->desktopDisplayMode.RefreshRate;
    desiredDisplayMode.Scaling = DXGI_MODE_SCALING_STRETCHED;
    desiredDisplayMode.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    DXGI_MODE_DESC displayMode{};
    Win32Call(renderer->output->FindClosestMatchingMode(&desiredDisplayMode, &displayMode, renderer->device));

    renderer->deviceContext->OMSetRenderTargets(0, 0, 0);
    renderer->renderTargetView->Release();
    renderer->depthStencilView->Release();
    renderer->depthStencilTexture->Release();

    Win32Call(renderer->swapChain->ResizeTarget(&displayMode));
    Win32Call(renderer->swapChain->ResizeBuffers(0, displayMode.Width, displayMode.Height, DXGI_FORMAT_UNKNOWN, 0));

    renderer->renderTargetView = CreateSwapChainRenderTragetView();
    auto swapChainResult = CreateSwapChainDepthStencilTraget(displayMode);
    renderer->depthStencilView = swapChainResult.view;
    renderer->depthStencilTexture = swapChainResult.texture;
    renderer->deviceContext->OMSetRenderTargets(1, &renderer->renderTargetView, renderer->depthStencilView);

    renderer->currentBackBufferParams = displayMode;

    Debug_LogInfo("[D3D11] Changed back buffer params: %lu x %lu @%luHz\n", displayMode.Width, displayMode.Height, displayMode.RefreshRate.Numerator / displayMode.RefreshRate.Denominator);

    DisplayParams actualDisplayParams{};
    actualDisplayParams.width = displayMode.Width;
    actualDisplayParams.height = displayMode.Height;

    ReleaseRenderTarget(&renderer->screenRenderTarget);
    renderer->screenRenderTarget = CreateScreenRT(renderer, displayMode.Width, displayMode.Height);

    return actualDisplayParams;
}

void SetVsyncMode(VSyncMode mode)
{
    RendererContext* renderer = GetRendererContext();

    switch (mode)
    {
        case VSyncMode_Disabled: { renderer->syncInterval = 0; } break;
        case VSyncMode_Full: { renderer->syncInterval = 1; } break;
        case VSyncMode_Half: { renderer->syncInterval = 2; } break;
        default: { renderer->syncInterval = 0; } break;
    }
}

void SwapScreenBuffers()
{
    RendererContext* renderer = GetRendererContext();
    renderer->swapChain->Present(renderer->syncInterval, 0);
}

void SetViewport(Vector2 min, Vector2 dimensions)
{
    RendererContext* renderer = GetRendererContext();
    D3D11_VIEWPORT viewportDesc{};
    viewportDesc.TopLeftX = min.x;
    viewportDesc.TopLeftY = min.y;
    viewportDesc.Width = dimensions.x;
    viewportDesc.Height = dimensions.y;
    viewportDesc.MinDepth = 0.0f;
    viewportDesc.MaxDepth = 1.0f;

    renderer->deviceContext->RSSetViewports(1, &viewportDesc);
}

DXGI_FORMAT MapTextureFormat(TextureFormat format)
{
    switch (format)
    {
    case TextureFormat_SRGB24_A8: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case TextureFormat_sRGB_DXT1: return DXGI_FORMAT_BC1_UNORM_SRGB;
    case TextureFormat_sRGBA_DXT5: return DXGI_FORMAT_BC3_UNORM_SRGB;
    //case TextureFormat::SRGB8: { result.internal = GL_SRGB8; result.format = GL_RGB; result.type = GL_UNSIGNED_BYTE; } break;
    //case TextureFormat::RGBA8: { result.internal = GL_RGBA8; result.format = GL_RGBA; result.type = GL_UNSIGNED_BYTE; } break;
    //case TextureFormat::RGB8: { result.internal = GL_RGB8; result.format = GL_RGB; result.type = GL_UNSIGNED_BYTE; } break;
    //case TextureFormat::RGB16F: { result.internal = GL_RGB16F; result.format = GL_RGB; result.type = GL_FLOAT; } break;
    //case TextureFormat::RG16F: { result.internal = GL_RG16F; result.format = GL_RG; result.type = GL_FLOAT; } break;
    //case TextureFormat::RG32F: { result.internal = GL_RG32F; result.format = GL_RG; result.type = GL_FLOAT; } break;
    case TextureFormat_R8: return DXGI_FORMAT_R8_UNORM;
    //case TextureFormat::RG8: { result.internal = GL_RG8; result.format = GL_RG; result.type = GL_UNSIGNED_BYTE; } break;
    InvalidDefault();
    }

    return (DXGI_FORMAT)0;
}

u32 GetTexturePitchFromFormat(TextureFormat format, u32 width)
{
    switch (format)
    {
    case TextureFormat_SRGB24_A8: return 4 * width;
    case TextureFormat_sRGB_DXT1: return 8 * (width / 4);
    case TextureFormat_sRGBA_DXT5: return 16 * (width / 4);
    //case TextureFormat::SRGB8: { result.internal = GL_SRGB8; result.format = GL_RGB; result.type = GL_UNSIGNED_BYTE; } break;
    //case TextureFormat::RGBA8: { result.internal = GL_RGBA8; result.format = GL_RGBA; result.type = GL_UNSIGNED_BYTE; } break;
    //case TextureFormat::RGB8: { result.internal = GL_RGB8; result.format = GL_RGB; result.type = GL_UNSIGNED_BYTE; } break;
    //case TextureFormat::RGB16F: { result.internal = GL_RGB16F; result.format = GL_RGB; result.type = GL_FLOAT; } break;
    //case TextureFormat::RG16F: { result.internal = GL_RG16F; result.format = GL_RG; result.type = GL_FLOAT; } break;
    //case TextureFormat::RG32F: { result.internal = GL_RG32F; result.format = GL_RG; result.type = GL_FLOAT; } break;
    case TextureFormat_R8: return 1 * width;
    //case TextureFormat::RG8: { result.internal = GL_RG8; result.format = GL_RG; result.type = GL_UNSIGNED_BYTE; } break;
    InvalidDefault();
    }

    return (DXGI_FORMAT)0;
}

D3D11_FILTER GetFiltering(TextureSamplerSettings sampler)
{
    switch (sampler.filtering)
    {
    case TextureFiltering_Point: return D3D11_FILTER_MIN_MAG_MIP_POINT;
    case TextureFiltering_Bilinear: return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    InvalidDefault();
    }
    return (D3D11_FILTER)0;

}

SamplerDescriptor CreateSampler(TextureSamplerSettings sampler)
{
    RendererContext* renderer = GetRendererContext();

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = GetFiltering(sampler);
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

    ID3D11SamplerState* samplerState;

    HRESULT result = renderer->device->CreateSamplerState(&samplerDesc, &samplerState);
    if (FAILED(result))
    {
        return {};
    }

    SamplerDescriptor res = {};
    res.data0 = (u64)samplerState;

    return res;
}

Texture2D LoadTexture2D(u32 width, u32 height, TextureFormat format, void* data, uptr dataSize)
{
    RendererContext* renderer = GetRendererContext();

    D3D11_TEXTURE2D_DESC desc = {0};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = MapTextureFormat(format);
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    //desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    desc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA dataDesc = {0};
    dataDesc.pSysMem = data;
    dataDesc.SysMemPitch = GetTexturePitchFromFormat(format, width);

    ID3D11Texture2D* texture = NULL;
    HRESULT result = renderer->device->CreateTexture2D(&desc, &dataDesc, &texture);
    if (FAILED(result))
    {
        texture = NULL;
    }

    ID3D11ShaderResourceView* textureSRV;
    HRESULT srvResult = renderer->device->CreateShaderResourceView(texture, nullptr, &textureSRV);
    if (FAILED(srvResult))
    {
        textureSRV = NULL;
    }

    Texture2D resultTexture;
    resultTexture.id.data0 = (u64)texture;
    resultTexture.id.data1 = (u64)textureSRV;
    resultTexture.width = width;
    resultTexture.height = height;
    resultTexture.format = format;

    return resultTexture;
}

void UnloadTexture2D(TextureDescriptor id)
{
    if (id.data0 != 0)
    {
        ID3D11Texture2D* texture = (ID3D11Texture2D*)id.data0;
        texture->Release();
    }
}

void Blit(RendererContext* renderer, ID3D11RenderTargetView* dst, ID3D11ShaderResourceView* src)
{
    UINT offset = 0;
    UINT stride = sizeof(BlitVertex);

    renderer->deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    renderer->deviceContext->IASetInputLayout(renderer->blitShaderVertLayout);
    renderer->deviceContext->IASetVertexBuffers(0, 1, &renderer->blitVertexBuffer, &stride, &offset);
    renderer->deviceContext->VSSetShader(renderer->blitVertexShader, nullptr, 0);
    renderer->deviceContext->RSSetState(renderer->blitRasterizerState);
    renderer->deviceContext->PSSetShader(renderer->blitPixelShader, nullptr, 0);
    renderer->deviceContext->PSSetShaderResources(0, 1, &src);
    renderer->deviceContext->PSSetSamplers(0, 1, &renderer->blitSampler);
    renderer->deviceContext->OMSetDepthStencilState(renderer->blitDepthStencilState, 0);
    renderer->deviceContext->OMSetBlendState(NULL, NULL, 0xffffffff);
    renderer->deviceContext->OMSetRenderTargets(1, &dst, NULL);
    renderer->deviceContext->Draw(6, 0);

    ID3D11ShaderResourceView* nullSRV = NULL;
    renderer->deviceContext->PSSetShaderResources(0, 1, &nullSRV);
}

void BeginFrame()
{
    RendererContext* renderer = GetRendererContext();
    renderer->deviceContext->OMSetRenderTargets(1, &renderer->screenRenderTarget.renderTargetView, renderer->screenRenderTarget.depthStencilView);
}

void EndFrame()
{
    RendererContext* renderer = GetRendererContext();
    renderer->deviceContext->OMSetRenderTargets(1, &renderer->renderTargetView, renderer->depthStencilView);
    Blit(renderer, renderer->renderTargetView, renderer->screenRenderTarget.colorSrv);
}

void ExecuteCommand_Clear(RenderCommandEntry* entry)
{
    RendererContext* renderer = GetRendererContext();

    if (entry->clear.flags & RenderClearFlags_Color)
    {
        float color[4] = { entry->clear.color.x, entry->clear.color.y, entry->clear.color.z, entry->clear.color.w };
        renderer->deviceContext->ClearRenderTargetView(renderer->screenRenderTarget.renderTargetView, color); // TODO: Clear specified target.
    }

    if (entry->clear.flags & RenderClearFlags_Depth)
    {
        renderer->deviceContext->ClearDepthStencilView(renderer->screenRenderTarget.depthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, entry->clear.depth, 0);
    }
}

void ExecuteCommand_DrawMeshImmediate(RenderCommandEntry* entry)
{
    RendererContext* renderer = GetRendererContext();

    D3D11_BUFFER_DESC vertBufferDesc = {0};
    vertBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vertBufferDesc.ByteWidth = entry->drawMeshImmediate.vertexCount * sizeof(RenderVertex);
    vertBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertBufferDesc.CPUAccessFlags = 0;
    vertBufferDesc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA vertDataDesc = {};
    vertDataDesc.pSysMem = entry->drawMeshImmediate.vertices;
    vertDataDesc.SysMemPitch = 0;
    vertDataDesc.SysMemSlicePitch = 0;

    ID3D11Buffer* vertexBuffer;
    Win32Call(renderer->device->CreateBuffer(&vertBufferDesc, &vertDataDesc, &vertexBuffer));

    D3D11_BUFFER_DESC indexBufferDesc = {0};
    indexBufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
    indexBufferDesc.ByteWidth = entry->drawMeshImmediate.indexCount * sizeof(u32);
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexBufferDesc.CPUAccessFlags = 0;
    indexBufferDesc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA indexDataDesc = {};
    indexDataDesc.pSysMem = entry->drawMeshImmediate.indices;
    indexDataDesc.SysMemPitch = 0;
    indexDataDesc.SysMemSlicePitch = 0;

    ID3D11Buffer* indexBuffer;
    Win32Call(renderer->device->CreateBuffer(&indexBufferDesc, &indexDataDesc, &indexBuffer));
    
    if (renderer->lastMaterialCommand->setMaterial.type == RenderMaterialType_Texture)
    {
        D3D11_MAPPED_SUBRESOURCE mapping;
        renderer->deviceContext->Map(renderer->quadCbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapping);
        QuadConstantBufferLayout* constants = (QuadConstantBufferLayout*)mapping.pData;
        constants->transform = *entry->drawMeshImmediate.transform;
        renderer->deviceContext->Unmap(renderer->quadCbuffer, 0);

        UINT offset = 0;
        UINT stride = sizeof(RenderVertex);

        renderer->deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        renderer->deviceContext->IASetInputLayout(renderer->quadShaderVertLayout);
        renderer->deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        renderer->deviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);

        renderer->deviceContext->RSSetState(renderer->rasterizerState);

        renderer->deviceContext->VSSetShader(renderer->quadVertexShader, nullptr, 0);
        renderer->deviceContext->VSSetConstantBuffers(0, 1, &renderer->quadCbuffer);

        renderer->deviceContext->PSSetShader(renderer->quadPixelShader, nullptr, 0);

        ID3D11ShaderResourceView* textureSRV = (ID3D11ShaderResourceView*)renderer->lastMaterialCommand->setMaterial.textureId.data1;
        ID3D11SamplerState* samplerState = (ID3D11SamplerState*)renderer->lastMaterialCommand->setMaterial.sampler.data0;

        renderer->deviceContext->PSSetShaderResources(0, 1, &textureSRV);
        renderer->deviceContext->PSSetSamplers(0, 1, &samplerState);

        renderer->deviceContext->OMSetDepthStencilState(renderer->depthStencilState, 0);
        renderer->deviceContext->OMSetBlendState(nullptr, nullptr, 0xffffffff);
    }
    else if (renderer->lastMaterialCommand->setMaterial.type == RenderMaterialType_TextSDF)
    {
        D3D11_MAPPED_SUBRESOURCE mapping;
        renderer->deviceContext->Map(renderer->sdfCbuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapping);
        TextSdfConstantBufferLayout* constants = (TextSdfConstantBufferLayout*)mapping.pData;
        constants->transform = *entry->drawMeshImmediate.transform;
        constants->params = renderer->lastMaterialCommand->setMaterial.sdfParams;
        renderer->deviceContext->Unmap(renderer->sdfCbuffer, 0);

        UINT offset = 0;
        UINT stride = sizeof(RenderVertex);

        renderer->deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        renderer->deviceContext->IASetInputLayout(renderer->sdfShaderVertLayout);
        renderer->deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
        renderer->deviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);

        renderer->deviceContext->RSSetState(renderer->rasterizerState);

        renderer->deviceContext->VSSetShader(renderer->sdfVertexShader, nullptr, 0);
        renderer->deviceContext->VSSetConstantBuffers(0, 1, &renderer->sdfCbuffer);

        renderer->deviceContext->PSSetShader(renderer->sdfPixelShader, nullptr, 0);
        renderer->deviceContext->PSSetConstantBuffers(0, 1, &renderer->sdfCbuffer);

        ID3D11ShaderResourceView* textureSRV = (ID3D11ShaderResourceView*)renderer->lastMaterialCommand->setMaterial.textureId.data1;
        ID3D11SamplerState* samplerState = (ID3D11SamplerState*)renderer->lastMaterialCommand->setMaterial.sampler.data0;

        renderer->deviceContext->PSSetShaderResources(0, 1, &textureSRV);
        renderer->deviceContext->PSSetSamplers(0, 1, &samplerState);

        renderer->deviceContext->OMSetDepthStencilState(renderer->depthStencilState, 0);
        renderer->deviceContext->OMSetBlendState(renderer->sdfBlendState, nullptr, 0xffffffff);
    }
    else
    {
        Unreachable();
    }

    renderer->deviceContext->DrawIndexed(entry->drawMeshImmediate.indexCount, 0, 0);

    vertexBuffer->Release();
    indexBuffer->Release();
}

void ExecuteCommand_SetMaterial(RenderCommandEntry* entry)
{
    RendererContext* renderer = GetRendererContext();
    renderer->lastMaterialCommand = entry;
}

void ExecuteCommandBuffer(RenderCommandBuffer* buffer)
{
    for (u32 i = 0; i < buffer->renderCommandsCount; i++)
    {
        RenderCommandEntry* entry = buffer->commands + i;
        switch(entry->command)
        {
        case RenderCommand_Clear: { ExecuteCommand_Clear(entry); } break;
        case RenderCommand_DrawMeshImmediate: { ExecuteCommand_DrawMeshImmediate(entry); } break;
        case RenderCommand_SetMaterial: { ExecuteCommand_SetMaterial(entry); } break;
        default: { Log_Error("Unknown render command!\n"); } break; // TODO: provide name via refelction.
        }
    }
}

#if !defined(META_PASS)
#include "D3D11_RendererBindings.Generated.c"
#endif
