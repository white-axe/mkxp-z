#include "simpleAlpha.pgl.h"

void mkxpSimpleAlphaVS(float *_output, pgl_vec4 *_attribs, Shader_Builtins *builtins, void *_uniforms)
{
	struct SimpleAlphaVarying *output = (struct SimpleAlphaVarying *)_output;
	struct SimpleAlphaAttribs *attribs = (struct SimpleAlphaAttribs *)_attribs;
	struct SimpleAlphaUniforms *uniforms = (struct SimpleAlphaUniforms *)_uniforms;

	pgl_vec2 pos = add_vec2s(attribs->position, uniforms->translation);
	builtins->gl_Position = mult_mat4_vec4(uniforms->projMat, (pgl_vec4){pos.x, pos.y, 0, 1});
	output->v_texCoord = mult_vec2s(attribs->texCoord, uniforms->texSizeInv);
	output->v_color = attribs->color;
}

void mkxpSimpleAlphaFS(float *_input, Shader_Builtins *builtins, void *_uniforms)
{
	struct SimpleAlphaVarying *input = (struct SimpleAlphaVarying *)_input;
	struct SimpleAlphaUniforms *uniforms = (struct SimpleAlphaUniforms *)_uniforms;

	builtins->gl_FragColor = mkxp_pgl_texture2D(uniforms->texture, input->v_texCoord.x, input->v_texCoord.y);
	builtins->gl_FragColor.w *= input->v_color.w;
}
