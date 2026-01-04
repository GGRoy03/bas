#pragma once

#include "utilities.h"
#include "engine/rendering/assets.h"


// ONCE WE HAVE SOME INTERMEDIARY FORMAT WE CAN STOP EXPOSING THESE TYPES.


mesh_data ParseObjFromFile(byte_string Path);