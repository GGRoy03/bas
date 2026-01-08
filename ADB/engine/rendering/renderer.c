#include <stdint.h>
#include <assert.h>
#include <math.h>

#include "third_party/stb_image.h"

#include "utilities.h"         // Arenas
#include "platform/platform.h" // Engine Memory
#include "renderer.h"          // Implementation File


void *
PushDataInBatchList(memory_arena *Arena, render_batch_list *BatchList)
{
    void *Result = NULL;

    render_batch_node *Node = BatchList->Last;
    if (!Node || (Node->Value.ByteCount + BatchList->BytesPerInstance > Node->Value.ByteCapacity))
    {
        Node = PushStruct(Arena, render_batch_node);
        if (Node)
        {
            Node->Next = NULL;
            Node->Value.ByteCount    = 0;
            Node->Value.ByteCapacity = KiB(64); // Still an issue!
            Node->Value.Memory       = PushArray(Arena, uint8_t, Node->Value.ByteCapacity);

            if (!BatchList->First)
            {
                BatchList->First = Node;
                BatchList->Last  = Node;
            }
            else if (BatchList->Last)
            {
                BatchList->Last->Next = Node;
                BatchList->Last       = Node;
            }
            else
            {
                assert(!"INVALID ENGINE STATE");
            }
        }
    }

    BatchList->ByteCount += BatchList->BytesPerInstance;
    BatchList->BatchCount += 1;

    if (Node)
    {
        Result = (void *)(Node->Value.Memory + Node->Value.ByteCount);
        Node->Value.ByteCount += BatchList->BytesPerInstance;
    }

    return Result;
}

render_pass *
GetRenderPass(memory_arena *Arena, RenderPassType Type, render_pass_list *PassList)
{
    render_pass_node *Result = PassList->Last;

    if (!Result || Result->Value.Type != Type)
    {
        Result = PushStruct(Arena, render_pass_node);
        if (!Result)
        {
            return NULL;
        }

        Result->Value.Type = Type;
        Result->Value.Params.UI.First = NULL;
        Result->Value.Params.UI.Last = NULL;
        Result->Value.Params.UI.Count = 0;
        Result->Next = NULL;

        if (!PassList->First)
        {
            PassList->First = Result;
        }

        if (PassList->Last)
        {
            PassList->Last->Next = Result;
        }

        PassList->Last = Result;
    }

    return &Result->Value;
}


// ==============================================
// <Resources> 
// ==============================================


#define MAX_RENDERER_RESOURCE 128


#define INVALID_LINK_SENTINEL   0xFFFFFFFF
#define INVALID_RESOURCE_ENTRY  0xFFFFFFFF
#define INVALID_RESOURCE_HANDLE 0xFFFFFFFF


typedef struct
{
    uint64_t Value;
} resource_uuid;


typedef struct
{
    resource_uuid   UUID;
    resource_handle Handle;
    uint32_t        NextSameHash;
} resource_reference_entry;


typedef struct resource_reference_table
{
    uint32_t                 HashMask;
    uint32_t                 HashCount;
    uint32_t                 EntryCount;

    uint32_t                 HashTable[32];
    resource_reference_entry Entries[MAX_RENDERER_RESOURCE];
    uint32_t                 FirstFreeEntry;
} resource_reference_table;


typedef struct
{
    uint32_t        Id;
    resource_handle Handle;
} resource_reference_state;



typedef struct renderer_resource
{
    RendererResource_Type Type;
    uint32_t              RefCount;
    uint32_t              NextFree;
    uint32_t              NextSameType;
    resource_uuid         UUID;

    union
    {
        renderer_backend_resource Backend;
        renderer_material         Material;
        renderer_static_mesh      StaticMesh;
    };
} renderer_resource;


