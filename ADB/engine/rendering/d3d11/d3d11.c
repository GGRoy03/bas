#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define D3D11_NO_HELPERS
#include <d3d11.h>
#include <dxgi1_3.h>

#pragma comment (lib, "d3d11")
#pragma comment (lib, "dxgi")
#pragma comment (lib, "dxguid")
#pragma comment (lib, "d3dcompiler")

#include "utilities.h"
#include "platform/platform.h"
#include "engine/rendering/renderer.h"
#include "engine/rendering/assets.h"

#include "mesh_vertex_shader.h"
#include "mesh_pixel_shader.h"

#define MAX_MATERIAL_COUNT 64
#define MAX_STATIC_MESH_COUNT 128


typedef struct
{
    ID3D11Device           *Device;
    ID3D11DeviceContext    *DeviceContext;
    IDXGISwapChain1        *SwapChain;
    ID3D11RenderTargetView *RenderView;

    // Mesh Objects

    ID3D11InputLayout     *MeshInputLayout;
    ID3D11VertexShader    *MeshVertexShader;
    ID3D11PixelShader     *MeshPixelShader;
    ID3D11SamplerState    *MeshSamplerState;
    ID3D11RasterizerState *MeshRasterizerState;
    ID3D11Buffer          *MeshTransformUniformBuffer;
} d3d11_renderer;


d3d11_renderer *
D3D11Initialize(HWND HWindow, memory_arena *Arena)
{
    d3d11_renderer *Result = PushStruct(Arena, d3d11_renderer);
    memset(Result, 0, sizeof(renderer));

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
        Result->Device->lpVtbl->CreateVertexShader(Result->Device, MeshVertexShaderBytes , sizeof(MeshVertexShaderBytes), 0, &Result->MeshVertexShader);
        Result->Device->lpVtbl->CreatePixelShader (Result->Device, MeshPixelShaderBytes  , sizeof(MeshPixelShaderBytes) , 0, &Result->MeshPixelShader );

        D3D11_INPUT_ELEMENT_DESC InputLayout[] =
        {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT   , 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL"  , 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };

        Result->Device->lpVtbl->CreateInputLayout(Result->Device, InputLayout, ARRAYSIZE(InputLayout), MeshVertexShaderBytes, sizeof(MeshVertexShaderBytes), &Result->MeshInputLayout);
    }

    {
        D3D11_BUFFER_DESC Desc = {0};
        Desc.ByteWidth      = sizeof(mesh_transform_uniform_buffer);
        Desc.Usage          = D3D11_USAGE_DYNAMIC;
        Desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        Result->Device->lpVtbl->CreateBuffer(Result->Device,&Desc,0, &Result->MeshTransformUniformBuffer);
    }

    {
        D3D11_SAMPLER_DESC Desc = {0};
        Desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        Desc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
        Desc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
        Desc.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
        Desc.MaxAnisotropy  = 1;
        Desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        Desc.MaxLOD         = D3D11_FLOAT32_MAX;

        Result->Device->lpVtbl->CreateSamplerState(Result->Device, &Desc, &Result->MeshSamplerState);
    }

    {
        D3D11_RASTERIZER_DESC Desc = {0};
        Desc.FillMode              = D3D11_FILL_SOLID;
        Desc.CullMode              = D3D11_CULL_BACK;
        Desc.FrontCounterClockwise = TRUE;
        Desc.DepthClipEnable       = FALSE;
        Desc.ScissorEnable         = FALSE;
        Desc.MultisampleEnable     = FALSE;
        Desc.AntialiasedLineEnable = FALSE;

        Result->Device->lpVtbl->CreateRasterizerState(Result->Device, &Desc, &Result->MeshRasterizerState);
    }

    return Result;
}


// ==============================================
// <Resources>
// ==============================================


