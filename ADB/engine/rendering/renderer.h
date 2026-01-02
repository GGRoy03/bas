#pragma once


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
// <Drawing> : Public
// ==============================================


typedef struct renderer renderer;


typedef struct
{
    float R, G, B, A;
} clear_color;


void RendererStartFrame  (clear_color Color, renderer *Renderer);
void RendererFlushFrame  (renderer *Renderer);