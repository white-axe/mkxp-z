#include "flatColor.pgl.h"

void mkxpFlatColorVS(float *_output, pgl_vec4 *_attribs, Shader_Builtins *builtins, void *_uniforms)
{
	struct FlatColorUniforms *uniforms = (struct FlatColorUniforms *)_uniforms;
	struct FlatColorAttribs *attribs = (struct FlatColorAttribs *)_attribs;

	builtins->gl_Position = mult_mat4_vec4(uniforms->projMat, (pgl_vec4){attribs->position.x, attribs->position.y, 0, 1});
}

void mkxpFlatColorFS(float *_input, Shader_Builtins *builtins, void *_uniforms)
{
	struct FlatColorUniforms *uniforms = (struct FlatColorUniforms *)_uniforms;

	builtins->gl_FragColor = uniforms->color;
}
