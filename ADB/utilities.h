#pragma once

#include <stdint.h>
#include <stdbool.h>

// ==============================================
// <Utility Macros>
// ==============================================

#define KiB(n)  (((uint64_t)(n)) << 10)
#define MiB(n)  (((uint64_t)(n)) << 20)
#define GiB(n)  (((uint64_t)(n)) << 30)

#define AlignPow2(x,b) (((x) + (b) - 1)&(~((b) - 1)))

#define Minimum(A,B) (((A)<(B))?(A):(B))
#define Maximum(A,B) (((A)>(B))?(A):(B))

#define ArrayCount(a) (sizeof(a) / sizeof(a[0]))

// ==============================================
// <Memory Arenas>
// ==============================================

typedef struct memory_arena
{
    struct memory_arena *Prev;
    struct memory_arena *Current;

    uint64_t             CommitSize;
    uint64_t             ReserveSize;
    uint64_t             Committed;
    uint64_t             Reserved;

    uint64_t             BasePosition;
    uint64_t             Position;

    const char          *AllocatedFromFile;
    uint32_t             AllocatedFromLine;
} memory_arena;

typedef struct
{
    uint64_t    ReserveSize;
    uint64_t    CommitSize;
    const char *AllocatedFromFile;
    uint32_t    AllocatedFromLine;
} memory_arena_params;

typedef struct
{
    memory_arena *Arena;
    uint64_t      Marker;
} memory_region;

memory_arena * AllocateArena      (memory_arena_params Params);
void           ReleaseArena       (memory_arena *Arena);

void         * PushArena          (memory_arena *Arena, uint64_t Size, uint64_t Alignment);
void           PopArenaTo         (memory_arena *Arena, uint64_t Position);
void           ClearArena         (memory_arena *Arena);

memory_region  EnterMemoryRegion  (memory_arena *Arena);
void           LeaveMemoryRegion  (memory_region Region);

#define PushArrayNoZeroAligned(Arena, Type, Count, Align)  ((Type *)PushArena((Arena), sizeof(Type) * (Count), (Align)))
#define PushArrayAligned(Arena, Type, Count, Align)        PushArrayNoZeroAligned((Arena), Type, (Count), (Align))
#define PushArray(Arena, Type, Count)                      PushArrayAligned((Arena), Type, (Count), ((sizeof(Type) < 8) ? 8 : _Alignof(Type)))
#define PushStruct(Arena, Type)                            PushArray((Arena), Type, 1)

// ==============================================
// <Strings>
// ==============================================

typedef struct
{
    uint8_t *Data;
    uint64_t Size;
} byte_string;

#define ByteStringLiteral(String) ByteString((uint8_t *)String, sizeof(String) - 1)

byte_string ByteString          (uint8_t *Data, uint64_t Size);
bool        IsValidByteString   (byte_string String);
                                
bool        ByteStringCompare   (byte_string A, byte_string B);
byte_string ByteStringCopy      (byte_string Input, memory_arena *Arena);
                                
byte_string ReplaceFileName     (byte_string Path, byte_string Name, memory_arena *Arena);
byte_string StripExtensionName  (byte_string Path);
                                
uint64_t    HashByteString      (byte_string String);


// ==============================================
// <Buffer>
// ==============================================

typedef struct
{
    uint8_t *Data;
    size_t   Size;
    size_t   At;
} buffer;


bool        IsBufferValid      (buffer *Buffer);
bool        IsBufferInBounds   (buffer *Buffer);

buffer      ReadFileInBuffer   (byte_string Path, memory_arena *Arena);
uint8_t     GetNextToken       (buffer *Buffer);
uint8_t     PeekBuffer         (buffer *Buffer);
void        SkipWhitespaces    (buffer *Buffer);

float       ParseToSign        (buffer *Buffer);
float       ParseToNumber      (buffer *Buffer);
float       ParseToFloat       (buffer *Buffer);
byte_string ParseToIdentifier  (buffer *Buffer);

bool        IsNewLine          (uint8_t Token);
bool        IsWhiteSpace       (uint8_t Token);
bool        BufferStartsWith   (byte_string String, buffer *Buffer);