#pragma once

#include "utilities.h"
#include "engine/rendering/assets.h"


typedef struct engine_memory engine_memory;
asset_file_data ParseObjFromFile(byte_string Path, engine_memory *EngineMemory);