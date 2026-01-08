#pragma once

// TODO:
// 0) Decouple materials from meshes
// 1) Vertex Elimination (Agnostic)
// 2) Index  Buffer Gen
// 3) Error Reporting + Robustness

#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "../utilities.h"
#include "parser_obj.h"
#include "platform/platform.h"


// ==============================================
// <.MTL File Parsing>
// ==============================================


typedef struct
{
    union
    {
        struct
        {
            float R, G, B;
        };
        float AsBuffer[3];
    };
} obj_color;


typedef struct
{
    byte_string    Path;
    byte_string    Name;

    obj_color      Diffuse;
    obj_color      Ambient;
    obj_color      Specular;
    obj_color      Emissive;
    float          Shininess;
    float          Opacity;

    loaded_texture ColorTexture;
    loaded_texture NormalTexture;
    loaded_texture RoughnessTexture;
} obj_material;


typedef struct obj_material_node obj_material_node;
struct obj_material_node
{
    obj_material_node *Next;
    obj_material       Value;
};


typedef struct
{
    obj_material_node *First;
    obj_material_node *Last;
    uint32_t           Count;
} obj_material_list;


static byte_string
FindMaterialPath(byte_string Name, obj_material_list *List)
{
    byte_string Result = ByteString(0, 0);

    if (List)
    {
        for (obj_material_node *Node = List->First; Node != 0; Node = Node->Next)
        {
            if (ByteStringCompare(Name, Node->Value.Name))
            {
                Result = Node->Value.Path;
                break;
            }
        }
    }

    return Result;
}


static obj_material_node *
ParseMTLFromFile(byte_string Path, engine_memory *EngineMemory)
{
    obj_material_node *First = 0;
    obj_material_node *Last  = 0;

    buffer FileBuffer = ReadFileInBuffer(Path, EngineMemory->FrameMemory);

    if (IsBufferValid(&FileBuffer))
    {
        while (IsBufferValid(&FileBuffer) && IsBufferInBounds(&FileBuffer))
        {
            uint8_t Token = GetNextToken(&FileBuffer);
            
            if (Token == '\0')
            {
                break;
            }

            switch (Token)
            {

            case 'n':   
            {
                byte_string Rest = ByteStringLiteral("ewmtl");
                if (BufferStartsWith(Rest, &FileBuffer))
                {
                    SkipWhitespaces(&FileBuffer);

                    byte_string MaterialName = ParseToIdentifier(&FileBuffer);
                    byte_string MaterialPath = ReplaceFileName(Path, MaterialName, EngineMemory->FrameMemory);
                    if (IsValidByteString(MaterialPath))
                    {
                        obj_material_node *Node = PushStruct(EngineMemory->FrameMemory, obj_material_node);
                        if (Node)
                        {
                            Node->Next = 0;
                            Node->Value.Name = MaterialName;
                            Node->Value.Path = MaterialPath;
                            // Node->Value.Ambient   = {0, 0, 0};
                            // Node->Value.Diffuse   = {0, 0, 0};
                            // Node->Value.Specular  = {0};
                            Node->Value.Shininess = 0;
                            Node->Value.Opacity   = 0;
                        
                            if (!First)
                            {
                                First = Node;
                                Last  = Node;
                            }
                            else if (Last)
                            {
                                Last->Next = Node;
                                Last       = Node;
                            }
                            else
                            {
                                assert(!"INVALID PARSER STATE");
                            }
                        }
                    }
                }
            } break;

            case 'K':
            {
                if (Last)
                {
                    obj_color *Color = 0;

                    if (PeekBuffer(&FileBuffer) == (uint8_t)'d')
                    {
                        Color = &Last->Value.Diffuse;
                    }
                    else if (PeekBuffer(&FileBuffer) == (uint8_t)'a')
                    {
                        Color = &Last->Value.Ambient;
                    }
                    else if (PeekBuffer(&FileBuffer) == (uint8_t)'s')
                    {
                        Color = &Last->Value.Specular;
                    }
                    else if (PeekBuffer(&FileBuffer) == (uint8_t)'e')
                    {
                        Color = &Last->Value.Emissive;
                    }
                    else
                    {
                        assert(!"UNKNOWN TOKEN");
                    }

                    ++FileBuffer.At; // Is it correct for all cases of 'K' ?

                    assert(Color);
                    for (uint32_t Idx = 0; Idx < 3; ++Idx)
                    {
                        SkipWhitespaces(&FileBuffer);

                        Color->AsBuffer[Idx] = ParseToFloat(&FileBuffer);
                    }
                }
                else
                {
                    assert(!"Bug/Malformed");
                }
            } break;

            case 'm':
            {
                texture_to_load *ToLoad = PushStruct(EngineMemory->FrameMemory, texture_to_load);

                if (Last && ToLoad)
                {
                    byte_string NormalMap    = ByteStringLiteral("ap_Bump");
                    byte_string ColorMap     = ByteStringLiteral("ap_Kd");
                    byte_string RoughnessMap = ByteStringLiteral("ap_Ns");
                    
                    if (BufferStartsWith(NormalMap, &FileBuffer))
                    {
                        ToLoad->Output = &Last->Value.NormalTexture;
                        ToLoad->Id     = 0;
                    }
                    else if (BufferStartsWith(ColorMap, &FileBuffer))
                    {
                        ToLoad->Output = &Last->Value.ColorTexture;
                        ToLoad->Id     = 1;
                    }
                    else if (BufferStartsWith(RoughnessMap, &FileBuffer))
                    {
                        ToLoad->Output = &Last->Value.RoughnessTexture;
                        ToLoad->Id     = 2;
                    }
                    else
                    {
                        assert(!"INVALID TOKEN");
                    }
                    
                    SkipWhitespaces(&FileBuffer);
                    
                    byte_string TextureName = ParseToIdentifier(&FileBuffer);
                    byte_string TexturePath = ReplaceFileName(Path, TextureName, EngineMemory->FrameMemory);
                    
                    ToLoad->FileContent  = ReadFileInBuffer(TexturePath, EngineMemory->FrameMemory);
                    ToLoad->Output->Path = TexturePath;
                    
                    EngineMemory->AddEntry(EngineMemory->WorkQueue, LoadTextureFromDisk, ToLoad);
                }
                else
                {
                    assert(!"INVALID PARSER STATE");
                }
            }

            case 'N':
            {
                if (PeekBuffer(&FileBuffer) == (uint8_t)'s')
                {
                    ++FileBuffer.At;
                    SkipWhitespaces(&FileBuffer);

                    if (Last)
                    {
                        Last->Value.Shininess = ParseToFloat(&FileBuffer);
                    }
                    else
                    {
                        assert(!"Malformed/Bug");
                    }
                }
            } break;

            case 'd':
            {
                SkipWhitespaces(&FileBuffer);

                if (Last)
                {
                    Last->Value.Opacity = ParseToFloat(&FileBuffer);
                }
                else
                {
                    assert(!"Malformed/Bug");
                }
            } break;

            // Ignore those lines.
            case 'i':
            case '#':
            {
                while (IsBufferInBounds(&FileBuffer) && !IsNewLine(PeekBuffer(&FileBuffer)))
                {
                    ++FileBuffer.At;
                }
            } break;

            case '\n':
            {
                // No-Op
            } break;

            default:
            {
                assert(!"Invalid Token");
            } break;

            }
        }
    }

    return First;
}

