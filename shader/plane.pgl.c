#include "plane.pgl.h"

GLuint mkxp_pgl_get_texture2D(GLuint unit);

void mkxpPlaneVS(float *_output, pgl_vec4 *_attribs, Shader_Builtins *builtins, void *_uniforms)
{
	struct PlaneVarying *output = (struct PlaneVarying *)_output;
	struct PlaneAttribs *attribs = (struct PlaneAttribs *)_attribs;
	struct PlaneUniforms *uniforms = (struct PlaneUniforms *)_uniforms;

	pgl_vec2 pos = add_vec2s(attribs->position, uniforms->translation);
	builtins->gl_Position = mult_mat4_vec4(uniforms->projMat, (pgl_vec4){pos.x, pos.y, 0, 1});
	output->v_texCoord = mult_vec2s(attribs->texCoord, uniforms->texSizeInv);
}

static const pgl_vec3 lumaF = {.299, .587, .114};

void mkxpPlaneFS(float *_input, Shader_Builtins *builtins, void *_uniforms)
{
	struct PlaneVarying *input = (struct PlaneVarying *)_input;
	struct PlaneUniforms *uniforms = (struct PlaneUniforms *)_uniforms;

	pgl_vec4 frag = mkxp_pgl_texture2D(mkxp_pgl_get_texture2D(uniforms->texture), input->v_texCoord.x, input->v_texCoord.y);
	float luma = dot_vec3s((pgl_vec3){frag.x, frag.y, frag.z}, lumaF);
	pgl_vec3 gray = pgl_mix_vec3((pgl_vec3){frag.x, frag.y, frag.z}, (pgl_vec3){luma, luma, luma}, uniforms->tone.w);
	frag.x = gray.x;
	frag.y = gray.y;
	frag.z = gray.z;
	frag.x += uniforms->tone.x;
	frag.y += uniforms->tone.y;
	frag.z += uniforms->tone.z;
	frag.w += uniforms->opacity;
	pgl_vec3 color = pgl_mix_vec3((pgl_vec3){frag.x, frag.y, frag.z}, (pgl_vec3){uniforms->color.x, uniforms->color.y, uniforms->color.z}, uniforms->color.w);
	frag.x = color.x;
	frag.y = color.y;
	frag.z = color.z;
	pgl_vec3 flash = pgl_mix_vec3((pgl_vec3){frag.x, frag.y, frag.z}, (pgl_vec3){uniforms->flash.x, uniforms->flash.y, uniforms->flash.z}, uniforms->flash.w);
	frag.x = flash.x;
	frag.y = flash.y;
	frag.z = flash.z;
	builtins->gl_FragColor = frag;
}