typedef struct renderer_resource_manager
{
    // Resources

    renderer_resource        Resources[MAX_RENDERER_RESOURCE];
    uint32_t                 FirstFree;
    uint32_t                 FirstByType[RendererResource_Count];
    uint32_t                 CountByType[RendererResource_Count];
} renderer_resource_manager;



static resource_uuid
MakeResourceUUID(byte_string PathToResource)
{
    resource_uuid Result = {.Value = HashByteString(PathToResource)};
    return Result;
}


static resource_reference_entry *
GetEntry(uint32_t Index, resource_reference_table *Table)
{
    assert(Index < Table->EntryCount);

    resource_reference_entry *Result = Table->Entries + Index;
    return Result;
}


static uint32_t *
GetSlotPointer(resource_uuid UUID, resource_reference_table *Table)
{
    uint64_t HashIndex = UUID.Value;
    uint32_t HashSlot  = (HashIndex & Table->HashMask);

    assert(HashSlot < Table->HashCount);
    uint32_t *Result = &Table->HashTable[HashSlot];

    return Result;
}


static bool
ResourceUUIDAreEqual(resource_uuid A, resource_uuid B)
{
    bool Result = A.Value == B.Value;
    return Result;
}


static uint32_t
PopFreeEntry(resource_reference_table *Table)
{
    uint32_t Result = Table->FirstFreeEntry;

    if (Result != INVALID_RESOURCE_ENTRY)
    {
        resource_reference_entry *Entry = GetEntry(Result, Table);
        assert(Entry);

        Table->FirstFreeEntry = Entry->NextSameHash;
        Entry->NextSameHash   = INVALID_RESOURCE_ENTRY;

    }

    return Result;
}


static resource_reference_state
FindResourceByUUID(resource_uuid UUID, resource_reference_table *Table)
{
    resource_reference_entry *Result = 0;

    uint32_t *Slot       = GetSlotPointer(UUID, Table);
    uint32_t  EntryIndex = *Slot;
    while (EntryIndex != INVALID_RESOURCE_ENTRY)
    {
        resource_reference_entry *Entry = GetEntry(EntryIndex, Table);
        if (ResourceUUIDAreEqual(Entry->UUID, UUID))
        {
            Result = Entry;
            break;
        }

        EntryIndex = Entry->NextSameHash;
    }

    if (!Result)
    {
        uint32_t Free = PopFreeEntry(Table);
        if (Free != INVALID_RESOURCE_ENTRY)
        {
            resource_reference_entry *Entry = GetEntry(Free, Table);
            Entry->UUID         = UUID;
            Entry->NextSameHash = *Slot;

            *Slot = Free;

            EntryIndex = Free;
            Result     = Entry;
        }
    }

    resource_reference_state State =
    {
        .Id     = EntryIndex,
        .Handle = Result ? Result->Handle : (resource_handle){.Value = INVALID_RESOURCE_HANDLE, .Type = RendererResource_None}, 
    };

    return State;
}

// Maybe expose this with params?? Same params as the resource_manager basically.

resource_reference_table *
CreateResourceReferenceTable(memory_arena *Arena)
{
    resource_reference_table *Table = PushStruct(Arena, resource_reference_table);

    if (Table)
    {
        Table->HashCount      = 32;
        Table->HashMask       = 32 - 1;
        Table->EntryCount     = MAX_RENDERER_RESOURCE;
        Table->FirstFreeEntry = 0;

        for (uint32_t HashSlot = 0; HashSlot < Table->HashCount; ++HashSlot)
        {
            Table->HashTable[HashSlot] = INVALID_RESOURCE_ENTRY;
        }

        for (uint32_t EntryIdx = 0; EntryIdx < Table->EntryCount; ++EntryIdx)
        {
            Table->Entries[EntryIdx].Handle       = (resource_handle){.Type = RendererResource_None, .Value = INVALID_RESOURCE_HANDLE};
            Table->Entries[EntryIdx].NextSameHash = EntryIdx < Table->EntryCount - 1 ? EntryIdx + 1: INVALID_RESOURCE_ENTRY;
            Table->Entries[EntryIdx].UUID         = (resource_uuid){.Value = 0};
        }
    }

    return Table;
}