// ==============================================
// <.OBJ File Parsing>
// ==============================================


typedef struct
{
    uint32_t PositionIndex;
    uint32_t TextureIndex;
    uint32_t NormalIndex;
} obj_vertex;


typedef struct
{
    uint32_t    VertexStart;
    uint32_t    VertexCount;
    byte_string MaterialPath;
} obj_submesh;


typedef struct obj_submesh_node obj_submesh_node;
struct obj_submesh_node
{
    obj_submesh_node *Next;
    obj_submesh       Value;
};


typedef struct
{
    obj_submesh_node *First;
    obj_submesh_node *Last;
    uint32_t          Count;
} obj_submesh_list;


typedef struct
{
    obj_submesh_list Submeshes;
} obj_mesh;

typedef struct obj_mesh_node obj_mesh_node;
struct obj_mesh_node
{
    obj_mesh_node *Next;
    obj_mesh       Value;
};

typedef struct
{
    obj_mesh_node *First;
    obj_mesh_node *Last;
    uint32_t       Count;
} obj_mesh_list;


// I still am unsure about the allocation strategy so force a huge number for now.
#define MAX_ATTRIBUTE_PER_FILE 1'000'000


asset_file_data
ParseObjFromFile(byte_string Path, engine_memory *EngineMemory)
{
    asset_file_data MeshData = {0};

    // Initialize the parsing state
    // (May be abstracted to reduce memory allocs when parsing a sequence of files.)

    obj_mesh_list     *MeshList          = PushStruct(EngineMemory->FrameMemory, obj_mesh_list);
    obj_material_list *MaterialList      = PushStruct(EngineMemory->FrameMemory, obj_material_list);
    buffer             FileBuffer        = ReadFileInBuffer(Path, EngineMemory->FrameMemory);
    vec3              *PositionBuffer    = PushArray(EngineMemory->FrameMemory, vec3, MAX_ATTRIBUTE_PER_FILE);
    uint32_t           PositionCount     = 0;
    vec3              *NormalBuffer      = PushArray(EngineMemory->FrameMemory, vec3, MAX_ATTRIBUTE_PER_FILE);
    uint32_t           NormalCount       = 0;
    vec2              *TextureBuffer     = PushArray(EngineMemory->FrameMemory, vec2, MAX_ATTRIBUTE_PER_FILE);
    uint32_t           TextureCount      = 0;
    obj_vertex        *VertexBuffer      = PushArray(EngineMemory->FrameMemory, obj_vertex, MAX_ATTRIBUTE_PER_FILE);
    uint32_t           VertexCount       = 0;
    uint32_t           TotalSubmeshCount = 0;

    if (IsBufferValid(&FileBuffer) && PositionBuffer && NormalBuffer && TextureBuffer && VertexBuffer && MeshList && MaterialList && EngineMemory->FrameMemory)
    {
        while (IsBufferValid(&FileBuffer) && IsBufferInBounds(&FileBuffer))
        {
            SkipWhitespaces(&FileBuffer);

            uint8_t Token = GetNextToken(&FileBuffer);
            switch (Token)
            {

            case 'v':
            {
                // Should be a hard check.
                assert(PositionCount < MAX_ATTRIBUTE_PER_FILE);
                assert(NormalCount   < MAX_ATTRIBUTE_PER_FILE);
                assert(TextureCount  < MAX_ATTRIBUTE_PER_FILE);
                assert(VertexCount   < MAX_ATTRIBUTE_PER_FILE);

                SkipWhitespaces(&FileBuffer);

                uint32_t Limit       = 0;
                float   *FloatBuffer = 0;

                if (PeekBuffer(&FileBuffer) == (uint8_t)'n' || PeekBuffer(&FileBuffer) == (uint8_t)'N')
                {
                    Limit       = 3;
                    FloatBuffer = NormalBuffer[NormalCount++].AsBuffer;

                    ++FileBuffer.At;
                } else
                if (PeekBuffer(&FileBuffer) == (uint8_t)'t' || PeekBuffer(&FileBuffer) == (uint8_t)'T')
                {
                    Limit       = 2;
                    FloatBuffer = TextureBuffer[TextureCount++].AsBuffer;

                    ++FileBuffer.At;
                }
                else
                {
                    Limit       = 3;
                    FloatBuffer = PositionBuffer[PositionCount++].AsBuffer;
                }

                if (Limit && FloatBuffer)
                {
                    for (uint32_t Idx = 0; Idx < Limit; ++Idx)
                    {
                        SkipWhitespaces(&FileBuffer);

                        if (IsBufferInBounds(&FileBuffer))
                        {
                            FloatBuffer[Idx] = ParseToFloat(&FileBuffer);
                        }
                        else
                        {
                            break;
                        }
                    }
                }
                else
                {
                    assert(!"Handle Error!");
                }
            } break;


            case 'f':
            {
                SkipWhitespaces(&FileBuffer);

                // What is the maximum amount? Is it infinite?
                // In which case we have to be more clever and push temporary data into the arena?
                // And then erase it? Like a small memory region?

                obj_vertex ParsedVertices[32] = {0};
                uint32_t   VertexCountInLine  = 0;

                while (!IsNewLine(PeekBuffer(&FileBuffer)) && VertexCountInLine < 32)
                {
                    SkipWhitespaces(&FileBuffer);

                    obj_vertex *Vertex = &ParsedVertices[VertexCountInLine++];

                    int PositionIndex = (int)ParseToNumber(&FileBuffer);
                    if (PositionIndex > 0)
                    {
                        Vertex->PositionIndex = PositionIndex - 1;
                    }

                    if (PeekBuffer(&FileBuffer) == '/')
                    {
                        ++FileBuffer.At;

                        if (PeekBuffer(&FileBuffer) != '/')
                        {
                            int TextureIndex = (int)ParseToNumber(&FileBuffer);
                            if (TextureIndex > 0)
                            {
                                Vertex->TextureIndex = TextureIndex - 1;
                            }
                        }

                        if (PeekBuffer(&FileBuffer) == '/')
                        {
                            ++FileBuffer.At;

                            int NormalIndex = (int)ParseToNumber(&FileBuffer);
                            if (NormalIndex > 0)
                            {
                                Vertex->NormalIndex = NormalIndex - 1;
                            }
                        }
                    }      
                }

                // 1 --  2
                // |     |
                // |     |
                // |     |
                // |     |
                // 0 --- 3

                // 1 --  2
                // |    /|
                // |   / |
                // |  /  |
                // | /   |
                // 0 --- 3

                // 0, 1, 2 : Triangle0
                // 0, 2, 3 : Triangle1

                obj_submesh_node *Current = MeshList->Last->Value.Submeshes.Last;
                if (Current && VertexCountInLine >= 3)
                {
                    for (uint32_t Idx = 1; Idx < VertexCountInLine - 1; ++Idx)
                    {
                        VertexBuffer[VertexCount++] = ParsedVertices[0];
                        VertexBuffer[VertexCount++] = ParsedVertices[Idx];
                        VertexBuffer[VertexCount++] = ParsedVertices[Idx + 1];

                        Current->Value.VertexCount += 3;
                    }

                }
                else
                {
                    assert(!"INVALID PARSER STATE");
                }
            } break;

            case 'o':
            {
                SkipWhitespaces(&FileBuffer);

                obj_mesh_node *MeshNode = PushStruct(EngineMemory->FrameMemory, obj_mesh_node);
                if (MeshNode)
                {
                    MeshNode->Next = 0;
                    MeshNode->Value.Submeshes.First = 0;
                    MeshNode->Value.Submeshes.Last  = 0;
                    MeshNode->Value.Submeshes.Count = 0;

                    if (!MeshList->First)
                    {
                        MeshList->First = MeshNode;
                        MeshList->Last  = MeshNode;
                    }
                    else if(MeshList->Last)
                    {
                        MeshList->Last->Next = MeshNode;
                        MeshList->Last       = MeshNode;
                    }
                    else
                    {
                        assert(!"INVALID PARSER STATE");
                    }

                    ++MeshList->Count;
                    
                    // TODO: Do we care about the name?
                    while (!IsNewLine(PeekBuffer(&FileBuffer)))
                    {
                        ++FileBuffer.At;
                    }
                }
                else
                {
                    assert(!"OUT OF MEMORY.");
                }
            } break;

            case 'u':
            {
                byte_string Rest = ByteStringLiteral("semtl");
                if (BufferStartsWith(Rest, &FileBuffer))
                {
                    SkipWhitespaces(&FileBuffer);

                    byte_string       MaterialName = ParseToIdentifier(&FileBuffer);
                    obj_submesh_node *SubmeshNode  = PushStruct(EngineMemory->FrameMemory, obj_submesh_node);
                    if (IsValidByteString(MaterialName) && SubmeshNode && MeshList->Last)
                    {
                        ++TotalSubmeshCount;

                        SubmeshNode->Next = 0;
                        SubmeshNode->Value.MaterialPath = FindMaterialPath(MaterialName, MaterialList);
                        SubmeshNode->Value.VertexCount  = 0;
                        SubmeshNode->Value.VertexStart  = VertexCount;

                        obj_submesh_list *List = &MeshList->Last->Value.Submeshes;
                        if (!List->First)
                        {
                            List->First = SubmeshNode;
                            List->Last  = SubmeshNode;
                        }
                        else if(List->Last)
                        {
                            List->Last->Next = SubmeshNode;
                            List->Last       = SubmeshNode;
                        }
                        else
                        {
                            assert(!"INVALID PARSER STATE");
                        }

                        ++List->Count;
                    }
                    else
                    {
                        assert(!"OUT OF MEMORY.");
                    }
                }
            } break;

            case 'm':
            {
                byte_string Rest = ByteStringLiteral("tllib");
                if (BufferStartsWith(Rest, &FileBuffer))
                {
                    SkipWhitespaces(&FileBuffer);

                    byte_string LibName = ParseToIdentifier(&FileBuffer);
                    byte_string Lib     = ReplaceFileName(Path, LibName, EngineMemory->FrameMemory);

                    for (obj_material_node *Node = ParseMTLFromFile(Lib, EngineMemory); Node != 0; Node = Node->Next)
                    {
                        if (MaterialList)
                        {
                            if (!MaterialList->First)
                            {
                                MaterialList->First = Node;
                                MaterialList->Last = Node;
                            }
                            else if (MaterialList->Last)
                            {
                                MaterialList->Last->Next = Node;
                                MaterialList->Last = Node;
                            }
                            else
                            {
                                assert(!"INVALID PARSER STATE");
                            }

                            ++MaterialList->Count;
                        }
                        else
                        {
                            assert(!"INVALID PARSER STATE");
                        }
                    }
                }
                else
                {
                    assert(!"INVALID TOKEN");
                }
            } break;

            case '#':
            case 's':
            case 'g':
            {
                while (IsBufferInBounds(&FileBuffer) && !IsNewLine(PeekBuffer(&FileBuffer)))
                {
                    ++FileBuffer.At;
                }
            } break;

            case '\n':
            {
                // No-Op
            } break;

            case '\0':
            {
                // Wait on all the threaded work we enqueued. This might be too early. Perhaps we want to be lazier.
                // Issue is we can't since we copy into the materials array. It's looks easily fixable, unsure yet.
                EngineMemory->CompleteWork(EngineMemory->WorkQueue);

                uint64_t VertexDataSize   = VertexCount         * sizeof(mesh_vertex_data);
                uint64_t SubmeshDataSize  = TotalSubmeshCount   * sizeof(submesh_data);
                uint64_t MaterialDataSize = MaterialList->Count * sizeof(material_data);

                // TODO: Should allocate from a backing buffer.
                uint64_t            Footprint = VertexDataSize + SubmeshDataSize + MaterialDataSize;
                memory_arena_params Params =
                {
                    .AllocatedFromFile = __FILE__,
                    .AllocatedFromLine = __LINE__,
                    .ReserveSize       = Footprint,
                    .CommitSize        = Footprint,
                };

                memory_arena *OutputArena = AllocateArena(Params);
                if (OutputArena)
                {
                    MeshData.Vertices      = PushArray(OutputArena, mesh_vertex_data, VertexCount);
                    MeshData.VertexCount   = 0;
                    MeshData.Submeshes     = PushArray(OutputArena, submesh_data, TotalSubmeshCount);
                    MeshData.SubmeshCount  = 0;
                    MeshData.Materials     = PushArray(OutputArena, material_data, MaterialList->Count);
                    MeshData.MaterialCount = 0;

                    for (obj_mesh_node *MeshNode = MeshList->First; MeshNode != 0; MeshNode = MeshNode->Next)
                    {
                        obj_mesh Mesh = MeshNode->Value;

                        for (obj_submesh_node *SubmeshNode = Mesh.Submeshes.First; SubmeshNode != 0; SubmeshNode = SubmeshNode->Next)
                        {
                            obj_submesh Submesh  = SubmeshNode->Value;
                            obj_vertex *Vertices = VertexBuffer + Submesh.VertexStart;

                            for (uint32_t Idx = 0; Idx < Submesh.VertexCount; ++Idx)
                            {
                                vec3 Position = PositionBuffer[Vertices[Idx].PositionIndex];
                                vec2 Texture  = TextureBuffer[Vertices[Idx].TextureIndex];
                                vec3 Normal   = NormalBuffer[Vertices[Idx].NormalIndex];

                                MeshData.Vertices[MeshData.VertexCount++] = (mesh_vertex_data){.Position = Position, .Texture = Texture, .Normal = Normal};
                            }

                            submesh_data *SubmeshData = MeshData.Submeshes + MeshData.SubmeshCount++;
                            SubmeshData->MaterialPath = Submesh.MaterialPath;
                            SubmeshData->Name         = ByteString(0, 0);
                            SubmeshData->VertexCount  = Submesh.VertexCount;
                            SubmeshData->VertexOffset = Submesh.VertexStart;
                        }
                    }

                    for (obj_material_node *MaterialNode = MaterialList->First; MaterialNode != 0; MaterialNode = MaterialNode->Next)
                    {
                        material_data *MaterialData = MeshData.Materials + MeshData.MaterialCount++;
                        MaterialData->Textures[MaterialMap_Color]     = MaterialNode->Value.ColorTexture;
                        MaterialData->Textures[MaterialMap_Normal]    = MaterialNode->Value.NormalTexture;
                        MaterialData->Textures[MaterialMap_Roughness] = MaterialNode->Value.RoughnessTexture;
                        MaterialData->Opacity                         = MaterialNode->Value.Opacity;
                        MaterialData->Shininess                       = MaterialNode->Value.Shininess;
                        MaterialData->Path                            = MaterialNode->Value.Path;
                    }
                }
            } break;

            default:
            {
                assert(!"Invalid Token");
            } break;

            }
        }
    }

    return MeshData;
}