
#pragma once

#include <glbinding/gl32core/gl.h>

void compile_info(const gl32core::GLuint shader);
void link_info(const gl32core::GLuint program);

struct mat4
{
    mat4();

    inline gl::GLfloat & operator[](const int i) { return m[i]; }
    inline const gl::GLfloat & operator[](const int i) const { return m[i]; }

    static mat4 lookAt(gl::GLfloat eyex, gl::GLfloat eyey, gl::GLfloat eyez
        , gl::GLfloat centerx, gl::GLfloat centery, gl::GLfloat centerz, gl::GLfloat upx, gl::GLfloat upy, gl::GLfloat upz);

    static mat4 perspective(gl::GLfloat fovy, gl::GLfloat aspect, gl::GLfloat zNear, gl::GLfloat zFar);

    static mat4 translate(gl::GLfloat x, gl::GLfloat y, gl::GLfloat z);
    static mat4 scale(gl::GLfloat x, gl::GLfloat y, gl::GLfloat z);
    static mat4 rotate(gl::GLfloat angle, gl::GLfloat x, gl::GLfloat y, gl::GLfloat z);

    gl::GLfloat m[16];
};

mat4 operator*(const mat4 & a, const mat4 & b);

struct vec3
{
    vec3();
    vec3(gl::GLfloat x, gl::GLfloat y, gl::GLfloat z);

    inline gl::GLfloat & operator[](const int i) { return v[i]; }
    inline const gl::GLfloat & operator[](const int i) const { return v[i]; }

    vec3 & operator+(const vec3 rhs);
    vec3 & operator-(const vec3 rhs);
    vec3 & operator*(const vec3 rhs);
    vec3 & operator/(const vec3 rhs);

    gl::GLfloat length();
    void normalize();

    gl::GLfloat v[3];
};

vec3 crossp(const vec3 & a, const vec3 & b);
gl::GLfloat dotp(const vec3 & a, const vec3 & b);
