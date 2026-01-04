#pragma once

// TODO:
// 1) Threaded Texture IO
// 2) Figure out memory stuff
// 3) Write to IF and pull the specific types in.

#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "../utilities.h"
#include "parser_obj.h"

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

typedef struct
{
    uint8_t *Data;
    size_t   Size;
    size_t   At;
} buffer;


static bool
IsBufferValid(buffer *Buffer)
{
    bool Result = (Buffer && Buffer->Data && Buffer->Size);
    return Result;
}


static bool
IsBufferInBounds(buffer *Buffer)
{
    assert(IsBufferValid(Buffer));

    bool Result = Buffer->At < Buffer->Size;
    return Result;
}

static uint8_t
GetNextToken(buffer *Buffer)
{
    assert(IsBufferValid(Buffer) && IsBufferInBounds(Buffer));

    uint8_t Result = Buffer->Data[Buffer->At++];
    return Result;
}


static uint8_t
PeekBuffer(buffer *Buffer)
{
    assert(IsBufferValid(Buffer) && IsBufferInBounds(Buffer));

    uint8_t Result = Buffer->Data[Buffer->At];
    return Result;
}


static bool
IsNewLine(uint8_t Token)
{
    bool Result = Token == '\n';
    return Result;
}


static bool
IsWhiteSpace(uint8_t Token)
{
    bool Result = Token == ' ' || Token == '\t' || Token == '\r';
    return Result;
}


static buffer
ReadFileInBuffer(byte_string Path, memory_arena *Arena)
{
    buffer Result = {0};

    if (IsValidByteString(Path))
    {
        FILE *File = fopen((const char *)Path.Data, "rb");
        if (File)
        {
            fseek(File, 0, SEEK_END);
            long FileSize = ftell(File);
            fseek(File, 0, SEEK_SET);
        
            if (FileSize > 0)
            {
                uint8_t *Buffer = PushArray(Arena, uint8_t, FileSize + 1);
                if (Buffer)
                {
                    size_t BytesRead = fread(Buffer, 1, (size_t)FileSize, File);
        
                    Result.Data       = Buffer;
                    Result.At         = 0;
                    Result.Size       = FileSize + 1;
                    Buffer[BytesRead] = '\0';
                }
            }
        
            fclose(File);
        }
    }

    return Result;
}


static void
SkipWhitespaces(buffer *Buffer)
{
    assert(IsBufferValid(Buffer));

    while (IsBufferInBounds(Buffer) && IsWhiteSpace(PeekBuffer(Buffer)))
    {
        ++Buffer->At;
    }
}


static float
ParseToSign(buffer *Buffer)
{
    float Result = 1.f;

    if (IsBufferInBounds(Buffer) && PeekBuffer(Buffer) ==  '-')
    {
        Result = -1.f;
        ++Buffer->At;
    }

    return Result;
}


static float
ParseToNumber(buffer *Buffer)
{
    assert(IsBufferValid(Buffer));

    float Result = 0.f;

    while (IsBufferInBounds(Buffer))
    {
        uint8_t Token    = GetNextToken(Buffer);
        uint8_t AsNumber = Token - (uint8_t)'0';

        if (AsNumber < 10)
        {
            Result = 10.f * Result + (float)AsNumber;
        }
        else
        {
            --Buffer->At;
            break;
        }
    }

    return Result;
}


static float
ParseToFloat(buffer *Buffer)
{
    assert(IsBufferValid(Buffer));

    float Result = 0.f;

    float Sign   = ParseToSign(Buffer);
    float Number = ParseToNumber(Buffer);

    if (IsBufferInBounds(Buffer) && PeekBuffer(Buffer) ==  '.')
    {
        ++Buffer->At;

        float C = 1.f / 10.f;
        while (IsBufferInBounds(Buffer))
        {
            uint8_t AsNumber = Buffer->Data[Buffer->At] - (uint8_t)'0';
            if (AsNumber < 10)
            {
                Number = Number + C * (float)AsNumber;
                C     *= 1.f / 10.f;

                ++Buffer->At;
            }
            else
            {
                break;
            }
        }
    }

    if (IsBufferInBounds(Buffer) && PeekBuffer(Buffer) == 'e' || PeekBuffer(Buffer) == 'E')
    {
        ++Buffer->At;

        if (IsBufferInBounds(Buffer) && PeekBuffer(Buffer) == '+')
        {
            ++Buffer->At;
        }

        float ExponentSign = ParseToSign(Buffer);
        float Exponent     = ExponentSign * ParseToFloat(Buffer);

        Number *= powf(10.f, Exponent);
    }

    Result = Sign * Number;

    return Result;
}


