#include "tilemapvx.pgl.h"

GLuint mkxp_pgl_get_texture2D(GLuint unit);

static const pgl_vec2 atAreaA = {9.0*32.0, 12.0*32.0};
static const float atAreaCX = 12.0*32.0;
static const float atAreaCW = 4.0*32.0;

void mkxpTilemapVXVS(float *_output, pgl_vec4 *_attribs, Shader_Builtins *builtins, void *_uniforms)
{
	struct TilemapVXVarying *output = (struct TilemapVXVarying *)_output;
	struct TilemapVXAttribs *attribs = (struct TilemapVXAttribs *)_attribs;
	struct TilemapVXUniforms *uniforms = (struct TilemapVXUniforms *)_uniforms;

	pgl_vec2 tex = attribs->texCoord;
	float pred;
	pred = tex.x <= atAreaA.x && tex.y <= atAreaA.y;
	tex.x += uniforms->aniOffset.x * pred;
	pred = tex.x >= atAreaCX && tex.x <= (atAreaCX+atAreaCW) && tex.y <= atAreaA.y;
	tex.y += uniforms->aniOffset.y * pred;
	builtins->gl_Position = mult_mat4_vec4(uniforms->projMat, (pgl_vec4){attribs->position.x + uniforms->translation.x, attribs->position.y + uniforms->translation.y, 0, 1});
	output->v_texCoord = mult_vec2s(tex, uniforms->texSizeInv);
}

void mkxpTilemapVXFS(float *_input, Shader_Builtins *builtins, void *_uniforms)
{
	struct TilemapVXVarying *input = (struct TilemapVXVarying *)_input;
	struct TilemapVXUniforms *uniforms = (struct TilemapVXUniforms *)_uniforms;

	builtins->gl_FragColor = mkxp_pgl_texture2D(mkxp_pgl_get_texture2D(uniforms->texture), input->v_texCoord.x, input->v_texCoord.y);
}
