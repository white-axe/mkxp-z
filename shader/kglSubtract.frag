uniform sampler2D source;
uniform sampler2D destination;

uniform vec4 subRect;

uniform lowp float opacity;

varying vec2 v_texCoord;

void main()
{
	vec2 coor = v_texCoord;
	vec2 dstCoor = (coor - subRect.xy) * subRect.zw;

	vec4 srcFrag = texture2D(source, coor);
	vec4 dstFrag = texture2D(destination, dstCoor);

	gl_FragColor = vec4(dstFrag.rgb - opacity * srcFrag.rgb, 1.);
}