static byte_string
ParseToIdentifier(buffer *Buffer)
{
    assert(IsBufferValid(Buffer));

    byte_string Result = ByteString(Buffer->Data + Buffer->At, 0);

    while (IsBufferInBounds(Buffer) && !IsNewLine(PeekBuffer(Buffer)) && !IsWhiteSpace(PeekBuffer(Buffer)))
    {
        Result.Size += 1;
        Buffer->At  += 1;
    }

    return Result;
}


static bool
BufferStartsWith(byte_string String, buffer *Buffer)
{
    bool Result = true;

    if (IsValidByteString(String) && IsBufferValid(Buffer) && Buffer->At + String.Size < Buffer->Size)
    {
        for (uint64_t Idx = 0; Idx < String.Size; ++Idx)
        {
            if (Buffer->Data[Buffer->At + Idx] != (uint8_t)String.Data[Idx])
            {
                Result = false;
                break;
            }
        }
    }

    return Result;
}


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
    byte_string Name;
    obj_color   Diffuse;
    obj_color   Ambient;
    obj_color   Specular;
    obj_color   Emissive;
    float       Shininess;
    float       Opacity;

    byte_string ColorTexture;
    byte_string NormalTexture;
    byte_string RoughnessTexture;
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


static obj_material
FindMaterial(byte_string Name, obj_material_list *List)
{
    obj_material Result = {0};

    if (List)
    {
        for (obj_material_node *Node = List->First; Node != 0; Node = Node->Next)
        {
            if (ByteStringCompare(Name, Node->Value.Name))
            {
                Result = Node->Value;
                break;
            }
        }
    }

    return Result;
}


