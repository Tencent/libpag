#version 330
in vec4 Position;
in vec2 UVs;
out vec4 out_pos;
out vec2 out_uvs;
uniform mat4 ModelviewProjection;

void main( void )
{
    out_pos = ModelviewProjection * Position;
    gl_Position = out_pos;
	out_uvs = UVs;
}