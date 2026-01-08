#pragma once

#include <stdint.h>

#include "utilities.h"
#include "engine/math/vector.h"

// ==============================================
// <IO>
// ==============================================


typedef struct
{
	uint32_t    Width;
	uint32_t    Height;
	uint32_t    BytesPerPixel;
	uint8_t    *Data;
	byte_string Path;
} loaded_texture;


typedef struct
{
	buffer          FileContent;
	loaded_texture *Output;
	uint32_t        Id;
} texture_to_load;

typedef struct platform_work_queue platform_work_queue;
void LoadTextureFromDisk(platform_work_queue *Queue, texture_to_load *ToLoad);

// ==============================================
// <Data>
// ==============================================


typedef struct
{
	vec3 Position;
	vec2 Texture;
	vec3 Normal;
} mesh_vertex_data;


typedef enum
{
	MaterialMap_Color     = 0,
	MaterialMap_Normal    = 1,
	MaterialMap_Roughness = 2,

	MaterialMap_Count     = 3,
} MaterialMap_Type;


typedef struct
{
	float          Shininess;
	float          Opacity;

	byte_string    Path;
	loaded_texture Textures[MaterialMap_Count];
} material_data;


typedef struct
{
	byte_string MaterialPath;
	uint32_t    VertexCount;
	uint32_t    VertexOffset;
} asset_submesh_data;


typedef struct
{
	asset_submesh_data *Submeshes;
	uint32_t            SubmeshCount;
	byte_string         Name;
	byte_string         Path;
} asset_mesh_data; 


// We should have an array of meshes not submeshes.
typedef struct
{
	mesh_vertex_data *Vertices;
	uint32_t          VertexCount;

	asset_mesh_data  *Meshes;
	uint32_t          MeshCount;

	material_data    *Materials;
	uint32_t          MaterialCount;
} asset_file_data;