static obj_material_node *
ParseMTLFromFile(byte_string Path, uint32_t *StringFootprint, memory_arena *ParseArena)
{
    obj_material_node *First = 0;
    obj_material_node *Last  = 0;

    buffer FileBuffer = ReadFileInBuffer(Path, ParseArena);

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
                    FileBuffer.At += Rest.Size;
                    SkipWhitespaces(&FileBuffer);

                    byte_string MtlName = ParseToIdentifier(&FileBuffer);
                    if (IsValidByteString(MtlName))
                    {
                        // Shouldn't we push into the output arena?
                        obj_material_node *Node = PushStruct(ParseArena, obj_material_node);
                        if (Node)
                        {
                            Node->Next = 0;
                            Node->Value.Name = ByteStringCopy(MtlName, ParseArena);
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

            // TODO: Remove the loading from here.
            case 'm':
            {
                byte_string Rest = ByteStringLiteral("ap");
                if (BufferStartsWith(Rest, &FileBuffer) && Last)
                {
                    FileBuffer.At += Rest.Size;
                    SkipWhitespaces(&FileBuffer);

                    byte_string NormalMap    = ByteStringLiteral("_Bump");
                    byte_string ColorMap     = ByteStringLiteral("_Kd");
                    byte_string RoughnessMap = ByteStringLiteral("_Ns");

                    byte_string *TextureName = 0;

                    if (BufferStartsWith(NormalMap, &FileBuffer))
                    {
                        FileBuffer.At += NormalMap.Size;
                        TextureName    = &Last->Value.NormalTexture;
                    }
                    else if (BufferStartsWith(ColorMap, &FileBuffer))
                    {
                        FileBuffer.At += ColorMap.Size;
                        TextureName    = &Last->Value.ColorTexture;
                    }
                    else if (BufferStartsWith(RoughnessMap, &FileBuffer))
                    {
                        FileBuffer.At += RoughnessMap.Size;
                        TextureName    = &Last->Value.RoughnessTexture;
                    }
                    else
                    {
                        assert(!"INVALID TOKEN");
                    }

                    SkipWhitespaces(&FileBuffer);

                    // But we need to know the size of the string, because it counts in the footprint.
                    if (TextureName)
                    {
                        byte_string Name = ParseToIdentifier(&FileBuffer);

                        StringFootprint += Name.Size;
                        *TextureName     = Name;
                    }
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

                assert(Last);
                Last->Value.Opacity = ParseToFloat(&FileBuffer);
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
    uint32_t     VertexStart;
    uint32_t     VertexCount;
    obj_material Material;
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


mesh_data
ParseObjFromFile(byte_string Path)
{
    mesh_data MeshData = {0};

    // Initialize the parsing state
    // (May be abstracted to reduce memory allocs when parsing a sequence of files.)

    buffer             FileBuffer        = {0};
    vec3              *PositionBuffer    = 0;
    uint32_t           PositionCount     = 0;
    vec3              *NormalBuffer      = 0;
    uint32_t           NormalCount       = 0;
    vec2              *TextureBuffer     = 0;
    uint32_t           TextureCount      = 0;
    obj_vertex        *VertexBuffer      = 0;
    uint32_t           VertexCount       = 0;
    uint32_t           TotalSubmeshCount = 0;
    obj_mesh_list     *MeshList          = 0;
    obj_material_list *MaterialList      = 0;
    uint64_t           StringFootprint   = 0;
    memory_arena      *ParseArena        = {0};
    {
        memory_arena_params Params =
        {
            .AllocatedFromFile = __FILE__,
            .AllocatedFromLine = __LINE__,
            .CommitSize        = MiB(64),
            .ReserveSize       = GiB(1),
        };

        ParseArena = AllocateArena(Params);

        if (ParseArena)
        {
             MeshList       = PushStruct(ParseArena, obj_mesh_list);
             MaterialList   = PushStruct(ParseArena, obj_material_list);
             FileBuffer     = ReadFileInBuffer(Path, ParseArena);
             PositionBuffer = PushArray(ParseArena, vec3      , MAX_ATTRIBUTE_PER_FILE);
             NormalBuffer   = PushArray(ParseArena, vec3      , MAX_ATTRIBUTE_PER_FILE);
             TextureBuffer  = PushArray(ParseArena, vec2      , MAX_ATTRIBUTE_PER_FILE);
             VertexBuffer   = PushArray(ParseArena, obj_vertex, MAX_ATTRIBUTE_PER_FILE);
        }
    }

    if (IsBufferValid(&FileBuffer) && PositionBuffer && NormalBuffer && TextureBuffer && VertexBuffer && MeshList && MaterialList && ParseArena)
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

                obj_mesh_node *MeshNode = PushStruct(ParseArena, obj_mesh_node);
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
                    FileBuffer.At += Rest.Size;
                    SkipWhitespaces(&FileBuffer);

                    byte_string       MtlName     = ParseToIdentifier(&FileBuffer);
                    obj_submesh_node *SubmeshNode = PushStruct(ParseArena, obj_submesh_node);
                    if (IsValidByteString(MtlName) && SubmeshNode && MeshList->Last)
                    {
                        ++TotalSubmeshCount;

                        SubmeshNode->Next = 0;
                        SubmeshNode->Value.Material    = FindMaterial(MtlName, MaterialList);
                        SubmeshNode->Value.VertexCount = 0;
                        SubmeshNode->Value.VertexStart = VertexCount;

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
                    FileBuffer.At += Rest.Size;
                    SkipWhitespaces(&FileBuffer);

                    byte_string LibName = ParseToIdentifier(&FileBuffer);
                    byte_string Lib     = ReplaceFileName(Path, LibName, ParseArena);

                    for (obj_material_node *Node = ParseMTLFromFile(Lib, &StringFootprint, ParseArena); Node != 0; Node = Node->Next)
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
                uint64_t VertexDataSize   = VertexCount         * sizeof(mesh_vertex_data);
                uint64_t SubmeshDataSize  = TotalSubmeshCount   * sizeof(submesh_data);
                uint64_t MaterialDataSize = MaterialList->Count * sizeof(material_data);

                // TODO: Should allocate from a backing buffer.
                uint64_t            Footprint = VertexDataSize + SubmeshDataSize + MaterialDataSize + StringFootprint;
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
                            SubmeshData->MaterialId = 0;                     // What is this?
                            SubmeshData->Name         = ByteString(0, 0);    // Issue with footprint? Has to be string CPY. Does this even matter?
                            SubmeshData->VertexCount  = Submesh.VertexCount; // How many vertices for this mesh.
                            SubmeshData->VertexOffset = Submesh.VertexStart; // Index into MeshData.Vertices (is this correct? Don't even have to track it)
                        }
                    }

                    for (obj_material_node *MaterialNode = MaterialList->First; MaterialNode != 0; MaterialNode = MaterialNode->Next)
                    {
                        material_data *MaterialData = MeshData.Materials + MeshData.MaterialCount++;
                        MaterialData->ColorTexture     = ByteStringCopy(MaterialNode->Value.ColorTexture    , OutputArena);
                        MaterialData->NormalTexture    = ByteStringCopy(MaterialNode->Value.NormalTexture   , OutputArena);
                        MaterialData->RoughnessTexture = ByteStringCopy(MaterialNode->Value.RoughnessTexture, OutputArena);
                        MaterialData->Opacity          = MaterialNode->Value.Opacity;
                        MaterialData->Shininess        = MaterialNode->Value.Shininess;
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