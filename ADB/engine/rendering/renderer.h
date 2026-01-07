#pragma once

#include "assets.h"
#include "engine/math/matrix.h"
#include <engine/math/vector.h>

typedef struct renderer renderer;


typedef enum
{
    RenderPass_None = 0,
    RenderPass_UI   = 1,
    RenderPass_Mesh = 2,
} RenderPassType;


// Data Batches


typedef struct
{
    uint8_t *Memory;
    uint64_t  ByteCount;
    uint64_t  ByteCapacity;
} render_batch;


typedef struct render_batch_node render_batch_node;
struct render_batch_node
{
    render_batch_node *Next;
    render_batch       Value;
};


typedef struct
{
    render_batch_node *First;
    render_batch_node *Last;
    uint64_t           BatchCount;
    uint64_t           ByteCount;
    uint64_t           BytesPerInstance;
} render_batch_list;


// Maps to constant buffers.

typedef struct
{
    void *Data;
} ui_group_params;


typedef struct ui_group_node ui_group_node;
struct ui_group_node
{
    ui_group_node    *Next;
    render_batch_list BatchList;
    ui_group_params   Params;
};


// Render Passes


typedef struct
{
    ui_group_node *First;
    ui_group_node *Last;
    uint32_t       Count;
} render_pass_params_ui;


typedef struct
{
    RenderPassType Type;
    union
    {
        render_pass_params_ui UI;
    } Params;
} render_pass;


typedef struct render_pass_node render_pass_node;
struct render_pass_node
{
    render_pass_node *Next;
    render_pass       Value;
};


typedef struct
{
    render_pass_node *First;
    render_pass_node *Last;
} render_pass_list;


render_pass * GetRenderPass        (memory_arena *Arena, RenderPassType Type, render_pass_list *PassList);
void *        PushDataInBatchList  (memory_arena *Arena, render_batch_list *BatchList);

// ==============================================
// <Resources> 
// ==============================================

// TODO: Hide most of this code and cleanup the public API.

typedef struct renderer_resource renderer_resource;


#define MAX_RENDERER_RESOURCE 128
#define MAX_SUBMESH_COUNT     8


typedef enum
{
    RendererResource_None = 0,

    // Base

    RendererResource_Texture2D    = 1,
    RendererResource_TextureView  = 2,
    RendererResource_VertexBuffer = 3,

    // Composite

    RendererResource_Material     = 4,
    RendererResource_StaticMesh   = 5,

    RendererResource_Count = 6,
} RendererResource_Type;


typedef struct
{
    uint32_t              Value;
    RendererResource_Type Type;
} resource_handle;


typedef struct
{
    void *Data;
} renderer_backend_resource;


typedef struct
{
    resource_handle Maps[MaterialMap_Count];
} renderer_material;


typedef struct
{
    uint64_t        VertexCount;
    uint64_t        VertexStart;
    resource_handle Material;
} renderer_static_submesh;


typedef struct
{
    resource_handle         VertexBuffer;
    uint64_t                VertexBufferSize;
    renderer_static_submesh Submeshes[MAX_SUBMESH_COUNT];
    uint32_t                SubmeshCount;
} renderer_static_mesh;


typedef struct renderer_resource
{
    RendererResource_Type Type;
    uint32_t             _RefCount;
    uint32_t              NextFree;
    uint32_t              NextSameType;
    uint64_t              UUID;

    // This is such a waste of space... Umph.. It is simpler.

    union
    {
        renderer_backend_resource Backend;
        renderer_material         Material;
        renderer_static_mesh      StaticMesh;
    };
} renderer_resource;


typedef struct
{
    // Resources

    renderer_resource Resources[MAX_RENDERER_RESOURCE];
    uint32_t          FirstFree;
    uint32_t          FirstByType[RendererResource_Count];
    uint32_t          CountByType[RendererResource_Count];
} renderer_resource_manager;


typedef struct
{
    mat4x4 World;
    mat4x4 View;
    mat4x4 Projection;
} mesh_transform_uniform_buffer;


typedef struct
{
    renderer_static_mesh **Data;
    uint32_t               Count;
} static_mesh_list;


renderer_resource_manager CreateResourceManager  (void);


void             CreateStaticMesh            (asset_file_data AssetFile, renderer *Renderer);
static_mesh_list RendererGetAllStaticMeshes  (engine_memory *EngineMemory, renderer *Renderer);


// 
// Backend specific functions that must be implemented by every backend
//


void * RendererCreateTexture       (loaded_texture Texture, renderer *Renderer);
void * RendererCreateVertexBuffer  (void *Data, uint64_t Size, renderer *Renderer);

// ==============================================
// <Camera>
// ==============================================


typedef struct
{
    vec3 Position;
    vec3 Forward;
    vec3 Up;

    float FovY;
    float AspectRatio;
    float NearPlane;
    float FarPlane;
} camera;

camera CreateCamera               (vec3 Position, float FovY, float AspectRatio);

mat4x4 GetCameraWorldMatrix       (camera *Camera);
mat4x4 GetCameraViewMatrix        (camera *Camera);
mat4x4 GetCameraProjectionMatrix  (camera *Camera);


// ==============================================
// <Scenes>
// ==============================================


typedef struct
{
    camera Camera;
} game_scene;


// ==============================================
// <Drawing>
// ==============================================


typedef struct
{
    float R, G, B, A;
} clear_color;


void RendererStartFrame  (clear_color Color, renderer *Renderer);
void RendererDrawFrame   (int Width, int Height, engine_memory *EngineMemory, renderer *Renderer);
void RendererFlushFrame  (renderer *Renderer);

// ==============================================
// <...> 
// ==============================================

typedef struct renderer
{
    void                     *Backend;
    render_pass_list          PassList;
    memory_arena             *FrameArena;
    renderer_resource_manager Resources;
    camera                    Camera;
} renderer;