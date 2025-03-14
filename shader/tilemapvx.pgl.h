#include <portablegl.h>
#define MKXPZ_NO_GL_TYPES

#ifdef __cplusplus
extern "C" {
#endif

struct TilemapVXUniforms
{
	pgl_mat4 projMat;
	pgl_vec2 texSizeInv;
	pgl_vec2 translation;
	pgl_vec2 aniOffset;
	GLuint texture;
};

struct TilemapVXAttribs
{
	pgl_vec2 position;
	pgl_vec2 texCoord;
};

struct TilemapVXVarying
{
	pgl_vec2 v_texCoord;
};

void mkxpTilemapVXVS(float *output, pgl_vec4 *attribs, Shader_Builtins *builtins, void *uniforms);
void mkxpTilemapVXFS(float *input, Shader_Builtins *builtins, void *uniforms);

#ifdef __cplusplus
}
#endif
