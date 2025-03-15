#include <portablegl.h>

#ifdef __cplusplus
extern "C" {
#endif

struct HueUniforms
{
	pgl_mat4 projMat;
	pgl_vec2 texSizeInv;
	pgl_vec2 translation;
	GLuint texture;
	float hueAdjust;
};

struct HueAttribs
{
	pgl_vec2 position;
	pgl_vec2 texCoord;
};

struct HueVarying
{
	pgl_vec2 v_texCoord;
};

void mkxpHueVS(float *output, pgl_vec4 *attribs, Shader_Builtins *builtins, void *uniforms);
void mkxpHueFS(float *input, Shader_Builtins *builtins, void *uniforms);

#ifdef __cplusplus
}
#endif
