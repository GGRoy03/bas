#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include "utilities.h"         // Implementation Header

#include "platform/platform.h" // Allocation

memory_arena *
AllocateArena(memory_arena_params Params)
{
    uint64_t ReserveSize = AlignPow2(Params.ReserveSize, KiB(4));
    uint64_t CommitSize  = AlignPow2(Params.CommitSize , KiB(4));

    if (CommitSize > ReserveSize)
    {
        CommitSize = ReserveSize;
    }

    void *HeapBase     = OSReserve(ReserveSize);
    bool  CommitResult = OSCommit(HeapBase, CommitSize);
    if (!HeapBase || !CommitResult)
    {
        return 0;
    }

    memory_arena *Arena = (memory_arena *)HeapBase;
    Arena->Prev              = 0;
    Arena->Current           = Arena;
    Arena->CommitSize        = Params.CommitSize;
    Arena->ReserveSize       = Params.ReserveSize;
    Arena->Committed         = CommitSize;
    Arena->Reserved          = ReserveSize;
    Arena->BasePosition      = 0;
    Arena->Position          = sizeof(memory_arena);
    Arena->AllocatedFromFile = Params.AllocatedFromFile;
    Arena->AllocatedFromLine = Params.AllocatedFromLine;

    return Arena;
}

void
ReleaseArena(memory_arena *Arena)
{
    memory_arena *Prev = 0;
    for (memory_arena *Node = Arena->Current; Node != 0; Node = Prev)
    {
        Prev = Node->Prev;
        OSRelease(Node, Node->Reserved);
    }
}

void *
PushArena(memory_arena *Arena, uint64_t Size, uint64_t Alignment)
{
    memory_arena *Active       = Arena->Current;
    uint64_t      PrePosition  = AlignPow2(Active->Position, Alignment);
    uint64_t      PostPosition = PrePosition + Size;

    if (Active->Reserved < PostPosition)
    {
        uint64_t ReserveSize = Active->ReserveSize;
        uint64_t CommitSize  = Active->CommitSize;

        if (Size + sizeof(memory_arena) > ReserveSize)
        {
            ReserveSize = AlignPow2(Size + sizeof(memory_arena), Alignment);
            CommitSize  = AlignPow2(Size + sizeof(memory_arena), Alignment);
        }

        memory_arena_params Params;
        Params.CommitSize        = CommitSize;
        Params.ReserveSize       = ReserveSize;
        Params.AllocatedFromFile = Active->AllocatedFromFile;
        Params.AllocatedFromLine = Active->AllocatedFromLine;

        memory_arena *NewArena = AllocateArena(Params);
        NewArena->BasePosition = Active->BasePosition + Active->ReserveSize;
        NewArena->Prev         = Active;

        Arena->Current = NewArena;
        Active = NewArena;

        PrePosition  = AlignPow2(Active->Position, Alignment);
        PostPosition = PrePosition + Size;
    }

    if (Active->Committed < PostPosition)
    {
        uint64_t CommitPostAligned = PostPosition + Active->CommitSize - 1;
        CommitPostAligned         -= CommitPostAligned % Active->CommitSize;

        uint64_t CommitPostClamped = Minimum(CommitPostAligned, Active->Reserved);
        uint64_t CommitSize        = CommitPostClamped - Active->Committed;
        uint8_t *CommitPointer     = (uint8_t *)Active + Active->Committed;

        bool CommitResult = OSCommit(CommitPointer, CommitSize);
        if (!CommitResult)
        {
            return 0;
        }

        Active->Committed = CommitPostClamped;
    }

    void *Result = 0;
    if (Active->Committed >= PostPosition)
    {
        Result = (uint8_t *)Active + PrePosition;
        Active->Position = PostPosition;
    }

    return Result;
}

uint64_t
GetArenaPosition(memory_arena *Arena)
{
    memory_arena *Active = Arena->Current;
    return Active->BasePosition + Active->Position;
}

void
PopArenaTo(memory_arena *Arena, uint64_t Position)
{
    memory_arena *Active    = Arena->Current;
    uint64_t      PoppedPos = Maximum(Position, sizeof(memory_arena));

    for (memory_arena *Prev = 0; Active->BasePosition >= PoppedPos; Active = Prev)
    {
        Prev = Active->Prev;
        OSRelease(Active, Active->Reserved);
    }

    Arena->Current           = Active;
    Arena->Current->Position = PoppedPos - Arena->Current->BasePosition;
}

