#include <portablegl.h>
#define MKXPZ_NO_GL_TYPES

#ifdef __cplusplus
extern "C" {
#endif

struct BlurHUniforms
{
	pgl_mat4 projMat;
	pgl_vec2 texSizeInv;
	GLuint texture;
};

struct BlurHAttribs
{
	pgl_vec2 position;
	pgl_vec2 texCoord;
};

struct BlurHVarying
{
	pgl_vec2 v_texCoord;
	pgl_vec2 v_blurCoord[2];
};

void mkxpBlurHVS(float *output, pgl_vec4 *attribs, Shader_Builtins *builtins, void *uniforms);
void mkxpBlurHFS(float *input, Shader_Builtins *builtins, void *uniforms);

#ifdef __cplusplus
}
#endif
