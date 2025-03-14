#include <portablegl.h>
#define MKXPZ_NO_GL_TYPES

#ifdef __cplusplus
extern "C" {
#endif

struct TransUniforms
{
	pgl_mat4 projMat;
	pgl_vec2 texSizeInv;
	pgl_vec2 translation;
	GLuint currentScene;
	GLuint frozenScene;
	GLuint transMap;
	float prog;
	float vague;
};

struct TransAttribs
{
	pgl_vec2 position;
	pgl_vec2 texCoord;
};

struct TransVarying
{
	pgl_vec2 v_texCoord;
};

void mkxpTransVS(float *output, pgl_vec4 *attribs, Shader_Builtins *builtins, void *uniforms);
void mkxpTransFS(float *input, Shader_Builtins *builtins, void *uniforms);

#ifdef __cplusplus
}
#endif
