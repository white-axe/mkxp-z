#include "simpleMatrix.pgl.h"

void mkxpSimpleMatrixVS(float *_output, pgl_vec4 *_attribs, Shader_Builtins *builtins, void *_uniforms)
{
	struct SimpleMatrixVarying *output = (struct SimpleMatrixVarying *)_output;
	struct SimpleMatrixAttribs *attribs = (struct SimpleMatrixAttribs *)_attribs;
	struct SimpleMatrixUniforms *uniforms = (struct SimpleMatrixUniforms *)_uniforms;

	builtins->gl_Position = mult_mat4_vec4(uniforms->projMat, mult_mat4_vec4(uniforms->matrix, (pgl_vec4){attribs->position.x, attribs->position.y, 0, 1}));
	output->v_texCoord = mult_vec2s(attribs->texCoord, uniforms->texSizeInv);
	output->v_color = attribs->color;
}

void mkxpSimpleMatrixFS(float *_input, Shader_Builtins *builtins, void *_uniforms)
{
	struct SimpleMatrixVarying *input = (struct SimpleMatrixVarying *)_input;
	struct SimpleMatrixUniforms *uniforms = (struct SimpleMatrixUniforms *)_uniforms;

	builtins->gl_FragColor = mkxp_pgl_texture2D(uniforms->texture, input->v_texCoord.x, input->v_texCoord.y);
	builtins->gl_FragColor.w *= input->v_color.w;
}
