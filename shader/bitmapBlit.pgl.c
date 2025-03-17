#include "bitmapBlit.pgl.h"

GLuint mkxp_pgl_get_texture2D(GLuint unit);

void mkxpBitmapBlitVS(float *_output, pgl_vec4 *_attribs, Shader_Builtins *builtins, void *_uniforms)
{
	struct BitmapBlitVarying *output = (struct BitmapBlitVarying *)_output;
	struct BitmapBlitAttribs *attribs = (struct BitmapBlitAttribs *)_attribs;
	struct BitmapBlitUniforms *uniforms = (struct BitmapBlitUniforms *)_uniforms;

	pgl_vec2 pos = add_vec2s(attribs->position, uniforms->translation);
	builtins->gl_Position = mult_mat4_vec4(uniforms->projMat, (pgl_vec4){pos.x, pos.y, 0, 1});
	output->v_texCoord = mult_vec2s(attribs->texCoord, uniforms->texSizeInv);
}

void mkxpBitmapBlitFS(float *_input, Shader_Builtins *builtins, void *_uniforms)
{
	struct BitmapBlitVarying *input = (struct BitmapBlitVarying *)_input;
	struct BitmapBlitUniforms *uniforms = (struct BitmapBlitUniforms *)_uniforms;

	pgl_vec2 coor = input->v_texCoord;
	pgl_vec2 dstCoor = mult_vec2s(sub_vec2s(coor, (pgl_vec2){uniforms->subRect.x, uniforms->subRect.y}), (pgl_vec2){uniforms->subRect.z, uniforms->subRect.w});
	pgl_vec4 srcFrag = mkxp_pgl_texture2D(mkxp_pgl_get_texture2D(uniforms->source), coor.x, coor.y);
	pgl_vec4 dstFrag = mkxp_pgl_texture2D(mkxp_pgl_get_texture2D(uniforms->destination), dstCoor.x, dstCoor.y);
	pgl_vec4 resFrag;
	float co1 = srcFrag.w * uniforms->opacity;
	float co2 = dstFrag.w * (1.0 - co1);
	resFrag.w = co1 + co2;
	if (resFrag.w == 0.0) {
		resFrag.x = srcFrag.x;
		resFrag.y = srcFrag.y;
		resFrag.z = srcFrag.z;
	} else {
		pgl_vec3 frag = add_vec3s((pgl_vec3){co1*srcFrag.x, co1*srcFrag.y, co1*srcFrag.z}, (pgl_vec3){co2*dstFrag.x, co2*dstFrag.y, co2*dstFrag.z});
		resFrag.x = frag.x / resFrag.w;
		resFrag.y = frag.y / resFrag.w;
		resFrag.z = frag.z / resFrag.w;
	}
	builtins->gl_FragColor = resFrag;
}
