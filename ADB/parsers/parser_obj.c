#pragma once

#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <string.h>

#include "../utilities.h"

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
    bool Result = Token == '\r' || Token == '\n';
    return Result;
}


static bool
IsWhiteSpace(uint8_t Token)
{
    bool Result = Token == ' ' || Token == '\t';
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
                    Result.Size       = BytesRead;
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


// MTL Specific.

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
    float       Shininess;
    float       Opacity;
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


// TODO: What do we even want to return from this?
static obj_material_node *
ParseMTLFromFile(byte_string Path, memory_arena *ParseArena)
{
    obj_material_node *First = 0;
    obj_material_node *Last  = 0;

    buffer Buffer = ReadFileInBuffer(Path, ParseArena);

    if (IsBufferValid(&Buffer))
    {
        while (IsBufferValid(&Buffer) && IsBufferInBounds(&Buffer))
        {
            char Token = GetNextToken(&Buffer);

            switch (Token)
            {

            case 'n':   
            {
                byte_string Rest = ByteStringLiteral("ewmtl");
                if (BufferStartsWith(Rest, &Buffer))
                {
                    Buffer.At += Rest.Size;
                    SkipWhitespaces(&Buffer);

                    byte_string MtlName = ParseToIdentifier(&Buffer);
                    if (IsValidByteString(MtlName))
                    {
                        // Shouldn't we push into the output arena?
                        obj_material_node *Node = PushStruct(ParseArena, obj_material_node);
                        if (Node)
                        {
                            Node->Next  = 0;
                            Node->Value.Name      = ByteStringCopy(MtlName, ParseArena);
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
                            else
                            {
                                assert(Last);
                        
                                Last->Next = Node;
                                Last       = Node;
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

                    if (PeekBuffer(&Buffer) == (uint8_t)'d')
                    {
                        Color = &Last->Value.Diffuse;
                    }
                    else if (PeekBuffer(&Buffer) == (uint8_t)'a')
                    {
                        Color = &Last->Value.Ambient;
                    }
                    else if (PeekBuffer(&Buffer) == (uint8_t)'s')
                    {
                        Color = &Last->Value.Specular;
                    }
                    else
                    {
                        assert(!"UNKNOWN TOKEN");
                    }

                    ++Buffer.At; // Is it correct for all cases of 'K' ?

                    assert(Color);
                    for (uint32_t Idx = 0; Idx < 3; ++Idx)
                    {
                        SkipWhitespaces(&Buffer);

                        Color->AsBuffer[Idx] = ParseToFloat(&Buffer);
                    }
                }
                else
                {
                    assert(!"Bug/Malformed");
                }
            } break;

            case 'N':
            {
                if (PeekBuffer(&Buffer) == (uint8_t)'s')
                {
                    ++Buffer.At;
                    SkipWhitespaces(&Buffer);

                    if (Last)
                    {
                        Last->Value.Shininess = ParseToFloat(&Buffer);
                    }
                    else
                    {
                        assert(!"Malformed/Bug");
                    }
                }
            } break;

            case 'd':
            {
                SkipWhitespaces(&Buffer);

                assert(Last);
                Last->Value.Opacity = ParseToFloat(&Buffer);
            } break;

            // Ignore those lines.
            case 'i':
            case '#':
            {
                while (IsBufferInBounds(&Buffer) && !IsNewLine(PeekBuffer(&Buffer)))
                {
                    ++Buffer.At;
                }
            } break;

            case '\r':
            case '\n':
            {
                ++Buffer.At;
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

// OBJ Specific.

#define OBJ_MAX_VALUE_PER_VERTEX  4
#define OBJ_MAX_VALUE_PER_NORMAL  3
#define OBJ_MAX_VALUE_PER_TEXTURE 2

typedef struct
{
    union
    {
        struct
        {
            float X, Y, Z, W;
        };
        float AsBuffer[OBJ_MAX_VALUE_PER_VERTEX];
    };
} obj_position;


typedef struct
{
    union
    {
        struct
        {
            float X, Y, Z;
        };
        float AsBuffer[OBJ_MAX_VALUE_PER_NORMAL];
    };
} obj_normal;


typedef struct
{
    union
    {
        struct
        {
            float X, Y;
        };
        float AsBuffer[OBJ_MAX_VALUE_PER_TEXTURE];
    };
} obj_texture;


typedef struct
{
    obj_position Position;
    obj_texture  Texture;
    obj_normal   Normal;
} obj_vertex;


typedef struct
{
    int Position;
    int Texture;
    int Normal;
} obj_face_vertex;


typedef struct
{
    obj_vertex  *VertexBuffer;
    uint64_t     VertexBufferSize;
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

    memory_arena  *BackingMemory;
} obj_mesh_list;


typedef struct
{
    obj_material_list *MaterialList;
    obj_mesh_node     *MeshNode;
    memory_arena      *ParseArena;
    memory_arena      *OutputArena;
    uint64_t          SubmeshStart;
} obj_parser_state;


static void
ClearParserState(obj_parser_state *State)
{
    if (State)
    {
        if (!State->ParseArena)
        {
            memory_arena_params Params =
            {
                .AllocatedFromFile = __FILE__,
                .AllocatedFromLine = __LINE__,
                .CommitSize        = MiB(64),
                .ReserveSize       = GiB(1),
            };

            State->ParseArena = AllocateArena(Params);
        }

        if (!State->OutputArena)
        {
            memory_arena_params Params =
            {
                .AllocatedFromFile = __FILE__,
                .AllocatedFromLine = __LINE__,
                .CommitSize        = MiB(64),
                .ReserveSize       = GiB(1),
            };

            State->OutputArena = AllocateArena(Params);
        }

        // I don't know enough about the format yet.

        // ClearArena(State->ParseArena);
        // ClearArena(State->OutputArena);
    }
}


void
ParseObjFromFile(byte_string Path)
{
    obj_parser_state State = {0};
    ClearParserState(&State);

    obj_mesh_list *MeshList = PushStruct(State.OutputArena, obj_mesh_list);
    if (!MeshList)
    {
        assert(!"OUT OF MEMORY");
    }
    MeshList->BackingMemory = State.OutputArena;

    // The overall allocation strategy is this:
    // [MeshList|SubMesh0|Submesh0.Data|Submesh1|Submesh1.Data|Submesh2|Submesh2.Data] -> Output Arena
    // We return the MeshList which contains the arena to prevent leaks.

    buffer Buffer = ReadFileInBuffer(Path, State.ParseArena);
    if (IsBufferValid(&Buffer))
    {
        // The overall allocation strategy is this:
        // [PositionBuffer0|NormalBuffer0|TextureBuffer0|PositionBuffer1|TextureBuffer1] -> Parse Arena
        // Re-Allocate into the arena as we need. Need some minor logic to find the current block to read from.
        // We'll keep it on the stack for now.

        obj_position *PositionBuffer = PushArray(State.ParseArena, obj_position, 512);
        uint32_t      PositionIndex  = 0;
        obj_normal   *NormalBuffer   = PushArray(State.ParseArena, obj_normal  , 512);
        uint32_t      NormalIndex    = 0;
        obj_texture  *TextureBuffer  = PushArray(State.ParseArena, obj_texture , 512);
        uint32_t      TextureIndex   = 0;

        if (!PositionBuffer || !NormalBuffer || !TextureBuffer)
        {
            assert(!"OUT OF MEMORY");
        }

        while (IsBufferValid(&Buffer) && IsBufferInBounds(&Buffer))
        {
            char Token = GetNextToken(&Buffer);

            if (Token == '\0')
            {
                break;
            }

            switch (Token)
            {

            case 'v':
            case 'V':
            {
                SkipWhitespaces(&Buffer);

                uint32_t Limit       = 0;
                float   *FloatBuffer = 0;

                if (PeekBuffer(&Buffer) == (uint8_t)'n' || PeekBuffer(&Buffer) == (uint8_t)'N')
                {
                    Limit       = OBJ_MAX_VALUE_PER_NORMAL;
                    FloatBuffer = NormalBuffer[NormalIndex++].AsBuffer;

                    ++Buffer.At;
                } else
                if (PeekBuffer(&Buffer) == (uint8_t)'t' || PeekBuffer(&Buffer) == (uint8_t)'T')
                {
                    Limit       = OBJ_MAX_VALUE_PER_TEXTURE;
                    FloatBuffer = TextureBuffer[TextureIndex++].AsBuffer;

                    ++Buffer.At;
                }
                else
                {
                    Limit       = OBJ_MAX_VALUE_PER_VERTEX;
                    FloatBuffer = PositionBuffer[PositionIndex++].AsBuffer;
                }

                if (Limit && FloatBuffer)
                {
                    for (uint32_t Idx = 0; Idx < Limit; ++Idx)
                    {
                        SkipWhitespaces(&Buffer);

                        if (IsBufferInBounds(&Buffer))
                        {
                            FloatBuffer[Idx] = ParseToFloat(&Buffer);
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
            case 'F':
            {
                SkipWhitespaces(&Buffer);

                while (!IsNewLine(PeekBuffer(&Buffer)))
                {
                    SkipWhitespaces(&Buffer);

                    obj_face_vertex Face = { 0, 0, 0 };

                    Face.Position = (int)ParseToNumber(&Buffer);

                    if (PeekBuffer(&Buffer) == '/')
                    {
                        ++Buffer.At;

                        if (PeekBuffer(&Buffer) != '/')
                        {
                            Face.Texture = (int)ParseToNumber(&Buffer);
                        }

                        if (PeekBuffer(&Buffer) == '/')
                        {
                            ++Buffer.At;

                            Face.Normal = (int)ParseToNumber(&Buffer);
                        }
                    }

                    // TODO: Add support for negative indices.
                    obj_vertex *Vertex = PushStruct(State.OutputArena, obj_vertex);
                    if (Vertex)
                    {
                        if (Face.Position > 0)
                        {
                            Vertex->Position = PositionBuffer[Face.Position - 1];
                        }

                        if (Face.Texture > 0)
                        {
                            Vertex->Texture  = TextureBuffer[Face.Texture - 1];
                        }

                        if (Face.Normal > 0)
                        {
                            Vertex->Normal  = NormalBuffer[Face.Normal - 1];
                        }

                        obj_submesh_node *SubmeshNode = State.MeshNode->Value.Submeshes.Last;
                        if (SubmeshNode)
                        {
                            if (!SubmeshNode->Value.VertexBuffer)
                            {
                                SubmeshNode->Value.VertexBuffer = Vertex;
                            }

                            SubmeshNode->Value.VertexBufferSize += 1;
                        }
                        else
                        {
                            assert(!"Invalid State");
                        }
                    }
                    else
                    {
                        assert(!"OUT OF MEMORY");
                    }
                }

                // TODO: Maybe triangulate? I think we need a bit more processing to be correct.
            } break;

            case 'o':
            {
                SkipWhitespaces(&Buffer);

                // Really unsure. Because this clears the output which we might not want.
                // This whole parser state is weird. Kind of unsure.
                ClearParserState(&State);

                State.MeshNode = PushStruct(State.OutputArena, obj_mesh_node);
                if (State.MeshNode)
                {
                    State.MeshNode->Next = 0;
                    State.MeshNode->Value.Submeshes.First = 0;
                    State.MeshNode->Value.Submeshes.Last  = 0;
                    State.MeshNode->Value.Submeshes.Count = 0;

                    if (!MeshList->First)
                    {
                        MeshList->First = State.MeshNode;
                        MeshList->Last  = State.MeshNode;
                    }
                    else
                    {
                        MeshList->Last->Next = State.MeshNode;
                        MeshList->Last       = State.MeshNode;
                    }
                    ++MeshList->Count;
                    
                    while (!IsNewLine(PeekBuffer(&Buffer)))
                    {
                        ++Buffer.At;
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
                if (BufferStartsWith(Rest, &Buffer))
                {
                    Buffer.At += Rest.Size;
                    SkipWhitespaces(&Buffer);

                    byte_string       MtlName     = ParseToIdentifier(&Buffer);
                    obj_submesh_node *SubmeshNode = PushStruct(State.OutputArena, obj_submesh_node);
                    if (IsValidByteString(MtlName) && SubmeshNode && State.MeshNode)
                    {
                        SubmeshNode->Next = 0;
                        SubmeshNode->Value.Material         = FindMaterial(MtlName, State.MaterialList);
                        SubmeshNode->Value.VertexBuffer     = 0;
                        SubmeshNode->Value.VertexBufferSize = 0;

                        obj_submesh_list *List = &State.MeshNode->Value.Submeshes;
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
                            assert(!"INVALID STATE");
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
                if (BufferStartsWith(Rest, &Buffer))
                {
                    Buffer.Data += Rest.Size;
                    SkipWhitespaces(&Buffer);

                    byte_string LibName = ParseToIdentifier(&Buffer);
                    byte_string Lib     = ReplaceFileName(Path, LibName, State.ParseArena);

                    for (obj_material_node *Node = ParseMTLFromFile(Lib, State.ParseArena); Node != 0; Node = Node->Next)
                    {
                        if (!State.MaterialList)
                        {
                            State.MaterialList = PushStruct(State.ParseArena, obj_material_list);
                        }

                        obj_material_list *MaterialList = State.MaterialList;
                        if (MaterialList)
                        {
                            if (!MaterialList->First)
                            {
                                MaterialList->First = Node;
                                MaterialList->Last  = Node;
                            }
                            else if(MaterialList->Last)
                            {
                                MaterialList->Last->Next = Node;
                                MaterialList->Last       = Node;
                            }
                            else
                            {
                                assert(!"INVALID STATE");
                            }

                            ++MaterialList->Count;
                        }
                        else
                        {
                            assert(!"OUT OF MEMORY");
                        }
                    }
                }
                else
                {
                    assert(!"INVALID TOKEN");
                }
            }

            case '#':
            case 's':
            case 'g':
            {
                while (IsBufferInBounds(&Buffer) && !IsNewLine(PeekBuffer(&Buffer)))
                {
                    ++Buffer.At;
                }
            } break;

            case '\r':
            case '\n':
            {
                ++Buffer.At;
            } break;

            default:
            {
                assert(!"Invalid Token");
            } break;

            }
        }

        // assert(!"Handle the rest.");
    }
}