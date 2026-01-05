#pragma once


// ==============================================
// <Threading>
// ==============================================


typedef struct platform_work_queue platform_work_queue;
typedef void platform_work_queue_callback(platform_work_queue *Queue, void *Data);
typedef void platform_add_entry(platform_work_queue *Queue, platform_work_queue_callback *Callback, void *Data);
typedef void platform_complete_work(platform_work_queue *Queue);

// ==============================================
// <Memory>
// ==============================================

typedef struct memory_arena memory_arena;
typedef struct engine_memory
{
	memory_arena           *StateMemory;
	memory_arena           *FrameMemory;
	platform_add_entry     *AddEntry;
	platform_complete_work *CompleteWork;
	platform_work_queue    *WorkQueue;
} engine_memory;

void *OSReserve(size_t Size);
bool  OSCommit(void *At, size_t Size);
void  OSRelease(void *At, size_t Size);