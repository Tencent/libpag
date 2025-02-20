
#include <glbinding/ProcAddress.h>

#ifdef WIN32
    #include <string>
    #include <tchar.h>
    #include <windows.h>
#elif __APPLE__
    #include <cassert>
    #include <string>
    #include <dlfcn.h>
#else
    #include <GL/glx.h>
#endif

namespace glbinding 
{

ProcAddress getProcAddress(const char * name)
{
#ifdef WIN32

    using PROCADDRESS = void (__stdcall *)();
    auto procAddress = reinterpret_cast<PROCADDRESS>(wglGetProcAddress(name));

    if (procAddress == nullptr)
    {
        static auto module = LoadLibrary(_T("OPENGL32.DLL"));
        procAddress = reinterpret_cast<PROCADDRESS>(::GetProcAddress(module, name));
    }

#elif __APPLE__

    using PROCADDRESS = void *;

    auto library = dlopen("/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL", RTLD_LAZY);
    assert(library != nullptr);

    auto symbol = dlsym(library, name);

    auto procAddress = reinterpret_cast<PROCADDRESS>(symbol);

#else

    using PROCADDRESS = void (*)();
    auto procAddress = reinterpret_cast<PROCADDRESS>(glXGetProcAddress(reinterpret_cast<const unsigned char*>(name)));

#endif

    return reinterpret_cast<ProcAddress>(procAddress);
}

} // namespace glbinding
