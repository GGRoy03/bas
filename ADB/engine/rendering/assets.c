#include <assert.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_FLIP_VERTICALLY_ON_LOAD 1
#include "third_party/stb_image.h"

#include "assets.h"

void 
LoadTextureFromDisk(platform_work_queue *Queue, texture_to_load *ToLoad)
{
	assert(Queue);
	assert(ToLoad);

	stbi_set_flip_vertically_on_load(1);

	// TODO:
	// 1) Instead of handling the file read on the "main" thread we could ask it to query the OS for the
	//    file size and then allocate memory from which we can read the file into.
	// 2) Write our own texture loader? How hard is it to handle the basic formats? (JPEG, PNG)

	if (ToLoad->Output && IsBufferValid(&ToLoad->FileContent))
	{
		loaded_texture *Texture = ToLoad->Output;

		// Currently we force to RGBA. Unsure if it's the correct choice, but we do this for simplicity.

		Texture->Data          = stbi_load_from_memory(ToLoad->FileContent.Data, ToLoad->FileContent.Size, &Texture->Width, &Texture->Height, &Texture->BytesPerPixel, 4);
		Texture->BytesPerPixel = 4;
	}
}