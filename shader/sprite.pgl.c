#include "sprite.pgl.h"

GLuint mkxp_pgl_get_texture2D(GLuint unit);

void mkxpSpriteVS(float *_output, pgl_vec4 *_attribs, Shader_Builtins *builtins, void *_uniforms)
{
	struct SpriteVarying *output = (struct SpriteVarying *)_output;
	struct SpriteAttribs *attribs = (struct SpriteAttribs *)_attribs;
	struct SpriteUniforms *uniforms = (struct SpriteUniforms *)_uniforms;

	builtins->gl_Position = mult_mat4_vec4(uniforms->projMat, mult_mat4_vec4(uniforms->spriteMat, (pgl_vec4){attribs->position.x, attribs->position.y, 0, 1}));
	output->v_texCoord = mult_vec2s(attribs->texCoord, uniforms->texSizeInv);
	if (uniforms->renderPattern) {
		if (uniforms->patternTile) {
			pgl_vec2 scroll = mult_vec2s(uniforms->patternScroll, div_vec2s(uniforms->patternSizeInv, uniforms->texSizeInv));
			output->v_patCoord = sub_vec2s(mult_vec2s(attribs->texCoord, div_vec2s(uniforms->patternSizeInv, uniforms->patternZoom)), mult_vec2s(scroll, uniforms->patternSizeInv));
		} else {
			pgl_vec2 scroll = mult_vec2s(uniforms->patternScroll, div_vec2s(uniforms->patternSizeInv, uniforms->texSizeInv));
			output->v_patCoord = sub_vec2s(mult_vec2s(attribs->texCoord, div_vec2s(uniforms->texSizeInv, uniforms->patternZoom)), mult_vec2s(scroll, uniforms->texSizeInv));
		}
	}
}

static const pgl_vec3 lumaF = {.299, .587, .114};
static const pgl_vec2 repeat = {1, 1};

static pgl_vec3 blendNormal(pgl_vec3 base, pgl_vec3 blend) {
	return blend;
}

static pgl_vec3 blendNormalOpacity(pgl_vec3 base, pgl_vec3 blend, float opacity) {
	return add_vec3s(mult_vec3s(blendNormal(base, blend), (pgl_vec3){opacity, opacity, opacity}), mult_vec3s(base, (pgl_vec3){1.0f - opacity, 1.0f - opacity, 1.0f - opacity}));
}

static pgl_vec3 blendAdd(pgl_vec3 base, pgl_vec3 blend) {
	return minf_vec3(add_vec3s(base, blend), (pgl_vec3){1.0, 1.0, 1.0});
}

static pgl_vec3 blendAddOpacity(pgl_vec3 base, pgl_vec3 blend, float opacity) {
	return add_vec3s(mult_vec3s(blendAdd(base, blend), (pgl_vec3){opacity, opacity, opacity}), mult_vec3s(base, (pgl_vec3){1.0f - opacity, 1.0f - opacity, 1.0f - opacity}));
}

static pgl_vec3 blendSubtract(pgl_vec3 base, pgl_vec3 blend) {
	return maxf_vec3(sub_vec3s(base, blend), (pgl_vec3){0.0, 0.0, 0.0});
}

static pgl_vec3 blendSubtractOpacity(pgl_vec3 base, pgl_vec3 blend, float opacity) {
	return add_vec3s(mult_vec3s(blendSubtract(base, blend), (pgl_vec3){opacity, opacity, opacity}), mult_vec3s(base, (pgl_vec3){1.0f - opacity, 1.0f - opacity, 1.0f - opacity}));
}

void mkxpSpriteFS(float *_input, Shader_Builtins *builtins, void *_uniforms)
{
	struct SpriteVarying *input = (struct SpriteVarying *)_input;
	struct SpriteUniforms *uniforms = (struct SpriteUniforms *)_uniforms;

	pgl_vec4 frag = mkxp_pgl_texture2D(mkxp_pgl_get_texture2D(uniforms->texture), input->v_texCoord.x, input->v_texCoord.y);
	if (uniforms->renderPattern) {
		pgl_vec2 patCoordRepeat = modulusf_vec2(input->v_patCoord, repeat);
		pgl_vec4 pattfrag = mkxp_pgl_texture2D(mkxp_pgl_get_texture2D(uniforms->pattern), patCoordRepeat.x, patCoordRepeat.y);
		if (uniforms->patternBlendType == 1) {
			pgl_vec3 blended = blendAddOpacity((pgl_vec3){frag.x, frag.y, frag.z}, (pgl_vec3){pattfrag.x, pattfrag.y, pattfrag.z}, pattfrag.w * uniforms->patternOpacity);
			frag.x = blended.x;
			frag.y = blended.y;
			frag.z = blended.z;
		}
		else if (uniforms->patternBlendType == 2) {
			pgl_vec3 blended = blendSubtractOpacity((pgl_vec3){frag.x, frag.y, frag.z}, (pgl_vec3){pattfrag.x, pattfrag.y, pattfrag.z}, pattfrag.w * uniforms->patternOpacity);
			frag.x = blended.x;
			frag.y = blended.y;
			frag.z = blended.z;
		}
		else {
			pgl_vec3 blended = blendNormalOpacity((pgl_vec3){frag.x, frag.y, frag.z}, (pgl_vec3){pattfrag.x, pattfrag.y, pattfrag.z}, pattfrag.w * uniforms->patternOpacity);
			frag.x = blended.x;
			frag.y = blended.y;
			frag.z = blended.z;
		}
	}
	float luma = dot_vec3s((pgl_vec3){frag.x, frag.y, frag.z}, lumaF);
	pgl_vec3 gray = pgl_mix_vec3((pgl_vec3){frag.x, frag.y, frag.z}, (pgl_vec3){luma, luma, luma}, uniforms->tone.w);
	frag.x = gray.x;
	frag.y = gray.y;
	frag.z = gray.z;
	frag.x += uniforms->tone.x;
	frag.y += uniforms->tone.y;
	frag.z += uniforms->tone.z;
	frag.w *= uniforms->opacity;
	pgl_vec3 color = pgl_mix_vec3((pgl_vec3){frag.x, frag.y, frag.z}, (pgl_vec3){uniforms->color.x, uniforms->color.y, uniforms->color.z}, uniforms->color.w);
	frag.x = color.x;
	frag.y = color.y;
	frag.z = color.z;
	if (uniforms->invert) {
		frag.x = 1.0f - frag.x;
		frag.y = 1.0f - frag.y;
		frag.z = 1.0f - frag.z;
	}
	float underBush = input->v_texCoord.y < uniforms->bushDepth;
	frag.w *= pgl_clamp(uniforms->bushOpacity + underBush, 0.0, 1.0);
	builtins->gl_FragColor = frag;
}
