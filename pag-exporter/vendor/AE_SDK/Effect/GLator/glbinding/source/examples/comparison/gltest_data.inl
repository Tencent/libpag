
struct vec2
{
    float x;
    float y;
};

const vec2 vertices[4] = { { +1.f, -1.f }, { +1.f, +1.f }, { -1.f, -1.f }, { -1.f, +1.f } };

const GLchar * vert = R"(
#version 150
#extension GL_ARB_explicit_attrib_location : require

layout (location = 0) in vec2 a_vertex;

out vec4 color;

void main()
{
    gl_Position = vec4(a_vertex, 0.0, 1.0);
    color = vec4(a_vertex * 0.5 + 0.5, 0.0, 1.0);
}
)";

const GLchar * frag = R"(
#version 150
#extension GL_ARB_explicit_attrib_location : require

layout (location = 0) out vec4 fragColor;

in vec4 color;

void main()
{
    fragColor = color;
}
)";

GLuint vao;
GLuint quad;
GLuint program;
GLuint vs;
GLuint fs;
GLuint a_vertex;
