#include <portablegl.h>

#ifdef __cplusplus
extern "C" {
#endif

struct PlaneUniforms
{
	pgl_mat4 projMat;
	pgl_vec2 texSizeInv;
	pgl_vec2 translation;
	GLuint texture;
	pgl_vec4 tone;
	float opacity;
	pgl_vec4 color;
	pgl_vec4 flash;
};

struct PlaneAttribs
{
	pgl_vec2 position;
	pgl_vec2 texCoord;
};

struct PlaneVarying
{
	pgl_vec2 v_texCoord;
};

void mkxpPlaneVS(float *output, pgl_vec4 *attribs, Shader_Builtins *builtins, void *uniforms);
void mkxpPlaneFS(float *input, Shader_Builtins *builtins, void *uniforms);

#ifdef __cplusplus
}
#endif
