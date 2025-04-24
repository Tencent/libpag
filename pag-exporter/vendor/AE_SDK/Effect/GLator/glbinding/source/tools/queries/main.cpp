
#include <iostream>
#include <sstream>
#include <array>
#include <string>
#include <type_traits>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glbinding/Meta.h>
#include <glbinding/ContextInfo.h>
#include <glbinding/Version.h>
#include <glbinding/Binding.h>

#include <glbinding/gl/types.h>
#include <glbinding/gl/enum.h>
#include <glbinding/gl/functions.h>
#include <glbinding/gl/boolean.h>


using namespace gl;
using namespace glbinding;

namespace
{

    void error(int errnum, const char * errmsg)
    {
        std::cerr << errnum << ": " << errmsg << std::endl;
    }

    static const std::array<GLfloat  , 9> identity3{ { 1.f, 0.f, 0.f,  0.f, 1.f, 0.f,  0.f, 0.f, 1.f } };
    static const std::array<GLfloat  , 16> identity4{ { 1.f, 0.f, 0.f, 0.f,  0.f, 1.f, 0.f, 0.f,  0.f, 0.f, 1.f, 0.f,  0.f, 0.f, 0.f, 1.f } };

    template <typename T>
    void glrequest(const GLenum pname, T * data);

    template <>
    void glrequest<GLboolean>(const GLenum pname, GLboolean * data)
    {
        glGetBooleanv(pname, data);
    }

    template <>
    void glrequest<GLenum>(const GLenum pname, GLenum * data)
    {
        glGetIntegerv(pname, reinterpret_cast<GLint *>(data));
    }

    template <>
    void glrequest<GLint>(const GLenum pname, GLint * data)
    {
        glGetIntegerv(pname, data);
    }

    template <>
    void glrequest<GLint64>(const GLenum pname, GLint64 * data)
    {
        glGetInteger64v(pname, data);
    }

    template <>
    void glrequest<GLfloat>(const GLenum pname, GLfloat * data)
    {
        glGetFloatv(pname, data);
    }

    //template <>
    //void glrequest<GLdouble>(const GLenum pname, GLdouble * data)
    //{
    //    glGetDoublev(pname, data);
    //}

    template <typename T, int count>
    struct identity
    {
        static bool valid(const std::array<T, count> &)
        {
            return false;
        }
        static std::string str()
        {
            return "";
        }
    };

    template <>
    struct identity<GLfloat  , 9>
    {
        static bool valid(const std::array<GLfloat  , 9> & data)
        {
            return identity3 == data;
        }
        static std::string str()
        {
            return "identity3";
        }
    };

    template <>
    struct identity<GLfloat  , 16>
    {
        static bool valid(const std::array<GLfloat  , 16> & data)
        {
            return identity4 == data;
        }
        static std::string str()
        {
            return "identity4";
        }
    };

    template <typename T, int count>
    std::string string(const std::array<T, count> & data)
    {
        std::stringstream stream;

        if (identity<T, count>::valid(data))
            stream << identity<T, count>::str();
        else
        {
            if (data.size() > 1)
                stream << "(";

            for (int i = 0; i < count; ++i)
            {
                stream << data[i];
                if (i + 1 < count)
                    stream << ", ";
            }

            if (data.size() > 1)
                stream << ")";
        }

        return stream.str();
    }

    template <typename T>
    std::string string(const std::vector<T> & data, int count)
    {
        std::stringstream stream;

            for (int i = 0; i < count; ++i)
            {
                stream << data[i];
                if (i + 1 < count)
                    stream << ", ";
            }

            if (data.size() == 1)
                stream << "NONE";

        return stream.str();
    }

    template <typename T, int count>
    bool request(const GLenum pname, std::array<T, count> & data)
    {
        glrequest<T>(pname, data.data());

        static const size_t MAX_PSTRING_LENGTH { 37 };    // actually, it's 44 / average is 23,
                                                          // but 37 works for 452 of 462 glGet enums (98%)

        const std::string pstring{ glbinding::Meta::getString(pname) };
        const std::string spaces{ std::string((glbinding::Meta::getString(pname).length() > 37) ? 0 : (MAX_PSTRING_LENGTH - pstring.length()), ' ') };

        if (glGetError() != gl::GL_NO_ERROR)
        {
            std::cout << "\t" << pstring << spaces << " = NOT AVAILABLE";
            return false;
        }
        std::cout << "\t" << glbinding::Meta::getString(pname) << spaces << " = " << string<T, count>(data);
        return true;
    }

    template <typename T>
    bool request(const GLenum pname, std::vector<T> & data, int count)
    {
        glrequest<T>(pname, data.data());

        static const size_t MAX_PSTRING_LENGTH{ 37 };    // actually, it's 44 / average is 23,
        // but 37 works for 452 of 462 glGet enums (98%)

        const std::string pstring{ glbinding::Meta::getString(pname) };
        const std::string spaces{ std::string(MAX_PSTRING_LENGTH - pstring.length(), ' ') };

        if (glGetError() != gl::GL_NO_ERROR)
        {
            std::cout << "\t" << pstring << spaces << " = NOT AVAILABLE";
            return false;
        }
        std::cout << "\t" << glbinding::Meta::getString(pname) << spaces << " = " << string<T>(data, count);
        return true;
    }

    template <typename T, int count>
    void requestState(const GLenum pname)
    {
        std::array<T, count> data;
        request<T, count>(pname, data);

        std::cout << std::endl;
    }

    enum class ExpectedType
    {
        Default
    ,   Minimum
    ,   Maximum
    };

    template <typename T, int count>
    void requestState(const GLenum pname, const std::array<T, count> & expected, ExpectedType expectedType = ExpectedType::Default)
    {
        std::array<T, count> data;
        
        if (!request<T, count>(pname, data))
        {
            std::cout << std::endl;
            return;
        }

        switch (expectedType)
        {
        case ExpectedType::Default:
            if (!expected.empty() && expected != data)
                std::cout << ", expected " << string<T, count>(expected);
            break;

        case ExpectedType::Minimum:
            if (!expected.empty() && expected > data)
                std::cout << ", expected mimimum of " << string<T, count>(expected);
            break;

        case ExpectedType::Maximum:
            if (!expected.empty() && expected < data)
                std::cout << ", expected maximum of " << string<T, count>(expected);
            break;
        }

        std::cout << std::endl;
    }

    template <typename T, GLenum count>
    void requestState(const GLenum pname)
    {
        int counti = 0;
        glGetIntegerv(count, &counti);

        std::vector<T> data(counti);
        request<T>(pname, data, counti);

        std::cout << std::endl;
    }
    
    template <typename T, int count, GLenum maxCount>
    void requestState(const GLenum pname, const std::array<T, count> & expected)
    {
        int maxCounti = 0;
        glGetIntegerv(maxCount, &maxCounti);

        std::array<T, count> data;
        for (int i = 0; i < maxCounti; ++i)
        {
            request<T, count>(static_cast<gl::GLenum>(static_cast<int>(pname)+i), data);

            if (!expected.empty() && expected != data)
                std::cout << ", expected " << string<T, count>(expected);

            std::cout << std::endl;
        }
    }
}

