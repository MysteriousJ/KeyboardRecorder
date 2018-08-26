#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_DEFAULT_FONT
#define NK_IMPLEMENTATION
#include "nuklear/nuklear.h"
#include <string>

struct nk_vertex {
	float position[2];
	float uv[2];
	nk_byte col[4];
};

struct GUI
{
	nk_context ctx;
	nk_font_atlas atlas;
	nk_buffer cmds;
	nk_draw_null_texture null;
	GLuint font_tex;
};

bool doButton(nk_context* ctx, std::string label, bool yellow)
{
	bool result = false;

	if (yellow) {
		nk_style_push_style_item(ctx, &ctx->style.button.normal, nk_style_item_color(nk_rgb(255, 255, 0)));
		nk_style_push_style_item(ctx, &ctx->style.button.hover, nk_style_item_color(nk_rgb(255, 255, 0)));
		nk_style_push_style_item(ctx, &ctx->style.button.active, nk_style_item_color(nk_rgb(255, 255, 0)));
	}

	result = nk_button_label(ctx, label.c_str());

	if (yellow) {
		nk_style_pop_style_item(ctx);
		nk_style_pop_style_item(ctx);
		nk_style_pop_style_item(ctx);
	}

	return result;
}

void initGUI(GUI* gui)
{
	*gui = {0};
	nk_init_default(&gui->ctx, 0);
	nk_buffer_init_default(&gui->cmds);
	
	nk_font_atlas_init_default(&gui->atlas);
	nk_font_atlas_begin(&gui->atlas);

	const void *image;
	int w, h;
	image = nk_font_atlas_bake(&gui->atlas, &w, &h, NK_FONT_ATLAS_RGBA32);
	
	glGenTextures(1, &gui->font_tex);
	glBindTexture(GL_TEXTURE_2D, gui->font_tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
	
	nk_font_atlas_end(&gui->atlas, nk_handle_id((int)gui->font_tex), &gui->null);
	if (gui->atlas.default_font)
	{
		nk_style_set_font(&gui->ctx, &gui->atlas.default_font->handle);
	}
}

void renderNuklear(GUI* gui, int display_width, int display_height, enum nk_anti_aliasing AA)
{
	// Copied from sdl gl2 example

	/* setup global state */
	struct nk_vec2 scale;

	scale.x = 1;
	scale.y = 1;

	glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TRANSFORM_BIT);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_SCISSOR_TEST);
	glEnable(GL_BLEND);
	glEnable(GL_TEXTURE_2D);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	/* setup viewport/project */
	glViewport(0, 0, (GLsizei)display_width, (GLsizei)display_height);
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0.0f, display_width, display_height, 0.0f, -1.0f, 1.0f);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	{
		GLsizei vs = sizeof(struct nk_vertex);
		size_t vp = offsetof(nk_vertex, position);
		size_t vt = offsetof(nk_vertex, uv);
		size_t vc = offsetof(nk_vertex, col);

		/* convert from command queue into draw list and draw to screen */
		const struct nk_draw_command *cmd;
		const nk_draw_index *offset = NULL;
		struct nk_buffer vbuf, ebuf;

		/* fill converting configuration */
		struct nk_convert_config config;
		static const struct nk_draw_vertex_layout_element vertex_layout[] = {
			{NK_VERTEX_POSITION, NK_FORMAT_FLOAT, NK_OFFSETOF(nk_vertex, position)},
			{NK_VERTEX_TEXCOORD, NK_FORMAT_FLOAT, NK_OFFSETOF(nk_vertex, uv)},
			{NK_VERTEX_COLOR, NK_FORMAT_R8G8B8A8, NK_OFFSETOF(nk_vertex, col)},
			{NK_VERTEX_LAYOUT_END}
		};
		NK_MEMSET(&config, 0, sizeof(config));
		config.vertex_layout = vertex_layout;
		config.vertex_size = sizeof(nk_vertex);
		config.vertex_alignment = NK_ALIGNOF(nk_vertex);
		config.null = gui->null;
		config.circle_segment_count = 22;
		config.curve_segment_count = 22;
		config.arc_segment_count = 22;
		config.global_alpha = 1.0f;
		config.shape_AA = AA;
		config.line_AA = AA;

		/* convert shapes into vertexes */
		nk_buffer_init_default(&vbuf);
		nk_buffer_init_default(&ebuf);
		nk_convert(&gui->ctx, &gui->cmds, &vbuf, &ebuf, &config);

		/* setup vertex buffer pointer */
		{const void *vertices = nk_buffer_memory_const(&vbuf);
		glVertexPointer(2, GL_FLOAT, vs, (const void*)((const nk_byte*)vertices + vp));
		glTexCoordPointer(2, GL_FLOAT, vs, (const void*)((const nk_byte*)vertices + vt));
		glColorPointer(4, GL_UNSIGNED_BYTE, vs, (const void*)((const nk_byte*)vertices + vc)); }

		/* iterate over and execute each draw command */
		offset = (const nk_draw_index*)nk_buffer_memory_const(&ebuf);
		nk_draw_foreach(cmd, &gui->ctx, &gui->cmds)
		{
			if (!cmd->elem_count) continue;
			glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->texture.id);
			glScissor(
				(GLint)(cmd->clip_rect.x * scale.x),
				(GLint)((display_height - (GLint)(cmd->clip_rect.y + cmd->clip_rect.h)) * scale.y),
				(GLint)(cmd->clip_rect.w * scale.x),
				(GLint)(cmd->clip_rect.h * scale.y));
			glDrawElements(GL_TRIANGLES, (GLsizei)cmd->elem_count, GL_UNSIGNED_SHORT, offset);
			offset += cmd->elem_count;
		}
		nk_clear(&gui->ctx);
		nk_buffer_free(&vbuf);
		nk_buffer_free(&ebuf);
	}

	/* default OpenGL state */
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_SCISSOR_TEST);
	glDisable(GL_BLEND);
	glDisable(GL_TEXTURE_2D);

	glBindTexture(GL_TEXTURE_2D, 0);
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glPopAttrib();
}

void render(GUI* gui, int displayWidth, int displayHeight)
{
	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT);
	renderNuklear(gui, displayWidth, displayHeight, NK_ANTI_ALIASING_ON);
}