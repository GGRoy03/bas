#include <stdbool.h>

#include "utilities.h"
#include "engine.h"
#include "rendering/renderer.h"
#include "parsers/parser_obj.h"

typedef struct renderer renderer;

typedef struct
{
	bool IsInitialized;
} engine_state;

void UpdateEngine(int WindowWidth, int WindowHeight, renderer *Renderer)
{
	static engine_state Engine;

	if (!Engine.IsInitialized)
	{
		// TODO: From the OBJ mesh list populate the renderer buffer.
		// TODO: Render the tree! (Triangulation, Material, Winding Order?)

		ParseObjFromFile(ByteStringLiteral("data/Lowpoly_tree_sample.obj"));

		Engine.IsInitialized = true;
	}

	clear_color Color = (clear_color){.R = 0.f, .G = 0.f, .B = 0.f, .A = 1.f};
	RendererStartFrame(Color, Renderer);

	RendererFlushFrame(Renderer);
}