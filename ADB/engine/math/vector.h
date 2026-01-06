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

vec3  Vec3Add        (vec3 A, vec3 B);
vec3  Vec3Subtract   (vec3 A, vec3 B);
vec3  Vec3Scale      (vec3 V, float Scale);
float Vec3Dot        (vec3 A, vec3 B);
float Vec3Length     (vec3 V);
vec3  Vec3Normalize  (vec3 V);
vec3  Vec3Cross      (vec3 A, vec3 B);