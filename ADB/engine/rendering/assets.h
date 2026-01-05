#pragma once

#include <stdint.h>

#include "utilities.h"
#include "engine/math/vector.h"

// ==============================================
// <IO>
// ==============================================


typedef struct
{
	uint32_t Width;
	uint32_t Height;
	uint32_t BytesPerPixel;
	uint8_t *Data;
} loaded_texture;


typedef struct
{
	buffer          FileContent;
	loaded_texture *Output;
	uint32_t        Id;
} texture_to_load;

typedef struct platform_work_queue platform_work_queue;
void LoadTextureFromDisk(platform_work_queue *Queue, texture_to_load *ToLoad);

// Most of this should directly relate to some sort of assert_file_content

typedef struct
{
	vec3 Position;
	vec2 Texture;
	vec3 Normal;
} mesh_vertex_data;

typedef struct
{
	float       Shininess;
	float       Opacity;

	byte_string    Name;
	loaded_texture ColorTexture;
	loaded_texture NormalTexture;
	loaded_texture RoughnessTexture;
} material_data;

typedef struct
{
	byte_string Name;
	uint32_t    MaterialId;
	uint32_t    VertexCount;
	uint32_t    VertexOffset;
} submesh_data;

typedef struct
{
	mesh_vertex_data *Vertices;
	uint32_t          VertexCount;

	submesh_data     *Submeshes;
	uint32_t          SubmeshCount;

	material_data    *Materials;
	uint32_t          MaterialCount;
} asset_file_data;