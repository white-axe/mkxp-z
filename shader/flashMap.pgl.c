#include "flashMap.pgl.h"

void mkxpFlashMapVS(float *_output, pgl_vec4 *_attribs, Shader_Builtins *builtins, void *_uniforms)
{
	struct FlashMapVarying *output = (struct FlashMapVarying *)_output;
	struct FlashMapAttribs *attribs = (struct FlashMapAttribs *)_attribs;
	struct FlashMapUniforms *uniforms = (struct FlashMapUniforms *)_uniforms;

	pgl_vec2 pos = add_vec2s(attribs->position, uniforms->translation);
	builtins->gl_Position = mult_mat4_vec4(uniforms->projMat, (pgl_vec4){pos.x, pos.y, 0, 1});
	output->v_texCoord = mult_vec2s(attribs->texCoord, uniforms->texSizeInv);
	output->v_color = attribs->color;
}

void mkxpFlashMapFS(float *_input, Shader_Builtins *builtins, void *_uniforms)
{
	struct FlashMapVarying *input = (struct FlashMapVarying *)_input;
	struct FlashMapUniforms *uniforms = (struct FlashMapUniforms *)_uniforms;

	builtins->gl_FragColor = (pgl_vec4){input->v_color.x * uniforms->alpha, input->v_color.y * uniforms->alpha, input->v_color.z * uniforms->alpha, 1};
}
