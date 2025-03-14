#include <portablegl.h>
#define MKXPZ_NO_GL_TYPES

#ifdef __cplusplus
extern "C" {
#endif

struct GrayUniforms
{
	pgl_mat4 projMat;
	pgl_vec2 texSizeInv;
	pgl_vec2 translation;
	GLuint texture;
	float gray;
};

struct GrayAttribs
{
	pgl_vec2 position;
	pgl_vec2 texCoord;
};

struct GrayVarying
{
	pgl_vec2 v_texCoord;
};

void mkxpGrayVS(float *output, pgl_vec4 *attribs, Shader_Builtins *builtins, void *uniforms);
void mkxpGrayFS(float *input, Shader_Builtins *builtins, void *uniforms);

#ifdef __cplusplus
}
#endif
