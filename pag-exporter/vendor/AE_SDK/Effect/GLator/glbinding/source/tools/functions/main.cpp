
#include <iostream>
#include <map>
#include <set>
#include <vector>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glbinding/Meta.h>
#include <glbinding/AbstractFunction.h>
#include <glbinding/ContextInfo.h>
#include <glbinding/Version.h>
#include <glbinding/Binding.h>

#include <glbinding/gl/gl.h>


using namespace gl;
using namespace glbinding;


void error(int errnum, const char * errmsg)
{
    std::cerr << errnum << ": " << errmsg << std::endl;
}

inline void coutFunc(const AbstractFunction * func)
{
    std::cout << "\t(0x" << reinterpret_cast<void *>(func->address()) << ") " << func->name() << std::endl;
}

int main()
{
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

    // gather available extensions

    unsigned int resolved(0);
    unsigned int assigned(0);

    std::set<std::string> unknownExts;

    const std::set<GLextension> & supportedExts = ContextInfo::extensions(&unknownExts);
    const std::set<GLextension> & allExts = Meta::extensions();

    // sort extensions by version

    std::map<Version, std::set<GLextension>> extsByVer;
    std::map<Version, int> suppByVer; // num supported exts by version

    for (GLextension ext : allExts)
    {

        const Version v = Meta::getRequiringVersion(ext);
        extsByVer[v].insert(ext);
        suppByVer[v] = 0;
    }

    // go through all extensions by version and show functions

    std::map<GLextension, std::set<const AbstractFunction *>> funcsByExt;
    std::set<const AbstractFunction *> nonExtFuncs;

    for (AbstractFunction * func : Binding::functions())
    {
        if (func->isResolved())
            ++resolved;

        if (func->extensions().empty())
            nonExtFuncs.insert(func);
        else
        {
            for (GLextension ext : func->extensions())
                funcsByExt[ext].insert(func);
            ++assigned;
        }
    }

    // print all extensions with support info and functions 

    for (auto i : extsByVer)
    {   
        std::cout << std::endl << std::endl << "[" << i.first << " EXTENSIONS]" << std::endl;
        for (GLextension ext : i.second)
        {
            const bool supported = supportedExts.find(ext) != supportedExts.cend();
            if (supported)
                ++suppByVer[i.first];

            std::cout << std::endl << Meta::getString(ext) << (supported ? " (supported)" : "") << std::endl;
            if (funcsByExt.find(ext) != funcsByExt.cend())
                for (auto func : funcsByExt[ext])
                    coutFunc(func);
        }
    }

    // show unknown extensions

    std::cout << std::endl << std::endl << "[EXTENSIONS NOT COVERED BY GLBINDING]" << std::endl << std::endl;
    for (const std::string & ext : unknownExts)
        std::cout << ext << std::endl;

    // show non ext functions

    std::cout << std::endl << std::endl << "[NON-EXTENSION OR NOT-COVERED-EXTENSION FUNCTIONS]" << std::endl << std::endl;
    for (const AbstractFunction * func : nonExtFuncs)
        coutFunc(func);

    // print summary

    std::cout << std::endl << std::endl << "[SUMMARY]" << std::endl << std::endl;

    std::cout << "# Functions:     " << resolved << " of " << Binding::size() << " resolved"
        << " (" << (Binding::size() - resolved) << " unresolved)" << std::endl;

    std::cout << "                 " << assigned << " assigned to extensions";

    if (!Meta::glByStrings())
        std::cout << " (none assigned, since the glbinding used was compiled without GL_BY_STRINGS support)." << std::endl;
    else
        std::cout << "." << std::endl;

    std::cout << std::endl;
    std::cout << "# Extensions:    " << (supportedExts.size() + unknownExts.size()) << " of " << allExts.size() + unknownExts.size() << " supported"
        << " (" << unknownExts.size() << " of which are unknown to glbinding)" << std::endl;

    for (auto p : extsByVer)
        std::cout << "  # " << p.first << " assoc.:  " << suppByVer[p.first] << " / " << p.second.size() << std::endl;

    // print some gl infos (query)

    std::cout << std::endl
        << "OpenGL Version:  " << ContextInfo::version() << std::endl
        << "OpenGL Vendor:   " << ContextInfo::vendor() << std::endl
        << "OpenGL Renderer: " << ContextInfo::renderer() << std::endl
        << "OpenGL Revision: " << Meta::glRevision() << " (gl.xml)" << std::endl << std::endl;

    glfwTerminate();
    return 0;
}
