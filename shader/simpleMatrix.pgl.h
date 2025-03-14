#include <portablegl.h>
#define MKXPZ_NO_GL_TYPES

#ifdef __cplusplus
extern "C" {
#endif

struct SimpleMatrixUniforms
{
	pgl_mat4 projMat;
	pgl_mat4 matrix;
	pgl_vec2 texSizeInv;
	GLuint texture;
};

struct SimpleMatrixAttribs
{
	pgl_vec2 position;
	pgl_vec2 texCoord;
	pgl_vec4 color;
};

struct SimpleMatrixVarying
{
	pgl_vec2 v_texCoord;
	pgl_vec4 v_color;
};

void mkxpSimpleMatrixVS(float *output, pgl_vec4 *attribs, Shader_Builtins *builtins, void *uniforms);
void mkxpSimpleMatrixFS(float *input, Shader_Builtins *builtins, void *uniforms);

#ifdef __cplusplus
}
#endif
