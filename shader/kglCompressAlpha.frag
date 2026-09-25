uniform sampler2D texture;

varying vec2 v_texCoord;

void main()
{
	vec4 frag = texture2D(texture, v_texCoord);
	gl_FragColor = vec4(frag.a * frag.rgb, 0.);
}
