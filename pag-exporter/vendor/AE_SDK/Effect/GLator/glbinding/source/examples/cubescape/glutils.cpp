
#include "glutils.h"

#include <iostream>
#include <math.h>


#include <glbinding/gl32core/gl.h>


using namespace gl32core;


void compile_info(const GLuint shader)
{
    GLint status(0);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);

    if (1 != status)
    {
        GLint maxLength(0);
        GLint logLength(0);

        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

        GLchar * log = new GLchar[maxLength];
        glGetShaderInfoLog(shader, maxLength, &logLength, log);

        std::cout << "Compiling shader failed." << std::endl
            << log << std::endl;
    }
}

void link_info(const GLuint program)
{
    GLint status(0);
    glGetProgramiv(program, GL_LINK_STATUS, &status);

    if (1 != status)
    {
        GLint maxLength(0);
        GLint logLength(0);

        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

        GLchar * log = new GLchar[maxLength];
        glGetProgramInfoLog(program, maxLength, &logLength, log);

        std::cout << "Linking program failed." << std::endl
            << log << std::endl;
    }
}


mat4::mat4()
{
    for (int i = 0; i < 16; ++i)
        m[i] = 0.f;

    m[0] = m[5] = m[10] = m[15] = 1.f;
}


 mat4 operator*(const mat4 & a, const mat4 & b)
{
    mat4 m;

    m[ 0] = a[ 0] * b[ 0] + a[ 4] * b[ 1] + a[ 8] * b[ 2] + a[12] * b[ 3];
    m[ 4] = a[ 0] * b[ 4] + a[ 4] * b[ 5] + a[ 8] * b[ 6] + a[12] * b[ 7];
    m[ 8] = a[ 0] * b[ 8] + a[ 4] * b[ 9] + a[ 8] * b[10] + a[12] * b[11];
    m[12] = a[ 0] * b[12] + a[ 4] * b[13] + a[ 8] * b[14] + a[12] * b[15];

    m[ 1] = a[ 1] * b[ 0] + a[ 5] * b[ 1] + a[ 9] * b[ 2] + a[13] * b[ 3];
    m[ 5] = a[ 1] * b[ 4] + a[ 5] * b[ 5] + a[ 9] * b[ 6] + a[13] * b[ 7];
    m[ 9] = a[ 1] * b[ 8] + a[ 5] * b[ 9] + a[ 9] * b[10] + a[13] * b[11];
    m[13] = a[ 1] * b[12] + a[ 5] * b[13] + a[ 9] * b[14] + a[13] * b[15];

    m[ 2] = a[ 2] * b[ 0] + a[ 6] * b[ 1] + a[10] * b[ 2] + a[14] * b[ 3];
    m[ 6] = a[ 2] * b[ 4] + a[ 6] * b[ 5] + a[10] * b[ 6] + a[14] * b[ 7];
    m[10] = a[ 2] * b[ 8] + a[ 6] * b[ 9] + a[10] * b[10] + a[14] * b[11];
    m[14] = a[ 2] * b[12] + a[ 6] * b[13] + a[10] * b[14] + a[14] * b[15];

    m[ 3] = a[ 3] * b[ 0] + a[ 7] * b[ 1] + a[11] * b[ 2] + a[15] * b[ 3];
    m[ 7] = a[ 3] * b[ 4] + a[ 7] * b[ 5] + a[11] * b[ 6] + a[15] * b[ 7];
    m[11] = a[ 3] * b[ 8] + a[ 7] * b[ 9] + a[11] * b[10] + a[15] * b[11];
    m[15] = a[ 3] * b[12] + a[ 7] * b[13] + a[11] * b[14] + a[15] * b[15];

    return m;
}

mat4 mat4::perspective(GLfloat fovy, GLfloat aspect, GLfloat zNear, GLfloat zFar)
{
    mat4 m;

    //const float zRange = zNear - zFar;
    const float f = 1.f / static_cast<float>(tan(fovy * 0.5f * 0.01745329251994329576923690768489f));

    m[ 0] = f / aspect;
    m[ 5] = f;

    m[10] = -(zFar + zNear) / (zFar - zNear);
    m[14] = -(2.f * zFar * zNear) / (zFar - zNear);

    m[11] = -1.f;
    m[15] =  0.f;

    return m;
}

