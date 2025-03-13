#include "hue.pgl.h"

void mkxpHueVS(float *_output, pgl_vec4 *_attribs, Shader_Builtins *builtins, void *_uniforms)
{
	struct HueVarying *output = (struct HueVarying *)_output;
	struct HueAttribs *attribs = (struct HueAttribs *)_attribs;
	struct HueUniforms *uniforms = (struct HueUniforms *)_uniforms;

	pgl_vec2 pos = add_vec2s(attribs->position, uniforms->translation);
	builtins->gl_Position = mult_mat4_vec4(uniforms->projMat, (pgl_vec4){pos.x, pos.y, 0, 1});
	output->v_texCoord = mult_vec2s(attribs->texCoord, uniforms->texSizeInv);
}

static pgl_vec3 rgb2hsv(pgl_vec3 c)
{
	const pgl_vec4 K = (pgl_vec4){0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0};
	pgl_vec4 p = pgl_mix_vec4((pgl_vec4){c.z, c.y, K.w, K.z}, (pgl_vec4){c.y, c.z, K.x, K.y}, c.z <= c.y);
	pgl_vec4 q = pgl_mix_vec4((pgl_vec4){p.x, p.y, p.w, c.x}, (pgl_vec4){c.x, p.y, p.z, p.x}, p.x <= c.x);
	float d = q.x - minf(q.w, q.y);
	const float eps = 1.0e-10;
	return (pgl_vec3){fabs(q.z + (q.w - q.y) / (6.0 * d + eps)), d / (q.x + eps), q.x};
}

static pgl_vec3 hsv2rgb(pgl_vec3 c)
{
	const pgl_vec4 K = (pgl_vec4){1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0};
	pgl_vec3 p = fabsf_vec3(sub_vec3s(mult_vec3s(fractf_vec3(add_vec3s((pgl_vec3){c.x, c.x, c.x}, (pgl_vec3){K.x, K.y, K.z})), (pgl_vec3){6.0, 6.0, 6.0}), (pgl_vec3){K.w, K.w, K.w}));
	return mult_vec3s((pgl_vec3){c.z, c.z, c.z}, pgl_mix_vec3((pgl_vec3){K.x, K.x, K.x}, pgl_clamp_vec3(sub_vec3s(p, (pgl_vec3){K.x, K.x, K.x}), 0.0, 1.0), c.y));
}

void mkxpHueFS(float *_input, Shader_Builtins *builtins, void *_uniforms)
{
	struct HueVarying *input = (struct HueVarying *)_input;
	struct HueUniforms *uniforms = (struct HueUniforms *)_uniforms;

	pgl_vec4 color = mkxp_pgl_texture2D(uniforms->texture, input->v_texCoord.x, input->v_texCoord.y);
	pgl_vec3 hsv = rgb2hsv((pgl_vec3){color.x, color.y, color.z});
	hsv.x += uniforms->hueAdjust;
	pgl_vec3 rgb = hsv2rgb(hsv);
	color.x = rgb.x;
	color.y = rgb.y;
	color.z = rgb.z;
	builtins->gl_FragColor = color;
}
