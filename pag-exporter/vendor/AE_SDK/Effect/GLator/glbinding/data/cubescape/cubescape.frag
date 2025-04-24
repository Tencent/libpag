#version 150 core

in float g_h;

in vec2 g_uv;
in vec3 g_normal;

out vec4 fragColor;

uniform sampler2D patches;

void main()
{
	vec3 n = normalize(g_normal);
	vec3 l = normalize(vec3(0.0, -0.5, 1.0));

	float lambert = dot(n, l);

	float t = (2.0 / 3.0 - g_h) * 1.5 * 4.0 - 1.0;
	vec2 uv = g_uv * vec2(0.25, 1.0);

	vec4 c0 = texture(patches, uv + max(floor(t), 0.0) * vec2(0.25, 0.0));
	vec4 c1 = texture(patches, uv + min(floor(t) + 1.0, 3.0) * vec2(0.25, 0.0));

	fragColor = mix(c0, c1, smoothstep(0.25, 0.75, fract(t))) * lambert;
}
