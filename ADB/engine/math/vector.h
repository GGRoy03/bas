#pragma once

typedef struct
{
    union
    {
        struct
        {
            float X, Y;
        };
        float AsBuffer[2];
    };
} vec2;

typedef struct
{
    union
    {
        struct
        {
            float X, Y, Z;
        };
        float AsBuffer[3];
    };
} vec3;

vec2 Vec2  (float X, float Y);
vec3 Vec3  (float X, float Y, float Z);