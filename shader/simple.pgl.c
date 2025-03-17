#include "simple.pgl.h"

GLuint mkxp_pgl_get_texture2D(GLuint unit);

void mkxpSimpleVS(float *_output, pgl_vec4 *_attribs, Shader_Builtins *builtins, void *_uniforms)
{
	struct SimpleVarying *output = (struct SimpleVarying *)_output;
	struct SimpleAttribs *attribs = (struct SimpleAttribs *)_attribs;
	struct SimpleUniforms *uniforms = (struct SimpleUniforms *)_uniforms;

	pgl_vec2 pos = add_vec2s(attribs->position, uniforms->translation);
	builtins->gl_Position = mult_mat4_vec4(uniforms->projMat, (pgl_vec4){pos.x, pos.y, 0, 1});
	output->v_texCoord = mult_vec2s(attribs->texCoord, uniforms->texSizeInv);
}

void mkxpSimpleFS(float *_input, Shader_Builtins *builtins, void *_uniforms)
{
	struct SimpleVarying *input = (struct SimpleVarying *)_input;
	struct SimpleUniforms *uniforms = (struct SimpleUniforms *)_uniforms;

	builtins->gl_FragColor = mkxp_pgl_texture2D(mkxp_pgl_get_texture2D(uniforms->texture), input->v_texCoord.x, input->v_texCoord.y);
}
