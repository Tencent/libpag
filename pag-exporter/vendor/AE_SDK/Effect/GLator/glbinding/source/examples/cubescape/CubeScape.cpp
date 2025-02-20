
#include "CubeScape.h"

#include <iostream>
#include <fstream>
#include <math.h>

#include <glbinding/gl/gl.h>

#include "glutils.h"
#include "RawFile.h"

using namespace gl;

namespace
{

bool readFile(const std::string & filePath, std::string & content)
{
    // http://insanecoding.blogspot.de/2011/11/how-to-read-in-file-in-c.html

    std::ifstream in(filePath, std::ios::in | std::ios::binary);

    if (!in)
        return false;

    content = std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

// convenience
std::string readFile(const std::string & filePath)
{
    std::string content;
    readFile(filePath, content);

    return content;
}

}

CubeScape::CubeScape()
: a_vertex(-1)
, u_transform(-1)
, u_time(-1)
, u_numcubes(-1)
, m_vao(0)
, m_indices(0)
, m_vertices(0)
, m_program(0)
, m_a(0.f)
, m_numcubes(16)
{
    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint gs = glCreateShader(GL_GEOMETRY_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);

    std::string vertexSource   = readFile("data/cubescape/cubescape.vert");
    std::string geometrySource = readFile("data/cubescape/cubescape.geom");
    std::string fragmentSource = readFile("data/cubescape/cubescape.frag");

    const char * vertSource = vertexSource.c_str();
    const char * geomSource = geometrySource.c_str();
    const char * fragSource = fragmentSource.c_str();

    glShaderSource(vs, 1, &vertSource, nullptr);
    glCompileShader(vs);
    compile_info(vs);

    glShaderSource(gs, 1, &geomSource, nullptr);
    glCompileShader(gs);
    compile_info(gs);

    glShaderSource(fs, 1, &fragSource, nullptr);
    glCompileShader(fs);
    compile_info(fs);

    m_program = glCreateProgram();
    
    glAttachShader(m_program, vs);
    glAttachShader(m_program, gs);
    glAttachShader(m_program, fs);

    glLinkProgram(m_program);
    link_info(m_program);

    // create textures

    glGenTextures(2, m_textures);

    glBindTexture(GL_TEXTURE_2D, m_textures[0]);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<int>(GL_REPEAT));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<int>(GL_REPEAT));

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<int>(GL_LINEAR));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<int>(GL_LINEAR));

    {
        RawFile terrain("data/cubescape/terrain.64.64.r.ub.raw");
        if (!terrain.isValid())
            std::cout << "warning: loading texture from " << terrain.filePath() << " failed.";

        glTexImage2D(GL_TEXTURE_2D, 0, static_cast<int>(GL_R8), 64, 64, 0, GL_RED, GL_UNSIGNED_BYTE, terrain.data());
    }

    glBindTexture(GL_TEXTURE_2D, m_textures[1]);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, static_cast<int>(GL_REPEAT));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, static_cast<int>(GL_REPEAT));

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, static_cast<int>(GL_NEAREST));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, static_cast<int>(GL_NEAREST));

    {
        RawFile patches("data/cubescape/patches.64.16.rgb.ub.raw");
        if (!patches.isValid())
            std::cout << "warning: loading texture from " << patches.filePath() << " failed.";

        glTexImage2D(GL_TEXTURE_2D, 0, static_cast<int>(GL_RGB8), 64, 16, 0, GL_RGB, GL_UNSIGNED_BYTE, patches.data());
    }


    // create cube

    static const GLfloat vertices_data[24] =
    {
        -1.f, -1.f, -1.f, // 0
        -1.f, -1.f,  1.f, // 1
        -1.f,  1.f, -1.f, // 2
        -1.f,  1.f,  1.f, // 3
         1.f, -1.f, -1.f, // 4
         1.f, -1.f,  1.f, // 5
         1.f,  1.f, -1.f, // 6
         1.f,  1.f,  1.f  // 7
    };

    static const GLubyte indices_data[18] = {
        2, 3, 6, 0, 1, 2, 1, 5, 3, 5, 4, 7, 4, 0, 6, 5, 1, 4 };

    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    glGenBuffers(1, &m_vertices);
    glBindBuffer(GL_ARRAY_BUFFER, m_vertices);
    glBufferData(GL_ARRAY_BUFFER, (8 * 3) * sizeof(float), vertices_data, GL_STATIC_DRAW);

    glGenBuffers(1, &m_indices);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indices);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (6 * 3) * sizeof(GLubyte), indices_data, GL_STATIC_DRAW);

    // setup uniforms

    a_vertex = glGetAttribLocation(m_program, "a_vertex");
    glEnableVertexAttribArray(static_cast<GLuint>(a_vertex));

    glVertexAttribPointer(static_cast<GLuint>(a_vertex), 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    u_transform = glGetUniformLocation(m_program, "modelViewProjection");
    u_time = glGetUniformLocation(m_program, "time");
    u_numcubes = glGetUniformLocation(m_program, "numcubes");

    m_time = clock::now();

    GLint terrain = glGetUniformLocation(m_program, "terrain");
    GLint patches = glGetUniformLocation(m_program, "patches");

    // since only single program and single data is used, bind only once 

    glEnable(GL_DEPTH_TEST);
    
    glClearColor(0.f, 0.f, 0.f, 1.0f);

    glUseProgram(m_program);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textures[0]);
    glUniform1i(terrain, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_textures[1]);
    glUniform1i(patches, 1);

    // view

    m_view = mat4::lookAt(0.f, 0.8f,-2.0f, 0.f, -1.2f, 0.f, 0.f, 1.f, 0.f);
}

CubeScape::~CubeScape()
{
    glDeleteBuffers(1, &m_vertices);
    glDeleteBuffers(1, &m_indices);

    glDeleteProgram(m_program);
}

void CubeScape::setNumCubes(int _numCubes)
{
    m_numcubes = std::min(4096, std::max(1, _numCubes));
}
int CubeScape::numCubes() const
{
    return m_numcubes;
}

void CubeScape::resize(int width, int height)
{
    m_projection = mat4::perspective(40.f, static_cast<GLfloat>(width) / static_cast<GLfloat>(height), 1.f, 4.f);

    glViewport(0, 0, width, height);
}

void CubeScape::draw()
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - m_time);
    float t = static_cast<float>(ms.count()) * 1e-3f;

    const mat4 transform = m_projection * m_view * mat4::rotate(t * 0.1f, 0.f, 1.f, 0.f);

    glUniformMatrix4fv(u_transform, 1, GL_FALSE, &transform[0]);
    glUniform1f(u_time, t);
    glUniform1i(u_numcubes, m_numcubes);

    glDrawElementsInstanced(GL_TRIANGLES, 18, GL_UNSIGNED_BYTE, 0, m_numcubes * m_numcubes);
}
