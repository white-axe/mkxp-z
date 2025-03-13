#include "tilemap.pgl.h"

#define tileW 32.0
#define tileH 32.0
#define autotileW (3.0*tileW)
#define autotileH (4.0*tileW)
#define atAreaW (autotileW)
#define atAreaH (autotileH*nAutotiles)
#define atAniOffsetX (3.0*tileW)
#define atAniOffsetY (tileH)

void mkxpTilemapVS(float *_output, pgl_vec4 *_attribs, Shader_Builtins *builtins, void *_uniforms)
{
	struct TilemapVarying *output = (struct TilemapVarying *)_output;
	struct TilemapAttribs *attribs = (struct TilemapAttribs *)_attribs;
	struct TilemapUniforms *uniforms = (struct TilemapUniforms *)_uniforms;

	pgl_vec2 tex = attribs->texCoord;
	int atIndex = tex.y / autotileH;
	int pred = tex.x <= atAreaW && tex.y <= atAreaH;
	int frame = uniforms->aniIndex - uniforms->atFrames[atIndex] * (uniforms->aniIndex / uniforms->atFrames[atIndex]);
	int row = frame / 8;
	int col = frame - 8 * row;
	tex.x += atAniOffsetX * (float)(col * pred);
	tex.y += atAniOffsetY * (float)(row * pred);
	pgl_vec2 pos = add_vec2s(attribs->position, uniforms->translation);
	builtins->gl_Position = mult_mat4_vec4(uniforms->projMat, (pgl_vec4){pos.x, pos.y, 0, 1});
	output->v_texCoord = mult_vec2s(tex, uniforms->texSizeInv);
}

static const pgl_vec3 lumaF = {.299, .587, .114};

void mkxpTilemapFS(float *_input, Shader_Builtins *builtins, void *_uniforms)
{
	struct TilemapVarying *input = (struct TilemapVarying *)_input;
	struct TilemapUniforms *uniforms = (struct TilemapUniforms *)_uniforms;

	pgl_vec4 frag = mkxp_pgl_texture2D(uniforms->texture, input->v_texCoord.x, input->v_texCoord.y);
	float luma = dot_vec3s((pgl_vec3){frag.x, frag.y, frag.z}, lumaF);
	pgl_vec3 gray = pgl_mix_vec3((pgl_vec3){frag.x, frag.y, frag.z}, (pgl_vec3){luma, luma, luma}, uniforms->tone.w);
	frag.x = gray.x;
	frag.y = gray.y;
	frag.z = gray.z;
	frag.x += uniforms->tone.x;
	frag.y += uniforms->tone.y;
	frag.z += uniforms->tone.z;
	frag.w += uniforms->opacity;
	pgl_vec3 color = pgl_mix_vec3((pgl_vec3){frag.x, frag.y, frag.z}, (pgl_vec3){uniforms->color.x, uniforms->color.y, uniforms->color.z}, uniforms->color.w);
	frag.x = color.x;
	frag.y = color.y;
	frag.z = color.z;
	builtins->gl_FragColor = frag;
}