int main(int argc, const char * argv[])
{
    if (!Meta::stringsByGL())
    {
        std::cout << "Strings by GL not supported (enable through OPTION_STRINGS_BY_GL)" << std::endl;

        return 1;
    }
#ifndef __APPLE__
    if (argc == 2 && (std::string(argv[1]).compare("--help") || std::string(argv[1]).compare("-h")))
    {
        std::cout << "Usage: " << argv[0] << " [MAJOR_VERSION MINOR_VERSION [CORE [FORWARD_COMPATIBLE]]]" << std::endl;

        return 1;
    }
#endif

    if (!glfwInit())
        return 1;

    glfwSetErrorCallback(error);

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_VISIBLE, false);

#ifdef __APPLE__
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, true);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#else
    if (argc >= 3)
    {
        int majorVersion = atoi(argv[1]);
        int minorVersion = atoi(argv[2]);

        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, majorVersion);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minorVersion);
    }

    if (argc >= 4)
    {
        int core = atoi(argv[3]);

        glfwWindowHint(GLFW_OPENGL_PROFILE, core ? GLFW_OPENGL_CORE_PROFILE : GLFW_OPENGL_ANY_PROFILE);
    }

    if (argc >= 5)
    {
        int forwardComaptability = atoi(argv[3]);

        glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, forwardComaptability ? true : false);
    }
