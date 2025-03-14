#include <portablegl.h>
#define MKXPZ_NO_GL_TYPES

#ifdef __cplusplus
extern "C" {
#endif

struct TransSimpleUniforms
{
	pgl_mat4 projMat;
	pgl_vec2 texSizeInv;
	pgl_vec2 translation;
	GLuint frozenScene;
	GLuint currentScene;
	float prog;
};

struct TransSimpleAttribs
{
	pgl_vec2 position;
	pgl_vec2 texCoord;
};

struct TransSimpleVarying
{
	pgl_vec2 v_texCoord;
};

void mkxpTransSimpleVS(float *output, pgl_vec4 *attribs, Shader_Builtins *builtins, void *uniforms);
void mkxpTransSimpleFS(float *input, Shader_Builtins *builtins, void *uniforms);

#ifdef __cplusplus
}
#endif