void
PopArena(memory_arena *Arena, uint64_t Amount)
{
    uint64_t OldPosition = GetArenaPosition(Arena);
    uint64_t NewPosition = OldPosition;

    if (Amount < OldPosition)
    {
        NewPosition = OldPosition - Amount;
    }

    PopArenaTo(Arena, NewPosition);
}

void
ClearArena(memory_arena *Arena)
{
    PopArenaTo(Arena, 0);
}

memory_region
EnterMemoryRegion(memory_arena *Arena)
{
    memory_region Result =
    {
        .Arena  = Arena,
        .Marker = GetArenaPosition(Arena),
    };

    return Result;
}

void
LeaveMemoryRegion(memory_region Region)
{
    PopArenaTo(Region.Arena, Region.Marker);
}

// ==============================================
// <Strings>
// ==============================================


byte_string
ByteString(uint8_t *Data, uint64_t Size)
{
    return (byte_string) {
        .Data = Data,
        .Size = Size,
    };
}


bool
IsValidByteString(byte_string String)
{
    bool Result = String.Data && String.Size;
    return Result;
}


bool
ByteStringCompare(byte_string A, byte_string B)
{
    bool Result = false;

    if (IsValidByteString(A) && IsValidByteString(B) && A.Size == B.Size)
    {
        uint64_t At = 0;
        while (At < A.Size && At < B.Size && A.Data[At] == B.Data[At])
        {
            ++At;
        }

        Result = At == A.Size;
    }

    return Result;
}


byte_string
ByteStringCopy(byte_string Input, memory_arena * Arena)
{
    byte_string Result = ByteString(0, 0);

    if (IsValidByteString(Input) && Arena)
    {
        Result.Data = PushArray(Arena, uint8_t, Input.Size);
        Result.Size = Input.Size;

        memcpy(Result.Data, Input.Data, Input.Size);
    }

    return Result;
}


byte_string
ReplaceFileName(byte_string Path, byte_string Name, memory_arena *Arena)
{
    byte_string Result = ByteString(0, 0);

    if (IsValidByteString(Path) && IsValidByteString(Name) || !Arena)
    {
        uint64_t Slash = Path.Size;
        while (Slash > 0)
        {
            uint8_t C = Path.Data[Slash - 1];
            if (C == '/' || C == '\\')
            {
                break;
            }

            --Slash;
        }

        Result.Size = Slash + Name.Size;
        Result.Data = PushArray(Arena, uint8_t, Result.Size);

        assert(IsValidByteString(Result));

        memcpy(Result.Data, Path.Data, Slash);
        memcpy(Result.Data + Slash, Name.Data, Name.Size);
    }

    return Result;
}


// ==============================================
// <Buffer>
// ==============================================


bool
IsBufferValid(buffer *Buffer)
{
    bool Result = (Buffer && Buffer->Data && Buffer->Size);
    return Result;
}


bool
IsBufferInBounds(buffer *Buffer)
{
    assert(IsBufferValid(Buffer));

    bool Result = Buffer->At < Buffer->Size;
    return Result;
}

uint8_t
GetNextToken(buffer *Buffer)
{
    assert(IsBufferValid(Buffer) && IsBufferInBounds(Buffer));

    uint8_t Result = Buffer->Data[Buffer->At++];
    return Result;
}


uint8_t
PeekBuffer(buffer *Buffer)
{
    assert(IsBufferValid(Buffer) && IsBufferInBounds(Buffer));

    uint8_t Result = Buffer->Data[Buffer->At];
    return Result;
}


bool
IsNewLine(uint8_t Token)
{
    bool Result = Token == '\n';
    return Result;
}


bool
IsWhiteSpace(uint8_t Token)
{
    bool Result = Token == ' ' || Token == '\t' || Token == '\r';
    return Result;
}


buffer
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


void
SkipWhitespaces(buffer *Buffer)
{
    assert(IsBufferValid(Buffer));

    while (IsBufferInBounds(Buffer) && IsWhiteSpace(PeekBuffer(Buffer)))
    {
        ++Buffer->At;
    }
}


float
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


float
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


float
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


byte_string
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


bool
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

    if (Result)
    {
        Buffer->At += String.Size;
    }

    return Result;
}