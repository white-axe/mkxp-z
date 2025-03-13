#include "simpleColor.pgl.h"

void mkxpSimpleColorVS(float *_output, pgl_vec4 *_attribs, Shader_Builtins *builtins, void *_uniforms)
{
	struct SimpleColorVarying *output = (struct SimpleColorVarying *)_output;
	struct SimpleColorAttribs *attribs = (struct SimpleColorAttribs *)_attribs;
	struct SimpleColorUniforms *uniforms = (struct SimpleColorUniforms *)_uniforms;

	pgl_vec2 pos = add_vec2s(attribs->position, uniforms->translation);
	builtins->gl_Position = mult_mat4_vec4(uniforms->projMat, (pgl_vec4){pos.x, pos.y, 0, 1});
	output->v_texCoord = mult_vec2s(attribs->texCoord, uniforms->texSizeInv);
	output->v_color = attribs->color;
}

void mkxpSimpleColorFS(float *_input, Shader_Builtins *builtins, void *_uniforms)
{
	struct SimpleColorVarying *input = (struct SimpleColorVarying *)_input;

	builtins->gl_FragColor = input->v_color;
}
