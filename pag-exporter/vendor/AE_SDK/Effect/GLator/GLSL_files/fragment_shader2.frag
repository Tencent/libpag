#version 330
uniform sampler2D videoTexture;
uniform float multiplier16bit;
in vec4 out_pos;
in vec2 out_uvs;
out vec4 colourOut;

void main( void )
{
	//simplest texture lookup
	colourOut = texture( videoTexture, out_uvs.xy ); 

	// convert to non-pre-multiplied alpha....
	if (colourOut.a == 0) {
		colourOut = vec4(0, 0, 0, 0);
	} else {
		// ... and also swizzle RGBA to ARGB
		colourOut = vec4(colourOut.a, colourOut.r / colourOut.a, colourOut.g / colourOut.a, colourOut.b / colourOut.a);
	}
	// finally handle 16 bits conversion (if applicable)
	// in case of 16 bits, convert 65535->32768
	colourOut = colourOut / multiplier16bit;
}
