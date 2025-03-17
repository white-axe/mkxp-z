#include "transSimple.pgl.h"

GLuint mkxp_pgl_get_texture2D(GLuint unit);

void mkxpTransSimpleVS(float *_output, pgl_vec4 *_attribs, Shader_Builtins *builtins, void *_uniforms)
{
	struct TransSimpleVarying *output = (struct TransSimpleVarying *)_output;
	struct TransSimpleAttribs *attribs = (struct TransSimpleAttribs *)_attribs;
	struct TransSimpleUniforms *uniforms = (struct TransSimpleUniforms *)_uniforms;

	pgl_vec2 pos = add_vec2s(attribs->position, uniforms->translation);
	builtins->gl_Position = mult_mat4_vec4(uniforms->projMat, (pgl_vec4){pos.x, pos.y, 0, 1});
	output->v_texCoord = mult_vec2s(attribs->texCoord, uniforms->texSizeInv);
}

void mkxpTransSimpleFS(float *_input, Shader_Builtins *builtins, void *_uniforms)
{
	struct TransSimpleVarying *input = (struct TransSimpleVarying *)_input;
	struct TransSimpleUniforms *uniforms = (struct TransSimpleUniforms *)_uniforms;

	pgl_vec4 newPixel = mkxp_pgl_texture2D(mkxp_pgl_get_texture2D(uniforms->currentScene), input->v_texCoord.x, input->v_texCoord.y);
	pgl_vec4 oldPixel = mkxp_pgl_texture2D(mkxp_pgl_get_texture2D(uniforms->frozenScene), input->v_texCoord.x, input->v_texCoord.y);
	builtins->gl_FragColor = pgl_mix_vec4(newPixel, oldPixel, uniforms->prog);
}
