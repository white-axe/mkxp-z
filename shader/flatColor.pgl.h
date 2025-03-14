#include <portablegl.h>
#define MKXPZ_NO_GL_TYPES

#ifdef __cplusplus
extern "C" {
#endif

struct FlatColorUniforms
{
	pgl_mat4 projMat;
	pgl_vec4 color;
};

struct FlatColorAttribs
{
	pgl_vec2 position;
};

void mkxpFlatColorVS(float *_output, pgl_vec4 *attribs, Shader_Builtins *builtins, void *uniforms);
void mkxpFlatColorFS(float *input, Shader_Builtins *builtins, void *uniforms);

#ifdef __cplusplus
}
#endif
