#include <stdint.h>

#include "utilities.h" // Arenas
#include "renderer.h"  // Implementation File

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
            else
            {
                BatchList->Last->Next = Node; // Warning but the logic should be sound?
                BatchList->Last = Node;
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