void *
RendererCreateVertexBuffer(void *Data, uint64_t Size, renderer *Renderer)
{
    ID3D11Buffer *Result = 0;

    if (Data && Size && Renderer)
    {
        d3d11_renderer *D3D11 = (d3d11_renderer *)Renderer->Backend;
        ID3D11Device   *Device = D3D11->Device;

        D3D11_BUFFER_DESC Desc =
        {
            .ByteWidth           = Size,
            .Usage               = D3D11_USAGE_DEFAULT,
            .BindFlags           = D3D11_BIND_VERTEX_BUFFER,
            .CPUAccessFlags      = 0,
            .MiscFlags           = 0,
            .StructureByteStride = 0,
        };

        D3D11_SUBRESOURCE_DATA InitialData =
        {
            .pSysMem          = Data,
            .SysMemPitch      = 0,
            .SysMemSlicePitch = 0,
        };

        Device->lpVtbl->CreateBuffer(Device, &Desc, &InitialData, &Result);
    }

    return Result;
}

void *
RendererCreateTexture(loaded_texture LoadedTexture, renderer *Renderer)
{
    ID3D11ShaderResourceView *Result = 0;

    // The BytesPerPixel check is artificial and should be removed.

    if (LoadedTexture.Data && LoadedTexture.Width && LoadedTexture.Height && LoadedTexture.BytesPerPixel == 4)
    {
        d3d11_renderer *D3D11 = (d3d11_renderer *)Renderer->Backend;
        ID3D11Device   *Device = D3D11->Device;

        D3D11_TEXTURE2D_DESC TextureDesc =
        {
            .Width              = LoadedTexture.Width,
            .Height             = LoadedTexture.Height,
            .MipLevels          = 1,
            .ArraySize          = 1,
            .Format             = DXGI_FORMAT_R8G8B8A8_UNORM,
            .SampleDesc.Count   = 1,
            .SampleDesc.Quality = 0,
            .Usage              = D3D11_USAGE_IMMUTABLE,
            .BindFlags          = D3D11_BIND_SHADER_RESOURCE,
            .CPUAccessFlags     = 0,
            .MiscFlags          = 0,
        };
        
        D3D11_SUBRESOURCE_DATA InitialData =
        {
            .pSysMem     = LoadedTexture.Data,
            .SysMemPitch = LoadedTexture.Width * LoadedTexture.BytesPerPixel,
        };
        
        ID3D11Texture2D *Texture = 0;
        Device->lpVtbl->CreateTexture2D(Device, &TextureDesc, &InitialData, &Texture);
        if (Texture)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC TextureViewDesc =
            {
                .Format                    = TextureDesc.Format,
                .ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D,
                .Texture2D.MostDetailedMip = 0,
                .Texture2D.MipLevels       = 1,
            };
        
            Device->lpVtbl->CreateShaderResourceView(Device, (ID3D11Resource *)Texture, &TextureViewDesc, &Result);

            Texture->lpVtbl->Release(Texture);
        }
    }

    return Result;
}


// ==============================================
// <Drawing>
// ==============================================


void
RendererStartFrame(clear_color Color, renderer *Renderer)
{
    d3d11_renderer         *D3D11      =  (d3d11_renderer *)Renderer->Backend;
    ID3D11DeviceContext    *Context    = D3D11->DeviceContext;
    ID3D11RenderTargetView *RenderView = D3D11->RenderView;

    Context->lpVtbl->ClearRenderTargetView(Context, RenderView, (FLOAT *)&Color);
}

// Obviously this is a super hardcoded implementation and we would rely on some sort of batcher. I just want to get something on screen.
// Then we will clean up all of this code and augment it.