static void
UpdateResourceReferenceTable(uint32_t Id, resource_handle Handle, resource_reference_table *Table)
{
    resource_reference_entry *Entry = GetEntry(Id, Table);
    assert(Entry);

    Entry->Handle = Handle;
}


static renderer_resource *
GetRendererResource(uint32_t Id, renderer_resource_manager *ResourceManager)
{
    assert(Id < MAX_RENDERER_RESOURCE);

    renderer_resource *Result = ResourceManager->Resources + Id;
    return Result;
}


// This function looks wrong for the current flow... It's a bit messy.

static resource_handle
CreateResourceHandle(resource_uuid UUID, RendererResource_Type Type, renderer_resource_manager *ResourceManager)
{
    resource_handle Result = {0};

    if (Type != RendererResource_None && ResourceManager)
    {
        renderer_resource *Resource = ResourceManager->Resources + ResourceManager->FirstFree;
        Resource->RefCount     = 0;
        Resource->Type         = Type;
        Resource->UUID         = UUID;
        Resource->NextSameType = ResourceManager->FirstByType[Type];

        Result.Value = ResourceManager->FirstFree;
        Result.Type   = Type;

        ResourceManager->CountByType[Type] += 1;
        ResourceManager->FirstByType[Type]  = ResourceManager->FirstFree;
        ResourceManager->FirstFree          = Resource->NextFree;
        Resource->NextFree                  = INVALID_LINK_SENTINEL;
    }

    return Result;
}


static bool
IsValidResourceHandle(resource_handle Handle)
{
    bool Result = Handle.Value != INVALID_RESOURCE_HANDLE && Handle.Type != RendererResource_None;
    return Result;
}


static resource_handle
BindResourceHandle(resource_handle Handle, renderer_resource_manager *ResourceManager)
{
    if (IsValidResourceHandle(Handle))
    {
        renderer_resource *Resource = GetRendererResource(Handle.Value, ResourceManager);
        assert(Resource);

        ++Resource->RefCount;
    }

    return Handle;
}


static resource_handle
UnbindResourceHandle(resource_handle Handle, renderer_resource_manager *ResourceManager)
{
    if (IsValidResourceHandle(Handle))
    {
        renderer_resource *Resource = GetRendererResource(Handle.Value, ResourceManager);
        assert(Resource);
        assert(Resource->RefCount > 0);

        --Resource->RefCount;

        if (Resource->RefCount == 0)
        {
            // TODO: Free the resource...
        }
    }

    resource_handle InvalidHandle =
    {
        .Type  = RendererResource_None,
        .Value = INVALID_RESOURCE_HANDLE,
    };

    return InvalidHandle;
}


void *
AccessUnderlyingResource(resource_handle Handle, renderer_resource_manager *ResourceManager)
{
    void *Result = 0;

    if (IsValidResourceHandle(Handle))
    {
        renderer_resource *Resource = GetRendererResource(Handle.Value, ResourceManager);
        assert(Resource);

        switch (Handle.Type)
        {

        case RendererResource_Texture2D:
        case RendererResource_TextureView:
        case RendererResource_VertexBuffer:
        {
            Result = &Resource->Backend;
        } break;

        case RendererResource_Material:
        {
            Result = &Resource->Material;
        } break;

        case RendererResource_StaticMesh:
        {
            Result = &Resource->StaticMesh;
        } break;

        default:
        {
            assert(!"INVALID ENGINE STATE");
        } break;
           
        }
    }

    return Result;
}


