#include <portablegl.h>
#define MKXPZ_NO_GL_TYPES

#ifdef __cplusplus
extern "C" {
#endif

struct SimpleAlphaUniforms
{
	pgl_mat4 projMat;
	pgl_vec2 texSizeInv;
	pgl_vec2 translation;
	GLuint texture;
};

struct SimpleAlphaAttribs
{
	pgl_vec2 position;
	pgl_vec2 texCoord;
	pgl_vec4 color;
};

struct SimpleAlphaVarying
{
	pgl_vec2 v_texCoord;
	pgl_vec4 v_color;
};

void mkxpSimpleAlphaVS(float *output, pgl_vec4 *attribs, Shader_Builtins *builtins, void *uniforms);
void mkxpSimpleAlphaFS(float *input, Shader_Builtins *builtins, void *uniforms);

#ifdef __cplusplus
}
#endif
