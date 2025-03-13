#include <portablegl.h>

#ifdef __cplusplus
extern "C" {
#endif

struct BitmapBlitUniforms
{
	pgl_mat4 projMat;
	pgl_vec2 texSizeInv;
	pgl_vec2 translation;
	GLuint source;
	GLuint destination;
	pgl_vec4 subRect;
	float opacity;
};

struct BitmapBlitAttribs
{
	pgl_vec2 position;
	pgl_vec2 texCoord;
};

struct BitmapBlitVarying
{
	pgl_vec2 v_texCoord;
};

void mkxpBitmapBlitVS(float *output, pgl_vec4 *attribs, Shader_Builtins *builtins, void *uniforms);
void mkxpBitmapBlitFS(float *input, Shader_Builtins *builtins, void *uniforms);

#ifdef __cplusplus
}
#endif
