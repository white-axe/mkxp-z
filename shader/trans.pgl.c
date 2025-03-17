#include "trans.pgl.h"

GLuint mkxp_pgl_get_texture2D(GLuint unit);

void mkxpTransVS(float *_output, pgl_vec4 *_attribs, Shader_Builtins *builtins, void *_uniforms)
{
	struct TransVarying *output = (struct TransVarying *)_output;
	struct TransAttribs *attribs = (struct TransAttribs *)_attribs;
	struct TransUniforms *uniforms = (struct TransUniforms *)_uniforms;

	pgl_vec2 pos = add_vec2s(attribs->position, uniforms->translation);
	builtins->gl_Position = mult_mat4_vec4(uniforms->projMat, (pgl_vec4){pos.x, pos.y, 0, 1});
	output->v_texCoord = mult_vec2s(attribs->texCoord, uniforms->texSizeInv);
}

void mkxpTransFS(float *_input, Shader_Builtins *builtins, void *_uniforms)
{
	struct TransVarying *input = (struct TransVarying *)_input;
	struct TransUniforms *uniforms = (struct TransUniforms *)_uniforms;

	float transV = mkxp_pgl_texture2D(mkxp_pgl_get_texture2D(uniforms->transMap), input->v_texCoord.x, input->v_texCoord.y).x;
	float cTransV = pgl_clamp(transV, uniforms->prog, uniforms->prog + uniforms->vague);
	float alpha = (cTransV - uniforms->prog) / uniforms->vague;
	pgl_vec4 newFrag = mkxp_pgl_texture2D(mkxp_pgl_get_texture2D(uniforms->currentScene), input->v_texCoord.x, input->v_texCoord.y);
	pgl_vec4 oldFrag = mkxp_pgl_texture2D(mkxp_pgl_get_texture2D(uniforms->frozenScene), input->v_texCoord.x, input->v_texCoord.y);
	builtins->gl_FragColor = pgl_mix_vec4(newFrag, oldFrag, alpha);
}
