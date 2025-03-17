#include "simpleSprite.pgl.h"

GLuint mkxp_pgl_get_texture2D(GLuint unit);

void mkxpSimpleSpriteVS(float *_output, pgl_vec4 *_attribs, Shader_Builtins *builtins, void *_uniforms)
{
	struct SimpleSpriteVarying *output = (struct SimpleSpriteVarying *)_output;
	struct SimpleSpriteAttribs *attribs = (struct SimpleSpriteAttribs *)_attribs;
	struct SimpleSpriteUniforms *uniforms = (struct SimpleSpriteUniforms *)_uniforms;

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

void mkxpSimpleSpriteFS(float *_input, Shader_Builtins *builtins, void *_uniforms)
{
	struct SimpleSpriteVarying *input = (struct SimpleSpriteVarying *)_input;
	struct SimpleSpriteUniforms *uniforms = (struct SimpleSpriteUniforms *)_uniforms;

	builtins->gl_FragColor = mkxp_pgl_texture2D(mkxp_pgl_get_texture2D(uniforms->texture), input->v_texCoord.x, input->v_texCoord.y);
}
