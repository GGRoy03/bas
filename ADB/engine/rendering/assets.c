#include <assert.h>

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

#include "assets.h"
#include "platform/platform.h"

void 
LoadTextureFromDisk(platform_work_queue *Queue, texture_to_load *ToLoad)
{
	assert(Queue);
	assert(ToLoad);

	// TODO:
	// 1) Instead of handling the file read on the "main" thread we could ask it to query the OS for the
	//    file size and then allocate memory from which we can read the file into.
	// 2) Write our own texture loader?

	if (ToLoad->Output && IsBufferValid(&ToLoad->FileContent))
	{
		loaded_texture *Texture = ToLoad->Output;

		Texture->Data = stbi_load_from_memory(ToLoad->FileContent.Data, ToLoad->FileContent.Size, &Texture->Width, &Texture->Height, &Texture->BytesPerPixel, 0);
		if (Texture->Data)
		{
			assert("");
		}
	}
}