renderer_resource_manager *
CreateResourceManager(memory_arena *Arena)
{
    renderer_resource_manager *ResourceManager = PushStruct(Arena, renderer_resource_manager);

    if (ResourceManager)
    {
        uint32_t ResourceCount = ArrayCount(ResourceManager->Resources);

        for (uint32_t ResourceIdx = 0; ResourceIdx < ResourceCount; ++ResourceIdx)
        {
            renderer_resource *Resource = ResourceManager->Resources + ResourceIdx;

            Resource->Type = RendererResource_None;
            Resource->RefCount = 0;

            if (ResourceIdx < ResourceCount - 1)
            {
                Resource->NextFree     = ResourceIdx + 1;
                Resource->NextSameType = INVALID_LINK_SENTINEL;
            }
            else
            {
                Resource->NextFree     = INVALID_LINK_SENTINEL;
                Resource->NextSameType = INVALID_LINK_SENTINEL;
            }
        }

        ResourceManager->FirstFree = 0;

        for (uint32_t ResourceType = RendererResource_Texture2D; ResourceType < RendererResource_Count; ++ResourceType)
        {
            ResourceManager->FirstByType[ResourceType] = INVALID_LINK_SENTINEL;
        }
    }
    
    return ResourceManager;
}


void
LoadAssetFileData(asset_file_data AssetFile, memory_arena *Arena, renderer *Renderer)
{
    for (uint32_t MaterialIdx = 0; MaterialIdx < AssetFile.MaterialCount; ++MaterialIdx)
    {
        resource_uuid            MaterialUUID  = MakeResourceUUID(AssetFile.Materials[MaterialIdx].Path);
        resource_reference_state MaterialState = FindResourceByUUID(MaterialUUID, Renderer->ReferenceTable);

        if (!IsValidResourceHandle(MaterialState.Handle))
        {
            resource_handle    MaterialHandle = CreateResourceHandle(MaterialUUID, RendererResource_Material, Renderer->Resources);
            renderer_material *Material       = AccessUnderlyingResource(MaterialHandle, Renderer->Resources);

            if (Material)
            {
                for (MaterialMap_Type MapType = MaterialMap_Color; MapType < MaterialMap_Count; ++MapType)
                {
                    resource_uuid            TextureUUID  = MakeResourceUUID(AssetFile.Materials[MaterialIdx].Textures[MapType].Path);
                    resource_reference_state TextureState = FindResourceByUUID(TextureUUID, Renderer->ReferenceTable);

                    if (!IsValidResourceHandle(TextureState.Handle))
                    {
                        resource_handle            TextureHandle   = CreateResourceHandle(TextureUUID, RendererResource_TextureView, Renderer->Resources);
                        renderer_backend_resource *BackendResource = AccessUnderlyingResource(TextureHandle, Renderer->Resources);

                        if (BackendResource)
                        {
                            BackendResource->Data = RendererCreateTexture(AssetFile.Materials[MaterialIdx].Textures[MapType], Renderer);
                        }

                        Material->Maps[MapType] = BindResourceHandle(TextureHandle, Renderer->Resources);
                    }
                    else
                    {
                        assert(!"How do we handle such a case?");
                    }

                    stbi_image_free(AssetFile.Materials[MaterialIdx].Textures[MapType].Data);
                }
            }
            else
            {
                assert(!"How do we handle such a case?");
            }

            UpdateResourceReferenceTable(MaterialState.Id, MaterialHandle, Renderer->ReferenceTable);
        }
        else
        {
            assert(!"How do we handle such a case?");
        }       
    }

    for (uint32_t MeshIdx = 0; MeshIdx < AssetFile.MeshCount; ++MeshIdx)
    {
        asset_mesh_data *MeshData = &AssetFile.Meshes[MeshIdx];

        // Since multiple meshes can come from the same file we have to modify their resource names as:
        // {Path}/{Name}
        // For example: data/strawberry.obj and a mesh strawberry => data/strawberry:strawberry

        byte_string PathWithNoExtension  = StripExtensionName(MeshData->Path);
        uint64_t    MeshResourceNameSize = PathWithNoExtension.Size + MeshData->Name.Size + 2;
        byte_string MeshResourceName     = ByteString(PushArray(Arena, uint8_t, MeshResourceNameSize), 0);

        for (uint32_t Idx = 0; Idx < PathWithNoExtension.Size; ++Idx)
        {
            MeshResourceName.Data[MeshResourceName.Size++] = PathWithNoExtension.Data[Idx];
        }

        MeshResourceName.Data[MeshResourceName.Size++] = ':';
        MeshResourceName.Data[MeshResourceName.Size++] = ':';
        
        for (uint32_t Idx = 0; Idx < MeshData->Name.Size; ++Idx)
        {
            MeshResourceName.Data[MeshResourceName.Size++] = MeshData->Name.Data[Idx];
        }

        resource_uuid            MeshUUID  = MakeResourceUUID(MeshResourceName);
        resource_reference_state MeshState = FindResourceByUUID(MeshUUID, Renderer->ReferenceTable);
        
        if (!IsValidResourceHandle(MeshState.Handle))
        {
            resource_handle       MeshHandle = CreateResourceHandle(MeshUUID, RendererResource_StaticMesh, Renderer->Resources);
            renderer_static_mesh *StaticMesh = AccessUnderlyingResource(MeshHandle, Renderer->Resources);
        
            resource_uuid            VertexBufferUUID  = MakeResourceUUID(ByteStringLiteral("This is a buffer"));
            resource_reference_state VertexBufferState = FindResourceByUUID(VertexBufferUUID, Renderer->ReferenceTable);
        
            if (!IsValidResourceHandle(VertexBufferState.Handle))
            {
                resource_handle            VertexBufferHandle = CreateResourceHandle(VertexBufferUUID, RendererResource_VertexBuffer, Renderer->Resources);
                renderer_backend_resource *VertexBuffer       = AccessUnderlyingResource(VertexBufferHandle, Renderer->Resources);
                uint64_t                   VertexBufferSize   = AssetFile.VertexCount * sizeof(mesh_vertex_data);
        
                if (VertexBuffer)
                {
                    VertexBuffer->Data = RendererCreateVertexBuffer(AssetFile.Vertices, VertexBufferSize, Renderer);
                }
        
                StaticMesh->VertexBuffer     = BindResourceHandle(VertexBufferHandle, Renderer->Resources);
                StaticMesh->VertexBufferSize = VertexBufferSize;
        
                UpdateResourceReferenceTable(VertexBufferState.Id, VertexBufferHandle, Renderer->ReferenceTable);
            }
        
            assert(MeshData->SubmeshCount < MAX_SUBMESH_COUNT);
        
            StaticMesh->SubmeshCount = MeshData->SubmeshCount;
        
            for (uint32_t SubmeshIdx = 0; SubmeshIdx < MeshData->SubmeshCount; ++SubmeshIdx)
            {
                asset_submesh_data *SubmeshData = &MeshData->Submeshes[SubmeshIdx];
        
                resource_uuid            MaterialUUID  = MakeResourceUUID(SubmeshData->MaterialPath);
                resource_reference_state MaterialState = FindResourceByUUID(MaterialUUID, Renderer->ReferenceTable);
        
                StaticMesh->Submeshes[SubmeshIdx].Material    = BindResourceHandle(MaterialState.Handle, Renderer->Resources);
                StaticMesh->Submeshes[SubmeshIdx].VertexCount = SubmeshData->VertexCount;
                StaticMesh->Submeshes[SubmeshIdx].VertexStart = SubmeshData->VertexOffset;
            }
        
            UpdateResourceReferenceTable(MeshState.Id, MeshHandle, Renderer->ReferenceTable);
        }
        else
        {
            assert(!"How do we handle such a case?");
        }
    }
}


