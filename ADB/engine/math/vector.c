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