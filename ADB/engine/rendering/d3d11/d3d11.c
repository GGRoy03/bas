#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#include "utilities.h"
#include "engine/rendering/renderer.h"

#include "mesh_vertex_shader.h"
#include "mesh_pixel_shader.h"

#define D3D11_NO_HELPERS
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_3.h>

#pragma comment (lib, "d3d11")
#pragma comment (lib, "dxgi")
#pragma comment (lib, "dxguid")
#pragma comment (lib, "d3dcompiler")

// Opaque Structure (Could remove typedef, but it helps for syntax highligthing?)
typedef struct renderer
{
    ID3D11Device           *Device;
    ID3D11DeviceContext    *DeviceContext;
    IDXGISwapChain1        *SwapChain;
    ID3D11RenderTargetView *RenderView;
    ID3D11BlendState       *BlendState;
    ID3D11SamplerState     *AtlasSamplerState;

    ID3D11InputLayout     *MeshInputLayout;
    ID3D11VertexShader    *MeshVertexShader;
    ID3D11PixelShader     *MeshPixelShader;
    ID3D11Buffer          *UniformBuffer;
    ID3D11RasterizerState *RasterState;
    ID3D11Buffer          *VtxBuffer;

    render_pass_list       PassList;
    memory_arena          *FrameArena;
} renderer;


typedef struct
{
    void *Data;
} d3d11_ui_uniform_buffer;


typedef struct
{
    void *Data;
} d3d11_mesh_uniform_buffer;

typedef struct
{
    void *Data;
} d3d11_material_uniform_buffer;


renderer *
D3D11Initialize(HWND HWindow)
{
    renderer *Result = malloc(sizeof(renderer));

    {
        memory_arena_params Params = {0};
        Params.AllocatedFromFile = __FILE__;
        Params.AllocatedFromLine = __LINE__;
        Params.CommitSize        = MiB(16);
        Params.ReserveSize       = MiB(128);

        Result->FrameArena = AllocateArena(Params);
    }

    {
        UINT CreateFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifndef NDEBUG
        CreateFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        D3D_FEATURE_LEVEL Levels[] = { D3D_FEATURE_LEVEL_11_0 };

        D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, CreateFlags, Levels, ARRAYSIZE(Levels), D3D11_SDK_VERSION, &Result->Device, 0, &Result->DeviceContext);
    }

    {
        IDXGIDevice *DXGIDevice = 0;
        Result->Device->lpVtbl->QueryInterface(Result->Device, &IID_IDXGIDevice, (void **)&DXGIDevice);

        if (DXGIDevice)
        {
            IDXGIAdapter *Adapter = 0;
            DXGIDevice->lpVtbl->GetAdapter(DXGIDevice, &Adapter);

            if (Adapter)
            {
                IDXGIFactory2 *Factory = 0;
                Adapter->lpVtbl->GetParent(Adapter, &IID_IDXGIFactory2, (void **)&Factory);

                if (Factory)
                {
                    DXGI_SWAP_CHAIN_DESC1 Desc = {0};
                    Desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
                    Desc.SampleDesc.Count = 1;
                    Desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                    Desc.BufferCount      = 2;
                    Desc.Scaling          = DXGI_SCALING_NONE;
                    Desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;

                    Factory->lpVtbl->CreateSwapChainForHwnd(Factory, (IUnknown *)Result->Device, HWindow, &Desc, 0, 0, &Result->SwapChain);
                    Factory->lpVtbl->MakeWindowAssociation(Factory, HWindow, DXGI_MWA_NO_ALT_ENTER);

                    Factory->lpVtbl->Release(Factory);
                }

                Adapter->lpVtbl->Release(Adapter);
            }

            DXGIDevice->lpVtbl->Release(DXGIDevice);
        }
    }

    {
        ID3D11Texture2D *BackBuffer = 0;
        Result->SwapChain->lpVtbl->GetBuffer(Result->SwapChain, 0, &IID_ID3D11Texture2D, (void **)&BackBuffer);

        if (BackBuffer)
        {
            Result->Device->lpVtbl->CreateRenderTargetView(Result->Device, (ID3D11Resource *)BackBuffer, 0, &Result->RenderView);

            BackBuffer->lpVtbl->Release(BackBuffer);
        }
    }

    {
        D3D11_BUFFER_DESC Desc = {0};
        Desc.ByteWidth      = KiB(64);
        Desc.Usage          = D3D11_USAGE_DYNAMIC;
        Desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        Result->Device->lpVtbl->CreateBuffer(Result->Device, &Desc, 0,&Result->VtxBuffer);
    }

    {
        Result->Device->lpVtbl->CreateVertexShader(Result->Device, MeshVertexShaderBytes , sizeof(MeshVertexShaderBytes), 0, &Result->MeshVertexShader);
        Result->Device->lpVtbl->CreatePixelShader (Result->Device, MeshPixelShaderBytes  , sizeof(MeshPixelShaderBytes) , 0, &Result->MeshPixelShader );

        D3D11_INPUT_ELEMENT_DESC InputLayout[] =
        {
            {"POS" , 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEX" , 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORM", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };

        Result->Device->lpVtbl->CreateInputLayout(Result->Device, InputLayout, ARRAYSIZE(InputLayout), MeshVertexShaderBytes, sizeof(MeshVertexShaderBytes), &Result->MeshInputLayout);
    }

    {
        D3D11_BUFFER_DESC Desc = {0};
        Desc.ByteWidth      = sizeof(d3d11_ui_uniform_buffer);
        Desc.Usage          = D3D11_USAGE_DYNAMIC;
        Desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        Result->Device->lpVtbl->CreateBuffer(Result->Device,&Desc,0, &Result->UniformBuffer);
    }

    {
        D3D11_BLEND_DESC Desc = {0};
        Desc.RenderTarget[0].BlendEnable           = TRUE;
        Desc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
        Desc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        Desc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        Desc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
        Desc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
        Desc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        Desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        Result->Device->lpVtbl->CreateBlendState(Result->Device, &Desc, &Result->BlendState);
    }

    {
        D3D11_RASTERIZER_DESC Desc = {0};
        Desc.FillMode              = D3D11_FILL_SOLID;
        Desc.CullMode              = D3D11_CULL_BACK;
        Desc.FrontCounterClockwise = FALSE;
        Desc.DepthClipEnable       = TRUE;
        Desc.ScissorEnable         = TRUE;
        Desc.MultisampleEnable     = FALSE;
        Desc.AntialiasedLineEnable = FALSE;

        Result->Device->lpVtbl->CreateRasterizerState(Result->Device, &Desc, &Result->RasterState);
    }

    return Result;
}


// ==============================================
// <Drawing> : Public
// ==============================================


void
RendererStartFrame(clear_color Color, renderer *Renderer)
{
    Renderer->DeviceContext->lpVtbl->ClearRenderTargetView(Renderer->DeviceContext, Renderer->RenderView, &Color);
}

void
RendererFlushFrame(renderer *Renderer)
{
    Renderer->SwapChain->lpVtbl->Present(Renderer->SwapChain, 0, 0);
}