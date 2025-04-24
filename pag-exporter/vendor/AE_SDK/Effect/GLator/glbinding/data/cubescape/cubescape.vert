#version 150 core

in vec3 a_vertex;
out float v_h;

uniform sampler2D terrain;
uniform float time;

uniform int numcubes;

void main()
{
	float oneovernumcubes = 1.f / float(numcubes);
	vec2 uv = vec2(mod(gl_InstanceID, numcubes), floor(gl_InstanceID * oneovernumcubes)) * 2.0 * oneovernumcubes;

	vec3 v = a_vertex * oneovernumcubes - (1.0 - oneovernumcubes);
	v.xz  += uv;

	v_h = texture(terrain, uv * 0.5 + vec2(sin(time * 0.04), time * 0.02)).r * 2.0 / 3.0;

	if(a_vertex.y > 0.0) 
	    v.y += v_h;

	gl_Position = vec4(v, 1.0); 
}
