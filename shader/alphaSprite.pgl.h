#include <portablegl.h>
#include <stdbool.h>
#define MKXPZ_NO_GL_TYPES

#ifdef __cplusplus
extern "C" {
#endif

struct AlphaSpriteUniforms
{
	pgl_mat4 projMat;
	pgl_mat4 spriteMat;
	pgl_vec2 texSizeInv;
	pgl_vec2 patternSizeInv;
	pgl_vec2 patternScroll;
	pgl_vec2 patternZoom;
	bool renderPattern;
	bool patternTile;
	GLuint texture;
	float alpha;
};

struct AlphaSpriteAttribs
{
	pgl_vec2 position;
	pgl_vec2 texCoord;
};

struct AlphaSpriteVarying
{
	pgl_vec2 v_texCoord;
	pgl_vec2 v_patCoord;
};

void mkxpAlphaSpriteVS(float *output, pgl_vec4 *attribs, Shader_Builtins *builtins, void *uniforms);
void mkxpAlphaSpriteFS(float *input, Shader_Builtins *builtins, void *uniforms);

#ifdef __cplusplus
}
#endif