mat4 mat4::lookAt(GLfloat eyex, GLfloat eyey, GLfloat eyez
    , GLfloat centerx, GLfloat centery, GLfloat centerz, GLfloat upx, GLfloat upy, GLfloat upz)
{
    vec3 e(eyex, eyey, eyez);

    vec3 f(centerx - eyex, centery - eyey, centerz - eyez);
    f.normalize();

    vec3 u(upx, upy, upz);
    u.normalize();

    vec3 s = crossp(f, u);
    u = crossp(s, f);

    mat4 m;
    
    m[ 0] =  s[0];
    m[ 4] =  s[1];
    m[ 8] =  s[2];

    m[ 1] =  u[0];
    m[ 5] =  u[1];
    m[ 9] =  u[2];

    m[ 2] = -f[0];
    m[ 6] = -f[1];
    m[10] = -f[2];

    m[12] = -dotp(s, e);
    m[13] = -dotp(u, e);
    m[14] =  dotp(f, e);

    return m;
}

mat4 mat4::translate(GLfloat x, GLfloat y, GLfloat z)
{
    mat4 m;

    m[12] = x;
    m[13] = y;
    m[14] = z;

    return m;
}

mat4 mat4::scale(GLfloat x, GLfloat y, GLfloat z)
{
    mat4 m;

    m[ 0] = x;
    m[ 5] = y;
    m[10] = z;

    return m;
}

mat4 mat4::rotate(GLfloat a, GLfloat x, GLfloat y, GLfloat z)
{
    mat4 m;

    GLfloat l = 1.f / static_cast<float>(sqrt(x * x + y * y + z * z));

    x *= l;
    y *= l;
    z *= l;

    const GLfloat c = static_cast<float>(cos(a));
    const GLfloat s = static_cast<float>(sin(a));

    const GLfloat d = 1.f - c;

    m[ 0] = x * x * d + c;
    m[ 4] = x * y * d - z * s;
    m[ 8] = x * z * d + y * s;

    m[ 1] = y * x * d + z * s;
    m[ 5] = y * y * d + c;
    m[ 9] = y * z * d - x * s;

    m[ 2] = z * x * d - y * s;
    m[ 6] = z * y * d + x * s;
    m[10] = z * z * d + c;

    return m;
}

vec3::vec3()
{
    v[0] = v[1] = v[2] = 0.f;
}

vec3::vec3(GLfloat x, GLfloat y, GLfloat z)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
}

vec3 & vec3::operator+(const vec3 rhs)
{
    v[0] += rhs[0];
    v[1] += rhs[1];
    v[2] += rhs[2];

    return *this;
}

vec3 & vec3::operator-(const vec3 rhs)
{
    v[0] -= rhs[0];
    v[1] -= rhs[1];
    v[2] -= rhs[2];

    return *this;
}

vec3 & vec3::operator*(const vec3 rhs)
{
    v[0] *= rhs[0];
    v[1] *= rhs[1];
    v[2] *= rhs[2];

    return *this;
}

vec3 & vec3::operator/(const vec3 rhs)
{
    v[0] /= rhs[0];
    v[1] /= rhs[1];
    v[2] /= rhs[2];

    return *this;
}

GLfloat vec3::length()
{
    return static_cast<float>(sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]));
}

void vec3::normalize()
{
    const GLfloat s = 1.f / length();

    v[0] *= s;
    v[1] *= s;
    v[2] *= s;    
}

vec3 crossp(const vec3 & a, const vec3 & b)
{
    vec3 v;

    v[0] = a[1] * b[2] - a[2] * b[1];
    v[1] = a[2] * b[0] - a[0] * b[2];
    v[2] = a[0] * b[1] - a[1] * b[0];

    return v;
}

GLfloat dotp(const vec3 & a, const vec3 & b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}
