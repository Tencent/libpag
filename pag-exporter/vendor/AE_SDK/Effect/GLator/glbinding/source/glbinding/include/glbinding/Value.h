#pragma once

#include <glbinding/glbinding_api.h>

#include <vector>

#include <glbinding/AbstractValue.h>
#include <glbinding/gl/types.h>

namespace glbinding 
{

template <typename T>
class Value : public AbstractValue
{
public:
    Value(const T & value);

    Value & operator=(const Value &) = delete;

    virtual void printOn(std::ostream & stream) const override;

protected:
    T value;
};

template <> GLBINDING_API void Value<gl::GLenum>::printOn(std::ostream & stream) const;
//template <> GLBINDING_API void Value<gl::GLbitfield>::printOn(std::ostream & stream) const;
template <> GLBINDING_API void Value<gl::GLboolean>::printOn(std::ostream & stream) const;
template <> GLBINDING_API void Value<const gl::GLubyte *>::printOn(std::ostream & stream) const;
template <> GLBINDING_API void Value<const gl::GLchar *>::printOn(std::ostream & stream) const;
template <> GLBINDING_API void Value<gl::GLuint_array_2>::printOn(std::ostream & stream) const;

template <typename Argument>
AbstractValue * createValue(Argument argument);

template <typename... Arguments>
std::vector<AbstractValue*> createValues(Arguments&&... arguments);

} // namespace glbinding

#include <glbinding/Value.hpp>
