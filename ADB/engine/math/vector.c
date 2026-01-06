#include <math.h>

#include "vector.h"

vec2 Vec2(float X, float Y)
{
	vec2 Result = (vec2)
	{
		.X = X,
		.Y = Y,
	};

	return Result;
}

vec3 Vec3(float X, float Y, float Z)
{
	vec3 Result = (vec3)
	{
		.X = X,
		.Y = Y,
		.Z = Z,
	};

	return Result;
}


vec3 Vec3Add(vec3 A, vec3 B)
{
    vec3 Result = {
        .X = A.X + B.X,
        .Y = A.Y + B.Y,
        .Z = A.Z + B.Z,
    };
    return Result;
}

vec3 Vec3Subtract(vec3 A, vec3 B)
{
    vec3 Result = {
        .X = A.X - B.X,
        .Y = A.Y - B.Y,
        .Z = A.Z - B.Z,
    };
    return Result;
}

vec3 Vec3Scale(vec3 V, float Scale)
{
    vec3 Result = {
        .X = V.X * Scale,
        .Y = V.Y * Scale,
        .Z = V.Z * Scale,
    };
    return Result;
}

float Vec3Dot(vec3 A, vec3 B)
{
    float Result = A.X * B.X + A.Y * B.Y + A.Z * B.Z;
    return Result;
}

float Vec3Length(vec3 V)
{
    float Result = sqrtf(Vec3Dot(V, V));
    return Result;
}

vec3 Vec3Normalize(vec3 V)
{
    float Length = Vec3Length(V);
    if (Length == 0.f)
        return V;

    vec3 Result = Vec3Scale(V, 1.f / Length);
    return Result;
}

vec3 Vec3Cross(vec3 A, vec3 B)
{
    vec3 Result = {
        .X = A.Y * B.Z - A.Z * B.Y,
        .Y = A.Z * B.X - A.X * B.Z,
        .Z = A.X * B.Y - A.Y * B.X,
    };
    return Result;
}