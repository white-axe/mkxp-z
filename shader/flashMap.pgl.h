#include <portablegl.h>

#ifdef __cplusplus
extern "C" {
#endif

struct FlashMapUniforms
{
	pgl_mat4 projMat;
	pgl_vec2 texSizeInv;
	pgl_vec2 translation;
	float alpha;
};

struct FlashMapAttribs
{
	pgl_vec2 position;
	pgl_vec2 texCoord;
	pgl_vec4 color;
};

struct FlashMapVarying
{
	pgl_vec2 v_texCoord;
	pgl_vec4 v_color;
};

void mkxpFlashMapVS(float *output, pgl_vec4 *attribs, Shader_Builtins *builtins, void *uniforms);
void mkxpFlashMapFS(float *input, Shader_Builtins *builtins, void *uniforms);

#ifdef __cplusplus
}
#endif
