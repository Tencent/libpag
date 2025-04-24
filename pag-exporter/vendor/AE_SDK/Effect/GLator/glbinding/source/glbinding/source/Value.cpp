
#include <glbinding/Value.h>

#include <sstream>
#include <bitset>

#include <glbinding/Meta.h>

namespace 
{

std::string wrapString(const char * value)
{
    std::stringstream ss;
    ss << "\"";
    ss << (value == nullptr ? "nullptr" : value);
    ss << "\"";
    return ss.str();
}

}

namespace glbinding 
{

template <>
void Value<gl::GLenum>::printOn(std::ostream & stream) const
{
    auto name = Meta::getString(value);
    stream.write(name.c_str(), static_cast<std::streamsize>(name.size()));
}

/*template <>
void Value<gl::GLbitfield>::printOn(std::ostream & stream) const
{
    std::stringstream ss;
    ss << "0x" << std::hex << static_cast<unsigned>(value);
    stream << ss.str();
}*/

template <>
void Value<gl::GLboolean>::printOn(std::ostream & stream) const
{
    auto name = Meta::getString(value);
    stream.write(name.c_str(), static_cast<std::streamsize>(name.size()));
}

template <>
void Value<const gl::GLubyte *>::printOn(std::ostream & stream) const
{
    auto s = wrapString(reinterpret_cast<const char*>(value));
    stream.write(s.c_str(), static_cast<std::streamsize>(s.size()));
}

template <>
void Value<const gl::GLchar *>::printOn(std::ostream & stream) const
{
    auto s = wrapString(reinterpret_cast<const char*>(value));
    stream.write(s.c_str(), static_cast<std::streamsize>(s.size()));
}

template <>
void Value<gl::GLuint_array_2>::printOn(std::ostream & stream) const
{
    std::stringstream ss;
    ss << "{ " << value[0] << ", " << value[1] << " }";
    stream << ss.str();
}

} // namespace glbinding