// This just works for any type now...

static_mesh_list
RendererGetAllStaticMeshes(engine_memory *EngineMemory, renderer *Renderer)
{
    static_mesh_list Result = {0};

    if (EngineMemory && Renderer)
    {
        renderer_resource_manager *ResourceManager = Renderer->Resources;

        uint32_t Count = ResourceManager->CountByType[RendererResource_StaticMesh];
        uint32_t First = ResourceManager->FirstByType[RendererResource_StaticMesh];
        
        renderer_static_mesh **List = PushArray(EngineMemory->FrameMemory, renderer_static_mesh *, Count);
        if (List)
        {
            uint32_t Added = 0;
            while (First != INVALID_LINK_SENTINEL)
            {
                renderer_resource *Resource = &ResourceManager->Resources[First];
        
                List[Added++] = &Resource->StaticMesh;
                First         =  Resource->NextSameType;
            }

            Result.Data  = List;
            Result.Count = Count;
        
            assert(Added == Count);
        }
    }

    return Result;
}


// ==============================================
// <Camera>
// ==============================================


camera
CreateCamera(vec3 Position, float FovY, float AspectRatio)
{
    camera Result =
    {
        .Position    = Position,
        .Forward     = Vec3(0.f, 0.f, 1.f),
        .Up          = Vec3(0.f, 1.f, 0.f),
        .AspectRatio = AspectRatio,
        .NearPlane   = 0.1f,
        .FarPlane    = 100.f,
        .FovY        = FovY,
    };

    return Result;
}


