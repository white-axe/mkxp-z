#include <portablegl.h>
#define MKXPZ_NO_GL_TYPES

#ifdef __cplusplus
extern "C" {
#endif

struct SimpleUniforms
{
	pgl_mat4 projMat;
	pgl_vec2 texSizeInv;
	pgl_vec2 translation;
	GLuint texture;
};

struct SimpleAttribs
{
	pgl_vec2 position;
	pgl_vec2 texCoord;
};

struct SimpleVarying
{
	pgl_vec2 v_texCoord;
};

void mkxpSimpleVS(float *output, pgl_vec4 *attribs, Shader_Builtins *builtins, void *uniforms);
void mkxpSimpleFS(float *input, Shader_Builtins *builtins, void *uniforms);

#ifdef __cplusplus
}
#endif
