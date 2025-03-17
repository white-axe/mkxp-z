#include "gray.pgl.h"

GLuint mkxp_pgl_get_texture2D(GLuint unit);

void mkxpGrayVS(float *_output, pgl_vec4 *_attribs, Shader_Builtins *builtins, void *_uniforms)
{
	struct GrayVarying *output = (struct GrayVarying *)_output;
	struct GrayAttribs *attribs = (struct GrayAttribs *)_attribs;
	struct GrayUniforms *uniforms = (struct GrayUniforms *)_uniforms;

	pgl_vec2 pos = add_vec2s(attribs->position, uniforms->translation);
	builtins->gl_Position = mult_mat4_vec4(uniforms->projMat, (pgl_vec4){pos.x, pos.y, 0, 1});
	output->v_texCoord = mult_vec2s(attribs->texCoord, uniforms->texSizeInv);
}

static const pgl_vec3 lumaF = {.299, .587, .114};

void mkxpGrayFS(float *_input, Shader_Builtins *builtins, void *_uniforms)
{
	struct GrayVarying *input = (struct GrayVarying *)_input;
	struct GrayUniforms *uniforms = (struct GrayUniforms *)_uniforms;

	pgl_vec4 frag = mkxp_pgl_texture2D(mkxp_pgl_get_texture2D(uniforms->texture), input->v_texCoord.x, input->v_texCoord.y);
	float luma = dot_vec3s((pgl_vec3){frag.x, frag.y, frag.z}, lumaF);
	pgl_vec3 gray = pgl_mix_vec3((pgl_vec3){frag.x, frag.y, frag.z}, (pgl_vec3){luma, luma, luma}, uniforms->gray);
	frag.x = gray.x;
	frag.y = gray.y;
	frag.z = gray.z;
	builtins->gl_FragColor = frag;
}