mat4x4
GetCameraWorldMatrix(camera *Camera)
{
    (void)Camera;

    mat4x4 World = {0};
    World.c0r0 = 1.f;
    World.c1r1 = 1.f;
    World.c2r2 = 1.f;
    World.c3r3 = 1.f;

    return World;
}


mat4x4
GetCameraViewMatrix(camera *Camera)
{
    mat4x4 View = {0};

    vec3 Right = Vec3Normalize(Vec3Cross(Camera->Up, Camera->Forward));
    vec3 Up    = Vec3Cross(Camera->Forward, Right);

    Camera->Up = Up;

    View.c0r0 = Right.X; View.c0r1 = Camera->Up.X; View.c0r2 = Camera->Forward.X; View.c0r3 = 0.f;
    View.c1r0 = Right.Y; View.c1r1 = Camera->Up.Y; View.c1r2 = Camera->Forward.Y; View.c1r3 = 0.f;
    View.c2r0 = Right.Z; View.c2r1 = Camera->Up.Z; View.c2r2 = Camera->Forward.Z; View.c2r3 = 0.f;
    View.c3r0 = -Vec3Dot(Right, Camera->Position);
    View.c3r1 = -Vec3Dot(Camera->Up, Camera->Position);
    View.c3r2 = -Vec3Dot(Camera->Forward, Camera->Position);
    View.c3r3 = 1.f;

    return View;
}


mat4x4
GetCameraProjectionMatrix(camera *Camera)
{
    mat4x4 Projection = {0};

    float FovY        = Camera->FovY;
    float AspectRatio = Camera->AspectRatio;
    float Near        = Camera->NearPlane;
    float Far         = Camera->FarPlane;

    float F = 1.f / tanf(FovY / 2.f);

    Projection.c0r0 = F / AspectRatio; Projection.c0r1 = 0.f; Projection.c0r2 = 0.f;                                Projection.c0r3 = 0.f;
    Projection.c1r0 = 0.f;             Projection.c1r1 = F;   Projection.c1r2 = 0.f;                                Projection.c1r3 = 0.f;
    Projection.c2r0 = 0.f;             Projection.c2r1 = 0.f; Projection.c2r2 = (Far + Near) / (Far - Near);        Projection.c2r3 = 1.f;
    Projection.c3r0 = 0.f;             Projection.c3r1 = 0.f; Projection.c3r2 = (-2.f * Far * Near) / (Far - Near); Projection.c3r3 = 0.f;

    return Projection;
}


// ==============================================
// <Scenes>
// ==============================================


void
TestScene1()
{

}