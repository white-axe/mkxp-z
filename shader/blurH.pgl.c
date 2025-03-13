#include "blurH.pgl.h"

void mkxpBlurHVS(float *_output, pgl_vec4 *_attribs, Shader_Builtins *builtins, void *_uniforms)
{
	struct BlurHVarying *output = (struct BlurHVarying *)_output;
	struct BlurHAttribs *attribs = (struct BlurHAttribs *)_attribs;
	struct BlurHUniforms *uniforms = (struct BlurHUniforms *)_uniforms;

	builtins->gl_Position = mult_mat4_vec4(uniforms->projMat, (pgl_vec4){attribs->position.x, attribs->position.y, 0, 1});
	output->v_texCoord = mult_vec2s(attribs->texCoord, uniforms->texSizeInv);
	output->v_blurCoord[0] = mult_vec2s((pgl_vec2){attribs->texCoord.x-1.0f, attribs->texCoord.y}, uniforms->texSizeInv);
	output->v_blurCoord[1] = mult_vec2s((pgl_vec2){attribs->texCoord.x+1.0f, attribs->texCoord.y}, uniforms->texSizeInv);
}

void mkxpBlurHFS(float *_input, Shader_Builtins *builtins, void *_uniforms)
{
	struct BlurHVarying *input = (struct BlurHVarying *)_input;
	struct BlurHUniforms *uniforms = (struct BlurHUniforms *)_uniforms;

	pgl_vec4 frag = {0, 0, 0, 0};
	frag = add_vec4s(frag, mkxp_pgl_texture2D(uniforms->texture, input->v_texCoord.x, input->v_texCoord.y));
	frag = add_vec4s(frag, mkxp_pgl_texture2D(uniforms->texture, input->v_blurCoord[0].x, input->v_blurCoord[0].y));
	frag = add_vec4s(frag, mkxp_pgl_texture2D(uniforms->texture, input->v_blurCoord[1].x, input->v_blurCoord[1].y));
	builtins->gl_FragColor = div_vec4s(frag, (pgl_vec4){3.0, 3.0, 3.0, 3.0});
}
