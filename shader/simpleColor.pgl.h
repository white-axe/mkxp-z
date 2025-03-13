#include <portablegl.h>

#ifdef __cplusplus
extern "C" {
#endif

struct SimpleColorUniforms
{
	pgl_mat4 projMat;
	pgl_vec2 texSizeInv;
	pgl_vec2 translation;
};

struct SimpleColorAttribs
{
	pgl_vec2 position;
	pgl_vec2 texCoord;
	pgl_vec4 color;
};

struct SimpleColorVarying
{
	pgl_vec2 v_texCoord;
	pgl_vec4 v_color;
};

void mkxpSimpleColorVS(float *output, pgl_vec4 *attribs, Shader_Builtins *builtins, void *uniforms);
void mkxpSimpleColorFS(float *input, Shader_Builtins *builtins, void *uniforms);

#ifdef __cplusplus
}
#endif