void
RendererDrawFrame(int Width, int Height, engine_memory *EngineMemory, renderer *Renderer)
{
    d3d11_renderer      *D3D11   = (d3d11_renderer *)Renderer->Backend;
    ID3D11DeviceContext *Context = D3D11->DeviceContext;

    {
        mesh_transform_uniform_buffer TransformBuffer =
        {
            .World      = GetCameraWorldMatrix(&Renderer->Camera),
            .View       = GetCameraViewMatrix(&Renderer->Camera),
            .Projection = GetCameraProjectionMatrix(&Renderer->Camera),
        };

        D3D11_MAPPED_SUBRESOURCE Mapped;
        Context->lpVtbl->Map(Context, (ID3D11Resource *)D3D11->MeshTransformUniformBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Mapped);
        if (Mapped.pData)
        {
            memcpy(Mapped.pData, &TransformBuffer, sizeof(TransformBuffer));
            Context->lpVtbl->Unmap(Context, (ID3D11Resource *)D3D11->MeshTransformUniformBuffer, 0);
        }
    }

    D3D11_VIEWPORT Viewport = { 0.f, 0.f, Width, Height, 0.f, 1.f };
    Context->lpVtbl->RSSetState(Context, D3D11->MeshRasterizerState);
    Context->lpVtbl->RSSetViewports(Context, 1, &Viewport);

    Context->lpVtbl->IASetPrimitiveTopology(Context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Context->lpVtbl->IASetInputLayout(Context, D3D11->MeshInputLayout);

    Context->lpVtbl->VSSetConstantBuffers(Context, 0, 1, &D3D11->MeshTransformUniformBuffer);
    Context->lpVtbl->VSSetShader(Context, D3D11->MeshVertexShader, 0, 0);
    Context->lpVtbl->PSSetShader(Context, D3D11->MeshPixelShader , 0, 0);

    Context->lpVtbl->OMSetRenderTargets(Context, 1, &D3D11->RenderView, 0);
    // Context->lpVtbl->OMSetBlendState(Context, Renderer->BlendState, 0, 0xFFFFFFFF);

    static_mesh_list MeshList = RendererGetAllStaticMeshes(EngineMemory, Renderer);

    for (uint32_t Idx = 0; Idx < MeshList.Count; ++Idx)
    {
        renderer_static_mesh *StaticMesh = MeshList.Data[Idx];

        renderer_backend_resource *BD = AccessUnderlyingResource(StaticMesh->VertexBuffer, Renderer->Resources);

        ID3D11Buffer *VertexBuffer = (ID3D11Buffer *)BD->Data;
        UINT32        Stride       = sizeof(mesh_vertex_data);
        UINT32        Offset       = 0;
        Context->lpVtbl->IASetVertexBuffers(Context, 0, 1, &VertexBuffer, &Stride, &Offset);

        for (uint32_t SubmeshIdx = 0; SubmeshIdx < StaticMesh->SubmeshCount; ++SubmeshIdx)
        {
            renderer_static_submesh *Submesh = StaticMesh->Submeshes + SubmeshIdx;

            ID3D11ShaderResourceView *MaterialView[MaterialMap_Count] = {0};
            {
                renderer_material *Material = AccessUnderlyingResource(Submesh->Material, Renderer->Resources);

                // TODO: Fix these queries

                renderer_backend_resource *BD1 = AccessUnderlyingResource(Material->Maps[MaterialMap_Color], Renderer->Resources);

                MaterialView[MaterialMap_Color] = BD1->Data;
                // MaterialView[MaterialMap_Normal]    = Material->Maps[MaterialMap_Normal]    ? Material->Maps[MaterialMap_Normal]->Backend    : 0;
                // MaterialView[MaterialMap_Roughness] = Material->Maps[MaterialMap_Roughness] ? Material->Maps[MaterialMap_Roughness]->Backend : 0;
            }

            Context->lpVtbl->PSSetSamplers(Context, 0, 1, &D3D11->MeshSamplerState);
            Context->lpVtbl->PSSetShaderResources(Context, 0, 1, MaterialView);

            Context->lpVtbl->Draw(Context, Submesh->VertexCount, Submesh->VertexStart);
        }
    }
}

void
RendererFlushFrame(renderer *Renderer)
{
    d3d11_renderer  *D3D11     = (d3d11_renderer *)Renderer->Backend;
    IDXGISwapChain1 *SwapChain = D3D11->SwapChain;

    SwapChain->lpVtbl->Present(SwapChain, 0, 0);
}