#endif

    GLFWwindow * window = glfwCreateWindow(320, 240, "", nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        return -1;
    }
    glfwHideWindow(window);

    glfwMakeContextCurrent(window);

    Binding::initialize();

    std::cout << std::endl << "[QUERYING STATE VALUES]" << std::endl;

    std::cout << std::endl << "Current Values and Associated Data" << std::endl;
    requestState<GLfloat  , 4>(GL_CURRENT_COLOR, { { 1, 1, 1, 1 } });
    requestState<GLfloat  , 1>(GL_CURRENT_INDEX, { { 1 } });
    requestState<GLfloat  , 4>(GL_CURRENT_TEXTURE_COORDS, { { 0, 0, 0, 1 } });
    requestState<GLfloat  , 3>(GL_CURRENT_NORMAL, { { 0, 0, 1 } });
    requestState<GLboolean, 1>(GL_EDGE_FLAG, { { GL_TRUE } });

    std::cout << std::endl << "Vertex Array States" << std::endl;
    requestState<GLboolean, 1>(GL_VERTEX_ARRAY, { { GL_FALSE } });
    requestState<GLint    , 1>(GL_VERTEX_ARRAY_SIZE, { { 4 } });
    requestState<GLenum   , 1>(GL_VERTEX_ARRAY_TYPE, { { GL_FLOAT } });
    requestState<GLint    , 1>(GL_VERTEX_ARRAY_STRIDE, { { 0 } });
    requestState<GLboolean, 1>(GL_NORMAL_ARRAY, { { GL_FALSE } });
    requestState<GLenum   , 1>(GL_NORMAL_ARRAY_TYPE, { { GL_FLOAT } });
    requestState<GLint    , 1>(GL_NORMAL_ARRAY_STRIDE, { { 0 } });
    requestState<GLboolean, 1>(GL_TEXTURE_COORD_ARRAY, { { GL_FALSE } });
    requestState<GLint    , 1>(GL_TEXTURE_COORD_ARRAY_SIZE, { { 4 } });
    requestState<GLenum   , 1>(GL_TEXTURE_COORD_ARRAY_TYPE, { { GL_FLOAT } });
    requestState<GLint    , 1>(GL_TEXTURE_COORD_ARRAY_STRIDE, { { 0 } });

    std::cout << std::endl << "Matrix Stack States" << std::endl;
    requestState<GLenum   , 1>(GL_MATRIX_MODE, { { GL_MODELVIEW } });
    requestState<GLfloat  , 16>(GL_COLOR_MATRIX, identity4);
    requestState<GLfloat  , 16>(GL_MODELVIEW_MATRIX, identity4);
    requestState<GLfloat  , 16>(GL_PROJECTION_MATRIX, identity4);
    requestState<GLfloat  , 16>(GL_TEXTURE_MATRIX, identity4);
    requestState<GLint    , 1>(GL_COLOR_MATRIX_STACK_DEPTH);
    requestState<GLint    , 1>(GL_MODELVIEW_STACK_DEPTH);
    requestState<GLint    , 1>(GL_PROJECTION_STACK_DEPTH);
    requestState<GLint    , 1>(GL_TEXTURE_STACK_DEPTH);
    requestState<GLint    , 1>(GL_MAX_MODELVIEW_STACK_DEPTH);
    requestState<GLint    , 1>(GL_MAX_PROJECTION_STACK_DEPTH, { { 2 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_TEXTURE_STACK_DEPTH);

    std::cout << std::endl << "Viewport States" << std::endl;
    requestState<GLint    , 4>(GL_VIEWPORT);
    requestState<GLint    , 2>(GL_MAX_VIEWPORT_DIMS);
    requestState<GLint    , 1>(GL_MAX_VIEWPORTS, { { 16 } }, ExpectedType::Minimum);
    requestState<GLfloat  , 2>(GL_VIEWPORT_BOUNDS_RANGE, { { -32768, 32767 } });
    requestState<GLint    , 1>(GL_VIEWPORT_SUBPIXEL_BITS, { { 0 } }, ExpectedType::Minimum);

    std::cout << std::endl << "Coloring States" << std::endl;
    requestState<GLenum   , 1>(GL_SHADE_MODEL, { { GL_SMOOTH } });

    std::cout << std::endl << "Lighting States" << std::endl;
    requestState<GLboolean, 1>(GL_LIGHTING, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_COLOR_MATERIAL, { { GL_FALSE } });
    requestState<GLenum   , 1>(GL_COLOR_MATERIAL_PARAMETER, { { GL_AMBIENT } });
    requestState<GLenum   , 1>(GL_COLOR_MATERIAL_FACE, { { GL_FRONT } });

    std::cout << std::endl << "Rasterization States" << std::endl;
    requestState<GLfloat  , 1>(GL_POINT_SIZE, { { 1 } });
    requestState<GLboolean, 1>(GL_POINT_SMOOTH, { { GL_FALSE } });
    //requestState<GLint    , 1>(GL_LINE_STIPPLE_PATTERN, { { 1's } });
    requestState<GLint    , 1>(GL_LINE_STIPPLE_REPEAT, { { 1 } });
    requestState<GLboolean, 1>(GL_LINE_STIPPLE, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_CULL_FACE, { { GL_FALSE } });
    requestState<GLenum   , 1>(GL_CULL_FACE_MODE, { { GL_BACK } });
    requestState<GLenum   , 1>(GL_FRONT_FACE, { { GL_CCW } });
    requestState<GLboolean, 1>(GL_POLYGON_SMOOTH, { { GL_FALSE } });
    requestState<GLenum   , 2>(GL_POLYGON_MODE, { { GL_FILL, GL_FILL } });
    requestState<GLfloat  , 1>(GL_POLYGON_OFFSET_FACTOR, { { 0 } });
    requestState<GLfloat  , 1>(GL_POLYGON_OFFSET_UNITS, { { 0 } });
    requestState<GLboolean, 1>(GL_POLYGON_OFFSET_POINT, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_POLYGON_OFFSET_LINE, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_POLYGON_OFFSET_FILL, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_POLYGON_STIPPLE, { { GL_FALSE } });

    std::cout << std::endl << "Pixel Operations" << std::endl;
    requestState<GLboolean, 1>(GL_SCISSOR_TEST, { { GL_FALSE } });
    requestState<GLint    , 4>(GL_SCISSOR_BOX);
    requestState<GLboolean, 1>(GL_ALPHA_TEST, { { GL_FALSE } });
    requestState<GLenum   , 1>(GL_ALPHA_TEST_FUNC, { { GL_ALWAYS } });
    requestState<GLint    , 1>(GL_ALPHA_TEST_REF, { { 0 } });
    requestState<GLboolean, 1>(GL_STENCIL_TEST, { { GL_FALSE } });
    requestState<GLenum   , 1>(GL_STENCIL_FUNC, { { GL_ALWAYS } });
    //requestState<GLint    , 1>(GL_STENCIL_VALUE_MASK, { { 1's } });
    requestState<GLint    , 1>(GL_STENCIL_REF, { { 0 } });
    requestState<GLenum   , 1>(GL_STENCIL_FAIL, { { GL_KEEP } });
    requestState<GLenum   , 1>(GL_STENCIL_PASS_DEPTH_FAIL, { { GL_KEEP } });
    requestState<GLenum   , 1>(GL_STENCIL_PASS_DEPTH_PASS, { { GL_KEEP } });
    requestState<GLboolean, 1>(GL_DEPTH_TEST, { { GL_FALSE } });
    requestState<GLenum   , 1>(GL_DEPTH_FUNC, { { GL_LESS } });
    requestState<GLboolean, 1>(GL_DITHER, { { GL_TRUE } });
    requestState<GLboolean, 1>(GL_INDEX_LOGIC_OP, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_COLOR_LOGIC_OP, { { GL_FALSE } });
    requestState<GLenum   , 1>(GL_LOGIC_OP_MODE, { { GL_COPY } });

    std::cout << std::endl << "Framebuffer Control States" << std::endl;
    //requestState<GLint    , 1>(GL_INDEX_WRITEMASK, { { 1's } });
    requestState<GLboolean, 4>(GL_COLOR_WRITEMASK, { { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE } });
    requestState<GLboolean, 1>(GL_DEPTH_WRITEMASK, { { GL_TRUE } });
    //requestState<GLint    , 1>(GL_STENCIL_WRITEMASK, { { 1's } });
    requestState<GLfloat  , 4>(GL_COLOR_CLEAR_VALUE, { { 0, 0, 0, 0 } });
    requestState<GLfloat  , 1>(GL_INDEX_CLEAR_VALUE, { { 0 } });
    requestState<GLfloat  , 1>(GL_DEPTH_CLEAR_VALUE, { { 1 } });
    requestState<GLint    , 1>(GL_STENCIL_CLEAR_VALUE, { { 0 } });
    requestState<GLfloat  , 4>(GL_ACCUM_CLEAR_VALUE, { { 0, 0, 0, 0 } });


    std::cout << std::endl << "Pixel Store States" << std::endl;
    requestState<GLboolean, 1>(GL_PACK_SWAP_BYTES, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_PACK_LSB_FIRST, { { GL_FALSE } });
    requestState<GLint    , 1>(GL_PACK_ROW_LENGTH, { { 0 } });
    requestState<GLint    , 1>(GL_PACK_IMAGE_HEIGHT, { { 0 } });
    requestState<GLint    , 1>(GL_PACK_SKIP_ROWS, { { 0 } });
    requestState<GLint    , 1>(GL_PACK_SKIP_PIXELS, { { 0 } });
    requestState<GLint    , 1>(GL_PACK_SKIP_IMAGES, { { 0 } });
    requestState<GLint    , 1>(GL_PACK_ALIGNMENT, { { 4 } });
    requestState<GLboolean, 1>(GL_UNPACK_SWAP_BYTES, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_UNPACK_LSB_FIRST, { { GL_FALSE } });
    requestState<GLint    , 1>(GL_UNPACK_ROW_LENGTH, { { 0 } });
    requestState<GLint    , 1>(GL_UNPACK_IMAGE_HEIGHT, { { 0 } });
    requestState<GLint    , 1>(GL_UNPACK_SKIP_ROWS, { { 0 } });
    requestState<GLint    , 1>(GL_UNPACK_SKIP_PIXELS, { { 0 } });
    requestState<GLint    , 1>(GL_UNPACK_SKIP_IMAGES, { { 0 } });
    requestState<GLint    , 1>(GL_UNPACK_ALIGNMENT, { { 4 } });

    std::cout << std::endl << "Pixel Transfer States" << std::endl;
    requestState<GLboolean, 1>(GL_MAP_COLOR, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_MAP_STENCIL, { { GL_FALSE } });
    requestState<GLint    , 1>(GL_INDEX_SHIFT, { { 0 } });
    requestState<GLint    , 1>(GL_INDEX_OFFSET, { { 0 } });
    requestState<GLint    , 1>(GL_RED_SCALE, { { 1 } });
    requestState<GLint    , 1>(GL_GREEN_SCALE, { { 1 } });
    requestState<GLint    , 1>(GL_BLUE_SCALE, { { 1 } });
    requestState<GLint    , 1>(GL_ALPHA_SCALE, { { 1 } });
    requestState<GLint    , 1>(GL_DEPTH_SCALE, { { 1 } });
    requestState<GLint    , 1>(GL_RED_BIAS, { { 0 } });
    requestState<GLint    , 1>(GL_GREEN_BIAS, { { 0 } });
    requestState<GLint    , 1>(GL_BLUE_BIAS, { { 0 } });
    requestState<GLint    , 1>(GL_ALPHA_BIAS, { { 0 } });
    requestState<GLint    , 1>(GL_DEPTH_BIAS, { { 0 } });
    requestState<GLint    , 1>(GL_POST_COLOR_MATRIX_RED_SCALE, { { 1 } });
    requestState<GLint    , 1>(GL_POST_COLOR_MATRIX_GREEN_SCALE, { { 1 } });
    requestState<GLint    , 1>(GL_POST_COLOR_MATRIX_BLUE_SCALE, { { 1 } });
    requestState<GLint    , 1>(GL_POST_COLOR_MATRIX_ALPHA_SCALE, { { 1 } });
    requestState<GLint    , 1>(GL_POST_COLOR_MATRIX_RED_BIAS, { { 0 } });
    requestState<GLint    , 1>(GL_POST_COLOR_MATRIX_GREEN_BIAS, { { 0 } });
    requestState<GLint    , 1>(GL_POST_COLOR_MATRIX_BLUE_BIAS, { { 0 } });
    requestState<GLint    , 1>(GL_POST_COLOR_MATRIX_ALPHA_BIAS, { { 0 } });
    requestState<GLint    , 1>(GL_POST_CONVOLUTION_RED_SCALE, { { 1 } });
    requestState<GLint    , 1>(GL_POST_CONVOLUTION_GREEN_SCALE, { { 1 } });
    requestState<GLint    , 1>(GL_POST_CONVOLUTION_BLUE_SCALE, { { 1 } });
    requestState<GLint    , 1>(GL_POST_CONVOLUTION_ALPHA_SCALE, { { 1 } });
    requestState<GLint    , 1>(GL_POST_CONVOLUTION_RED_BIAS, { { 0 } });
    requestState<GLint    , 1>(GL_POST_CONVOLUTION_GREEN_BIAS, { { 0 } });
    requestState<GLint    , 1>(GL_POST_CONVOLUTION_BLUE_BIAS, { { 0 } });
    requestState<GLint    , 1>(GL_POST_CONVOLUTION_ALPHA_BIAS, { { 0 } });

    std::cout << std::endl << "Pixel Zoom States" << std::endl;
    requestState<GLfloat  , 1>(GL_ZOOM_X, { { 1 } });
    requestState<GLfloat  , 1>(GL_ZOOM_Y, { { 1 } });

    std::cout << std::endl << "Pixel Map States" << std::endl;
    requestState<GLint    , 1>(GL_PIXEL_MAP_I_TO_I_SIZE, { { 1 } });
    requestState<GLint    , 1>(GL_PIXEL_MAP_S_TO_S_SIZE, { { 1 } });
    requestState<GLint    , 1>(GL_PIXEL_MAP_I_TO_R_SIZE, { { 1 } });
    requestState<GLint    , 1>(GL_PIXEL_MAP_I_TO_G_SIZE, { { 1 } });
    requestState<GLint    , 1>(GL_PIXEL_MAP_I_TO_B_SIZE, { { 1 } });
    requestState<GLint    , 1>(GL_PIXEL_MAP_I_TO_A_SIZE, { { 1 } });
    requestState<GLint    , 1>(GL_PIXEL_MAP_R_TO_R_SIZE, { { 1 } });
    requestState<GLint    , 1>(GL_PIXEL_MAP_G_TO_G_SIZE, { { 1 } });
    requestState<GLint    , 1>(GL_PIXEL_MAP_B_TO_B_SIZE, { { 1 } });
    requestState<GLint    , 1>(GL_PIXEL_MAP_A_TO_A_SIZE, { { 1 } });

    std::cout << std::endl << "Read Buffer States" << std::endl;
    requestState<GLenum   , 1>(GL_READ_BUFFER);

    std::cout << std::endl << "Evaluator States" << std::endl;
    requestState<GLfloat  , 2>(GL_MAP1_GRID_DOMAIN, { { 0, 1 } });
    requestState<GLfloat  , 4>(GL_MAP2_GRID_DOMAIN, { { 0, 1, 0, 1 } });
    requestState<GLfloat  , 1>(GL_MAP1_GRID_SEGMENTS, { { 1 } });
    requestState<GLfloat  , 2>(GL_MAP2_GRID_SEGMENTS, { { 1, 1 } });
    requestState<GLboolean, 1>(GL_AUTO_NORMAL, { { GL_FALSE } });

    std::cout << std::endl << "Implementation-Dependent States" << std::endl;
    requestState<GLint    , 1>(GL_MAX_3D_TEXTURE_SIZE, { { 64 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_ARRAY_TEXTURE_LAYERS, { { 256 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_ATTRIB_STACK_DEPTH, { { 16 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_CLIENT_ATTRIB_STACK_DEPTH, { { 16 } }, ExpectedType::Minimum);
    requestState<GLfloat  , 1>(GL_MAX_CLIP_DISTANCES, { { 8 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_COLOR_TEXTURE_SAMPLES);
    requestState<GLint    , 1>(GL_MAX_COMBINED_ATOMIC_COUNTERS);
    requestState<GLint    , 1>(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS);
    requestState<GLint    , 1>(GL_MAX_CUBE_MAP_TEXTURE_SIZE, { { 1024 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_DEBUG_GROUP_STACK_DEPTH);
    requestState<GLint    , 1>(GL_MAX_DEPTH_TEXTURE_SAMPLES);
    requestState<GLint64  , 1>(GL_MAX_ELEMENT_INDEX);
    requestState<GLint    , 1>(GL_MAX_EVAL_ORDER, { { 8 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_INTEGER_SAMPLES);
    requestState<GLint    , 1>(GL_MAX_LABEL_LENGTH);
    requestState<GLint    , 1>(GL_MAX_LIGHTS, { { 8 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_LIST_NESTING, { { 64 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_MODELVIEW_STACK_DEPTH, { { 32 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_NAME_STACK_DEPTH, { { 64 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_PIXEL_MAP_TABLE, { { 32 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_RECTANGLE_TEXTURE_SIZE, { { 1024 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_RENDERBUFFER_SIZE);
    requestState<GLint    , 1>(GL_MAX_SAMPLE_MASK_WORDS);
    requestState<GLint    , 1>(GL_MAX_SERVER_WAIT_TIMEOUT);
    requestState<GLint    , 1>(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, { { 8 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_TEXTURE_SIZE, { { 64 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_TEXTURE_STACK_DEPTH, { { 2 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_UNIFORM_BLOCK_SIZE, { { 16384 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_UNIFORM_BUFFER_BINDINGS, { { 36 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_UNIFORM_LOCATIONS, { { 1024 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_VARYING_COMPONENTS, { { 60 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_VARYING_FLOATS, { { 32 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_VARYING_VECTORS, { { 15 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_NUM_COMPRESSED_TEXTURE_FORMATS, { { 4 } }, ExpectedType::Minimum);
    requestState<GLenum   , GL_NUM_COMPRESSED_TEXTURE_FORMATS>(GL_COMPRESSED_TEXTURE_FORMATS); 
    requestState<GLint    , 1>(GL_NUM_EXTENSIONS);
    requestState<GLenum   , GL_NUM_EXTENSIONS>(GL_EXTENSIONS);
    requestState<GLint    , 1>(GL_NUM_PROGRAM_BINARY_FORMATS);
    requestState<GLenum   , GL_NUM_PROGRAM_BINARY_FORMATS>(GL_PROGRAM_BINARY_FORMATS);  
    requestState<GLint    , 1>(GL_NUM_SHADER_BINARY_FORMATS);
    requestState<GLenum   , GL_NUM_SHADER_BINARY_FORMATS>(GL_SHADER_BINARY_FORMATS);
    requestState<GLint    , 1>(GL_AUX_BUFFERS);
    requestState<GLboolean, 1>(GL_DOUBLEBUFFER);
    requestState<GLboolean, 1>(GL_INDEX_MODE);
    requestState<GLfloat  , 2>(GL_POINT_SIZE_RANGE);
    requestState<GLfloat  , 1>(GL_POINT_SIZE_GRANULARITY);
    requestState<GLboolean, 1>(GL_RGBA_MODE);
    requestState<GLboolean, 1>(GL_STEREO);
    requestState<GLint    , 1>(GL_SUBPIXEL_BITS, { { 4 } }, ExpectedType::Minimum);
    requestState<GLfloat  , 2>(GL_LINE_WIDTH_RANGE);
    requestState<GLfloat  , 1>(GL_LINE_WIDTH_GRANULARITY);

    std::cout << std::endl << "Implementation-Dependent Pixel-Depth States" << std::endl;
    requestState<GLint    , 1>(GL_RED_BITS);
    requestState<GLint    , 1>(GL_GREEN_BITS);
    requestState<GLint    , 1>(GL_BLUE_BITS);
    requestState<GLint    , 1>(GL_ALPHA_BITS);
    requestState<GLint    , 1>(GL_INDEX_BITS);
    requestState<GLint    , 1>(GL_DEPTH_BITS);
    requestState<GLint    , 1>(GL_STENCIL_BITS);
    requestState<GLint    , 1>(GL_ACCUM_RED_BITS);
    requestState<GLint    , 1>(GL_ACCUM_GREEN_BITS);
    requestState<GLint    , 1>(GL_ACCUM_BLUE_BITS);
    requestState<GLint    , 1>(GL_ACCUM_ALPHA_BITS);

    std::cout << std::endl << "Miscellaneous States" << std::endl;
    requestState<GLint    , 1>(GL_LIST_BASE, { { 0 } });
    requestState<GLint    , 1>(GL_LIST_INDEX, { { 0 } });
    requestState<GLint    , 1>(GL_LIST_MODE, { { 0 } });
    requestState<GLint    , 1>(GL_ATTRIB_STACK_DEPTH, { { 0 } });
    requestState<GLint    , 1>(GL_CLIENT_ATTRIB_STACK_DEPTH, { { 0 } });
    requestState<GLint    , 1>(GL_NAME_STACK_DEPTH, { { 0 } });
    requestState<GLenum   , 1>(GL_RENDER_MODE, { { GL_RENDER } });
    requestState<GLint    , 1>(GL_SELECTION_BUFFER_SIZE, { { 0 } });
    requestState<GLint    , 1>(GL_FEEDBACK_BUFFER_SIZE, { { 0 } });
    requestState<GLenum   , 1>(GL_FEEDBACK_BUFFER_TYPE, { { GL_2D } });

    std::cout << std::endl << "Active Texture States" << std::endl;
    requestState<GLenum   , 1>(GL_ACTIVE_TEXTURE, { { GL_TEXTURE0 } });
    requestState<GLint    , 1>(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, { { 48 } }, ExpectedType::Minimum);

    std::cout << std::endl << "Line Width States" << std::endl;
    requestState<GLfloat  , 1>(GL_LINE_WIDTH, { { 1 } });
    requestState<GLfloat  , 2>(GL_ALIASED_LINE_WIDTH_RANGE);
    requestState<GLfloat  , 2>(GL_SMOOTH_LINE_WIDTH_RANGE);
    requestState<GLint    , 1>(GL_SMOOTH_LINE_WIDTH_GRANULARITY);
    requestState<GLboolean, 1>(GL_LINE_SMOOTH, { { GL_FALSE } });

    std::cout << std::endl << "Bind Buffer States" << std::endl;
    requestState<GLint    , 1>(GL_ARRAY_BUFFER_BINDING, { { 0 } });
    requestState<GLint    , 1>(GL_ATOMIC_COUNTER_BUFFER_BINDING, { { 0 } });
    requestState<GLint    , 1>(GL_COPY_READ_BUFFER_BINDING, { { 0 } });
    requestState<GLint    , 1>(GL_COPY_WRITE_BUFFER_BINDING, { { 0 } });
    requestState<GLint    , 1>(GL_DRAW_INDIRECT_BUFFER_BINDING, { { 0 } });
    requestState<GLint    , 1>(GL_ELEMENT_ARRAY_BUFFER_BINDING, { { 0 } });
    requestState<GLint    , 1>(GL_PIXEL_PACK_BUFFER_BINDING, { { 0 } });
    requestState<GLint    , 1>(GL_PIXEL_UNPACK_BUFFER_BINDING, { { 0 } });
    requestState<GLint    , 1>(GL_SHADER_STORAGE_BUFFER_BINDING, { { 0 } });
    requestState<GLint    , 1>(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, { { 1 } });
    requestState<GLint    , 1>(GL_SHADER_STORAGE_BUFFER_SIZE, { { 0 } });
    requestState<GLint    , 1>(GL_SHADER_STORAGE_BUFFER_START, { { 0 } });
    requestState<GLint    , 1>(GL_TRANSFORM_FEEDBACK_BUFFER_BINDING, { { 0 } });
    requestState<GLint    , 1>(GL_TRANSFORM_FEEDBACK_BUFFER_SIZE, { { 0 } });
    requestState<GLint    , 1>(GL_TRANSFORM_FEEDBACK_BUFFER_START, { { 0 } });
    requestState<GLint    , 1>(GL_UNIFORM_BUFFER_BINDING, { { 0 } });
    requestState<GLint    , 1>(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, { { 1 } });
    requestState<GLint    , 1>(GL_UNIFORM_BUFFER_SIZE, { { 0 } });
    requestState<GLint    , 1>(GL_UNIFORM_BUFFER_START, { { 0 } });
    requestState<GLint    , 1>(GL_DISPATCH_INDIRECT_BUFFER_BINDING, { { 0 } });

    std::cout << std::endl << "Blend Color States" << std::endl;
    requestState<GLfloat  , 4>(GL_BLEND_COLOR);

    std::cout << std::endl << "Blend Func States" << std::endl;
    requestState<GLenum   , 1>(GL_BLEND_DST_RGB, { { GL_ZERO } });
    requestState<GLenum   , 1>(GL_BLEND_DST_ALPHA, { { GL_ZERO } });
    requestState<GLenum   , 1>(GL_BLEND_SRC_RGB, { { GL_ONE } });
    requestState<GLenum   , 1>(GL_BLEND_SRC_ALPHA, { { GL_ONE } });
    requestState<GLboolean, 1>(GL_BLEND, { { GL_FALSE } });
    requestState<GLint    , 1>(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS, { { 1 } }, ExpectedType::Minimum);
    
    std::cout << std::endl << "Blend Equation Separate States" << std::endl;
    requestState<GLenum   , 1>(GL_BLEND_EQUATION_ALPHA);
    requestState<GLenum   , 1>(GL_BLEND_EQUATION_RGB);

    std::cout << std::endl << "Client Active Texture States" << std::endl;
    requestState<GLenum   , 1>(GL_CLIENT_ACTIVE_TEXTURE, { { GL_TEXTURE0 } });
    requestState<GLint    , 1>(GL_MAX_TEXTURE_COORDS, { { 2 } }, ExpectedType::Minimum);

    std::cout << std::endl << "Clip Plane States" << std::endl;
    requestState<GLboolean, 1, GL_MAX_CLIP_PLANES>(GL_CLIP_PLANE0, { { GL_FALSE } });
    requestState<GLint    , 1>(GL_MAX_CLIP_PLANES, { { 6 } }, ExpectedType::Minimum);

    std::cout << std::endl << "Color Pointer States" << std::endl;
    requestState<GLboolean, 1>(GL_COLOR_ARRAY, { { GL_FALSE } });
    requestState<GLint    , 1>(GL_COLOR_ARRAY_SIZE, { { 4 } });
    requestState<GLenum   , 1>(GL_COLOR_ARRAY_TYPE, { { GL_FLOAT } });
    requestState<GLint    , 1>(GL_COLOR_ARRAY_STRIDE, { { 0 } });
    requestState<GLint    , 1>(GL_COLOR_ARRAY_BUFFER_BINDING, { { 0 } });

    std::cout << std::endl << "Secondary Color States" << std::endl;
    requestState<GLfloat  , 4>(GL_CURRENT_SECONDARY_COLOR, { { 0, 0, 0, 0 } });
    requestState<GLboolean, 1>(GL_COLOR_SUM);

    std::cout << std::endl << "Convolution Filter States" << std::endl;
    requestState<GLboolean, 1>(GL_CONVOLUTION_1D, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_CONVOLUTION_2D, { { GL_FALSE } });

    std::cout << std::endl << "Raster Pos States" << std::endl;
    requestState<GLfloat  , 4>(GL_CURRENT_RASTER_POSITION, { { 0, 0, 0, 1 } });
    requestState<GLboolean, 1>(GL_CURRENT_RASTER_POSITION_VALID, { { GL_TRUE } });
    requestState<GLfloat  , 1>(GL_CURRENT_RASTER_DISTANCE, { { 0 } });
    requestState<GLfloat  , 4>(GL_CURRENT_RASTER_COLOR, { { 1, 1, 1, 1 } });
    requestState<GLfloat  , 4>(GL_CURRENT_RASTER_SECONDARY_COLOR, { { 1, 1, 1, 1 } });
    requestState<GLint    , 1>(GL_CURRENT_RASTER_INDEX, { { 1 } });
    requestState<GLfloat  , 4>(GL_CURRENT_RASTER_TEXTURE_COORDS, { { 0, 0, 0, 1 } });

    std::cout << std::endl << "Bind Framebuffer States" << std::endl;
    requestState<GLenum   , 1>(GL_DRAW_FRAMEBUFFER_BINDING, { { GL_ZERO } });
    requestState<GLenum   , 1>(GL_READ_FRAMEBUFFER_BINDING, { { GL_ZERO } });

    std::cout << std::endl << "Edge Flag Pointer States" << std::endl;
    requestState<GLboolean, 1>(GL_EDGE_FLAG_ARRAY, { { GL_FALSE } });
    requestState<GLint    , 1>(GL_EDGE_FLAG_ARRAY_STRIDE, { { 0 } });
    requestState<GLint    , 1>(GL_EDGE_FLAG_ARRAY_BUFFER_BINDING, { { 0 } });

    std::cout << std::endl << "Fog Coord Pointer States" << std::endl;
    requestState<GLboolean, 1>(GL_FOG_COORD_ARRAY, { { GL_FALSE } });
    requestState<GLint    , 1>(GL_FOG_COORD_ARRAY_STRIDE, { { 0 } });
    requestState<GLenum   , 1>(GL_FOG_COORD_ARRAY_TYPE, { { GL_FLOAT } });
    requestState<GLint    , 1>(GL_FOG_COORD_ARRAY_BUFFER_BINDING, { { 0 } });

    std::cout << std::endl << "Fog States" << std::endl;
    requestState<GLboolean, 1>(GL_FOG, { { GL_FALSE } });
    requestState<GLfloat  , 4>(GL_FOG_COLOR, { { 0, 0, 0, 0 } });
    requestState<GLfloat  , 1>(GL_FOG_INDEX, { { 0 } });
    requestState<GLfloat  , 1>(GL_FOG_DENSITY, { { 1 } });
    requestState<GLfloat  , 1>(GL_FOG_START, { { 0 } });
    requestState<GLfloat  , 1>(GL_FOG_END, { { 1 } });
    requestState<GLenum   , 1>(GL_FOG_MODE, { { GL_EXP } });
    requestState<GLenum   , 1>(GL_FOG_COORD_SRC, { { GL_FRAGMENT_DEPTH } });

    std::cout << std::endl << "Hint States" << std::endl;
    requestState<GLenum   , 1>(GL_FOG_HINT, { { GL_DONT_CARE } });
    requestState<GLenum   , 1>(GL_FRAGMENT_SHADER_DERIVATIVE_HINT, { { GL_DONT_CARE } });
    requestState<GLenum   , 1>(GL_GENERATE_MIPMAP_HINT, { { GL_DONT_CARE } });
    requestState<GLenum   , 1>(GL_LINE_SMOOTH_HINT, { { GL_DONT_CARE } });
    requestState<GLenum   , 1>(GL_PERSPECTIVE_CORRECTION_HINT, { { GL_DONT_CARE } });
    requestState<GLenum   , 1>(GL_POINT_SMOOTH_HINT, { { GL_DONT_CARE } });
    requestState<GLenum   , 1>(GL_POLYGON_SMOOTH_HINT, { { GL_DONT_CARE } });
    requestState<GLenum   , 1>(GL_TEXTURE_COMPRESSION_HINT, { { GL_DONT_CARE } });

    std::cout << std::endl << "Pixel Representation States" << std::endl;
    requestState<GLenum   , 1>(GL_IMPLEMENTATION_COLOR_READ_FORMAT);
    requestState<GLenum   , 1>(GL_IMPLEMENTATION_COLOR_READ_TYPE);

    std::cout << std::endl << "Index Pointer States" << std::endl;
    requestState<GLboolean, 1>(GL_INDEX_ARRAY, { { GL_FALSE } });
    requestState<GLenum   , 1>(GL_INDEX_ARRAY_TYPE, { { GL_FLOAT } });
    requestState<GLint, 1>(GL_INDEX_ARRAY_STRIDE, { { 0 } });
    requestState<GLint    , 1>(GL_INDEX_ARRAY_BUFFER_BINDING, { { 0 } });

    std::cout << std::endl << "Light States" << std::endl;
    requestState<GLboolean, 1, GL_MAX_LIGHTS>(GL_LIGHT0, { { GL_FALSE } });
    requestState<GLfloat  , 4>(GL_LIGHT_MODEL_AMBIENT, { { 0.2f, 0.2f, 0.2f, 1.0f } });
    requestState<GLenum   , 1>(GL_LIGHT_MODEL_COLOR_CONTROL, { { GL_SINGLE_COLOR } });
    requestState<GLboolean, 1>(GL_LIGHT_MODEL_LOCAL_VIEWER, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_LIGHT_MODEL_TWO_SIDE, { { GL_FALSE } });

    std::cout << std::endl << "Map1 States" << std::endl;
    requestState<GLboolean, 1>(GL_MAP1_VERTEX_3, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_MAP1_VERTEX_4, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_MAP1_INDEX, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_MAP1_COLOR_4, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_MAP1_NORMAL, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_MAP1_TEXTURE_COORD_1, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_MAP1_TEXTURE_COORD_2, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_MAP1_TEXTURE_COORD_3, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_MAP1_TEXTURE_COORD_4, { { GL_FALSE } });

    std::cout << std::endl << "Map2 States" << std::endl;
    requestState<GLboolean, 1>(GL_MAP2_VERTEX_3, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_MAP2_VERTEX_4, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_MAP2_INDEX, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_MAP2_COLOR_4, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_MAP2_NORMAL, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_MAP2_TEXTURE_COORD_1, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_MAP2_TEXTURE_COORD_2, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_MAP2_TEXTURE_COORD_3, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_MAP2_TEXTURE_COORD_4, { { GL_FALSE } });

    std::cout << std::endl << "Uniform States" << std::endl;
    requestState<GLint    , 1>(GL_MAX_COMBINED_COMPUTE_UNIFORM_COMPONENTS, { { 1 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_COMBINED_FRAGMENT_UNIFORM_COMPONENTS, { { 1 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_COMBINED_GEOMETRY_UNIFORM_COMPONENTS, { { 1 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_COMBINED_UNIFORM_BLOCKS, { { 70 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_COMBINED_VERTEX_UNIFORM_COMPONENTS, { { 1 } }, ExpectedType::Minimum);

    std::cout << std::endl << "Compute Shader States" << std::endl;
    requestState<GLint    , 1>(GL_MAX_COMPUTE_ATOMIC_COUNTER_BUFFERS);
    requestState<GLint    , 1>(GL_MAX_COMPUTE_ATOMIC_COUNTERS);
    requestState<GLint    , 1>(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS);
    requestState<GLint    , 1>(GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS, { { 16 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_COMPUTE_UNIFORM_BLOCKS, { { 14 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_COMPUTE_UNIFORM_COMPONENTS, { { 1 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_COMPUTE_WORK_GROUP_COUNT);
    requestState<GLint    , 1>(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS);
    requestState<GLint    , 1>(GL_MAX_COMPUTE_WORK_GROUP_SIZE);

    std::cout << std::endl << "Draw Buffer States" << std::endl;
    requestState<GLint    , 1>(GL_DRAW_BUFFER);
    requestState<GLint    , 1>(GL_MAX_DRAW_BUFFERS, { { 8 } }, ExpectedType::Minimum);

    std::cout << std::endl << "Draw Range Elements States" << std::endl;
    requestState<GLint    , 1>(GL_MAX_ELEMENTS_INDICES);
    requestState<GLint    , 1>(GL_MAX_ELEMENTS_VERTICES);

    std::cout << std::endl << "Fragment Shader States" << std::endl;
    requestState<GLint    , 1>(GL_MAX_FRAGMENT_ATOMIC_COUNTERS);
    requestState<GLint    , 1>(GL_MAX_FRAGMENT_INPUT_COMPONENTS, { { 128 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS);
    requestState<GLint    , 1>(GL_MAX_FRAGMENT_UNIFORM_BLOCKS, { { 12 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_FRAGMENT_UNIFORM_COMPONENTS, { { 1024 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_FRAGMENT_UNIFORM_VECTORS, { { 256 } }, ExpectedType::Minimum);

    std::cout << std::endl << "Framebuffer Parameter States" << std::endl;
    requestState<GLint    , 1>(GL_MAX_FRAMEBUFFER_HEIGHT, { { 16384 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_FRAMEBUFFER_LAYERS, { { 2048 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_FRAMEBUFFER_SAMPLES, { { 4 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_FRAMEBUFFER_WIDTH, { { 16384 } }, ExpectedType::Minimum);

    std::cout << std::endl << "Geometry Shader States" << std::endl;
    requestState<GLint    , 1>(GL_MAX_GEOMETRY_ATOMIC_COUNTERS);
    requestState<GLint    , 1>(GL_MAX_GEOMETRY_INPUT_COMPONENTS, { { 64 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_GEOMETRY_OUTPUT_COMPONENTS, { { 128 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_GEOMETRY_SHADER_STORAGE_BLOCKS);
    requestState<GLint    , 1>(GL_MAX_GEOMETRY_TEXTURE_IMAGE_UNITS, { { 16 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_GEOMETRY_UNIFORM_BLOCKS, { { 12 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_GEOMETRY_UNIFORM_COMPONENTS, { { 1024 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_GEOMETRY_OUTPUT_VERTICES, { { 256 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_GEOMETRY_SHADER_INVOCATIONS, { { 32 } }, ExpectedType::Minimum);

    std::cout << std::endl << "Texel Offset States" << std::endl;
    requestState<GLint    , 1>(GL_MAX_PROGRAM_TEXEL_OFFSET, { { 7 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MIN_PROGRAM_TEXEL_OFFSET, { { -8 } }, ExpectedType::Maximum);

    std::cout << std::endl << "Tesselation Max States" << std::endl;
    requestState<GLint    , 1>(GL_MAX_TESS_CONTROL_ATOMIC_COUNTERS);
    requestState<GLint    , 1>(GL_MAX_TESS_CONTROL_SHADER_STORAGE_BLOCKS);
    requestState<GLint    , 1>(GL_MAX_TESS_EVALUATION_ATOMIC_COUNTERS);
    requestState<GLint    , 1>(GL_MAX_TESS_EVALUATION_SHADER_STORAGE_BLOCKS);

    std::cout << std::endl << "Texture Max States" << std::endl;
    requestState<GLint    , 1>(GL_MAX_TEXTURE_BUFFER_SIZE, { { 65536 } }, ExpectedType::Minimum);
    requestState<GLfloat  , 1>(GL_MAX_TEXTURE_IMAGE_UNITS, { { 16 } }, ExpectedType::Minimum);
    requestState<GLfloat  , 1>(GL_MAX_TEXTURE_LOD_BIAS, { { 2.0f } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_TEXTURE_UNITS, { { 2 } }, ExpectedType::Minimum);

    std::cout << std::endl << "Vertex Max States" << std::endl;
    requestState<GLint    , 1>(GL_MAX_VERTEX_ATOMIC_COUNTERS);
    requestState<GLint    , 1>(GL_MAX_VERTEX_ATTRIB_BINDINGS);
    requestState<GLint    , 1>(GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET);
    requestState<GLint    , 1>(GL_MAX_VERTEX_ATTRIBS, { { 16 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_VERTEX_OUTPUT_COMPONENTS, { { 64 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS);
    requestState<GLint    , 1>(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, { { 16 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_VERTEX_UNIFORM_BLOCKS, { { 12 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_VERTEX_UNIFORM_COMPONENTS, { { 1024 } }, ExpectedType::Minimum);
    requestState<GLint    , 1>(GL_MAX_VERTEX_UNIFORM_VECTORS, { { 256 } }, ExpectedType::Minimum);

    std::cout << std::endl << "Point Parameter States" << std::endl;
    requestState<GLfloat  , 3>(GL_POINT_DISTANCE_ATTENUATION);
    requestState<GLfloat  , 1>(GL_POINT_FADE_THRESHOLD_SIZE);
    requestState<GLfloat  , 1>(GL_POINT_SIZE_MAX, { { 0.0 } }, ExpectedType::Minimum);
    requestState<GLfloat  , 1>(GL_POINT_SIZE_MIN, { { 1.0 } }, ExpectedType::Maximum);
    requestState<GLenum   , 1>(GL_POINT_SPRITE_COORD_ORIGIN, { { GL_UPPER_LEFT } });
    requestState<GLboolean, 1>(GL_POINT_SPRITE, { { GL_FALSE } });
    requestState<GLfloat  , 1>(GL_SMOOTH_POINT_SIZE_GRANULARITY);
    requestState<GLfloat  , 2>(GL_SMOOTH_POINT_SIZE_RANGE);

    std::cout << std::endl << "Sample States" << std::endl;
    requestState<GLint    , 1>(GL_SAMPLE_BUFFERS);
    requestState<GLboolean, 1>(GL_SAMPLE_COVERAGE_INVERT);
    requestState<GLfloat  , 1>(GL_SAMPLE_COVERAGE_VALUE);
    requestState<GLint    , 1>(GL_SAMPLES);

    std::cout << std::endl << "Secondary Color Pointer States" << std::endl;
    requestState<GLboolean, 1>(GL_SECONDARY_COLOR_ARRAY, { { GL_FALSE } });
    requestState<GLint    , 1>(GL_SECONDARY_COLOR_ARRAY_BUFFER_BINDING, { { 0 } });
    requestState<GLint    , 1>(GL_SECONDARY_COLOR_ARRAY_SIZE, { { 3 } });
    requestState<GLint    , 1>(GL_SECONDARY_COLOR_ARRAY_STRIDE, { { 0 } });
    requestState<GLenum   , 1>(GL_SECONDARY_COLOR_ARRAY_TYPE, { { GL_FLOAT } });

    std::cout << std::endl << "Stencil Seperate States" << std::endl;
    requestState<GLenum   , 1>(GL_STENCIL_BACK_FAIL, { { GL_KEEP } });
    requestState<GLenum   , 1>(GL_STENCIL_BACK_FUNC, { { GL_ALWAYS } });
    requestState<GLenum   , 1>(GL_STENCIL_BACK_PASS_DEPTH_FAIL, { { GL_KEEP } });
    requestState<GLenum   , 1>(GL_STENCIL_BACK_PASS_DEPTH_PASS, { { GL_KEEP } });
    requestState<GLint    , 1>(GL_STENCIL_BACK_REF, { { 0 } });
    //requestState<GLint    , 1>(GL_STENCIL_BACK_VALUE_MASK, { { 1's } });
    //requestState<GLint    , 1>(GL_STENCIL_BACK_WRITEMASK, { { 1's } });

    std::cout << std::endl << "Texture Type States" << std::endl;
    requestState<GLboolean, 1>(GL_TEXTURE_1D, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_TEXTURE_2D, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_TEXTURE_3D, { { GL_FALSE } });
    requestState<GLint    , 1>(GL_TEXTURE_BINDING_1D, { { 0 } });
    requestState<GLint    , 1>(GL_TEXTURE_BINDING_1D_ARRAY, { { 0 } });
    requestState<GLint    , 1>(GL_TEXTURE_BINDING_2D, { { 0 } });
    requestState<GLint    , 1>(GL_TEXTURE_BINDING_2D_ARRAY, { { 0 } });
    requestState<GLint    , 1>(GL_TEXTURE_BINDING_2D_MULTISAMPLE, { { 0 } });
    requestState<GLint    , 1>(GL_TEXTURE_BINDING_2D_MULTISAMPLE_ARRAY, { { 0 } });
    requestState<GLint    , 1>(GL_TEXTURE_BINDING_3D, { { 0 } });
    requestState<GLint    , 1>(GL_TEXTURE_BINDING_BUFFER, { { 0 } });
    requestState<GLint    , 1>(GL_TEXTURE_BINDING_CUBE_MAP, { { 0 } });
    requestState<GLint    , 1>(GL_TEXTURE_BINDING_RECTANGLE, { { 0 } });
    requestState<GLint    , 1>(GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT, { { 1 } });
    requestState<GLint    , 1>(GL_TEXTURE_COORD_ARRAY_BUFFER_BINDING, { { 0 } });
    requestState<GLboolean, 1>(GL_TEXTURE_CUBE_MAP, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_TEXTURE_GEN_Q, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_TEXTURE_GEN_R, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_TEXTURE_GEN_S, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_TEXTURE_GEN_T, { { GL_FALSE } });

    std::cout << std::endl << "Transpose Matrix States" << std::endl;
    requestState<GLfloat  , 16>(GL_TRANSPOSE_COLOR_MATRIX); // GLdouble
    requestState<GLfloat  , 16>(GL_TRANSPOSE_MODELVIEW_MATRIX); // GLdouble
    requestState<GLfloat  , 16>(GL_TRANSPOSE_PROJECTION_MATRIX); // GLdouble
    requestState<GLfloat  , 16>(GL_TRANSPOSE_TEXTURE_MATRIX); // GLdouble

    std::cout << std::endl << "Vertex Binding States" << std::endl;
    requestState<GLint    , 1>(GL_VERTEX_ARRAY_BINDING, { { 0 } });
    requestState<GLint    , 1>(GL_VERTEX_ARRAY_BUFFER_BINDING, { { 0 } });
    requestState<GLint    , 1>(GL_VERTEX_BINDING_DIVISOR);
    requestState<GLint    , 1>(GL_VERTEX_BINDING_OFFSET);
    requestState<GLint    , 1>(GL_VERTEX_BINDING_STRIDE);

    std::cout << std::endl << "Ungrouped States" << std::endl;
    requestState<GLfloat  , 2>(GL_ALIASED_POINT_SIZE_RANGE);
    requestState<GLboolean, 1>(GL_COLOR_TABLE);
    requestState<GLenum   , 1>(GL_CONTEXT_FLAGS);
    requestState<GLfloat  , 1>(GL_CURRENT_FOG_COORD, { { 0 } }); // GLdouble
    requestState<GLint    , 1>(GL_CURRENT_PROGRAM);
    requestState<GLint    , 1>(GL_DEBUG_GROUP_STACK_DEPTH);
    requestState<GLfloat  , 2>(GL_DEPTH_RANGE, { { 0, 1 } });
    requestState<GLboolean, 1>(GL_HISTOGRAM, { { GL_FALSE } });
    requestState<GLint    , 1>(GL_MIN_MAP_BUFFER_ALIGNMENT, { { 64 } });
    requestState<GLboolean, 1>(GL_MINMAX, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_NORMALIZE, { { GL_FALSE } });
    requestState<GLint    , 1>(GL_NORMAL_ARRAY_BUFFER_BINDING, { { 0 } });
    requestState<GLboolean, 1>(GL_POST_COLOR_MATRIX_COLOR_TABLE, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_POST_CONVOLUTION_COLOR_TABLE, { { GL_FALSE } });
    requestState<GLint    , 1>(GL_PRIMITIVE_RESTART_INDEX, { { 0 } });
    requestState<GLint    , 1>(GL_PROGRAM_PIPELINE_BINDING);
    requestState<GLboolean, 1>(GL_PROGRAM_POINT_SIZE, { { GL_FALSE } });
    requestState<GLenum   , 1>(GL_PROVOKING_VERTEX, { { GL_LAST_VERTEX_CONVENTION } });
    requestState<GLint    , 1>(GL_RENDERBUFFER_BINDING, { { 0 } });
    requestState<GLboolean, 1>(GL_RESCALE_NORMAL);
    requestState<GLint    , 1>(GL_SAMPLER_BINDING, { { 0 } });
    requestState<GLboolean, 1>(GL_SEPARABLE_2D, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_SHADER_COMPILER, { { GL_TRUE } });
    requestState<GLint    , 1>(GL_TIMESTAMP);
    requestState<GLboolean, 1>(GL_VERTEX_PROGRAM_POINT_SIZE, { { GL_FALSE } });
    requestState<GLboolean, 1>(GL_VERTEX_PROGRAM_TWO_SIDE, { { GL_FALSE } });
    requestState<GLenum   , 1>(GL_LAYER_PROVOKING_VERTEX);
    requestState<GLenum   , 1>(GL_VIEWPORT_INDEX_PROVOKING_VERTEX);

    requestState<GLint    , 1>(GL_MAJOR_VERSION);
    requestState<GLint    , 1>(GL_MINOR_VERSION);


    std::cout << std::endl << std::endl << "[QUERYING STATE VALUES - UNGROUPED/TODO]" << std::endl;


    std::cout << std::endl
        << "OpenGL Version:  " << ContextInfo::version() << std::endl
        << "OpenGL Vendor:   " << ContextInfo::vendor() << std::endl
        << "OpenGL Renderer: " << ContextInfo::renderer() << std::endl
        << "OpenGL Revision: " << Meta::glRevision() << " (gl.xml)" << std::endl << std::endl;

    glfwTerminate();
    return 0;
}
