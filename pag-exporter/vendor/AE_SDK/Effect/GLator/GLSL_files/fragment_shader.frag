#version 330
uniform sampler2D videoTexture;
uniform float sliderVal;
uniform float multiplier16bit;
in vec4 out_pos;
in vec2 out_uvs;
out vec4 colourOut;

void main( void )
{
	//simplest texture lookup
	colourOut = texture( videoTexture, out_uvs.xy ); 

	// in case of 16 bits, convert 32768->65535
	colourOut = colourOut * multiplier16bit;

	// swizzle ARGB to RGBA
	colourOut = vec4(colourOut.g, colourOut.b, colourOut.a, colourOut.r);

	// convert to pre-multiplied alpha
	colourOut = vec4(colourOut.a * colourOut.r, colourOut.a * colourOut.g, colourOut.a * colourOut.b, colourOut.a);

	// I got the blues
	colourOut.g = sliderVal;
}
