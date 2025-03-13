#include <portablegl.h>

#ifdef __cplusplus
extern "C" {
#endif

#define nAutotiles 7

struct TilemapUniforms
{
	pgl_mat4 projMat;
	pgl_vec2 texSizeInv;
	pgl_vec2 translation;
	int aniIndex;
	int atFrames[nAutotiles];
	GLuint texture;
	pgl_vec4 tone;
	float opacity;
	pgl_vec4 color;
};

struct TilemapAttribs
{
	pgl_vec2 position;
	pgl_vec2 texCoord;
};

struct TilemapVarying
{
	pgl_vec2 v_texCoord;
};

void mkxpTilemapVS(float *output, pgl_vec4 *attribs, Shader_Builtins *builtins, void *uniforms);
void mkxpTilemapFS(float *input, Shader_Builtins *builtins, void *uniforms);

#ifdef __cplusplus
}
#endif
