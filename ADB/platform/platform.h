#pragma once

// ==============================================
// <Memory>
// ==============================================

void *OSReserve  (size_t Size);
bool  OSCommit   (void *At, size_t Size);
void  OSRelease  (void *At, size_t Size);