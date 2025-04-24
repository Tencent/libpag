#pragma once

#include <glbinding/glbinding_api.h>

#include <string>
#include <set>
#include <vector>

#include <glbinding/ProcAddress.h>
#include <glbinding/callbacks.h>


namespace gl
{
enum class GLextension : int;
}

namespace glbinding 
{

class GLBINDING_API AbstractFunction
{
    friend class Binding;

public:
    AbstractFunction(const char * name);
    virtual ~AbstractFunction();

    const char * name() const;

    void resolveAddress();
    bool isResolved() const;

    ProcAddress address() const;

    const std::set<gl::GLextension> & extensions() const;

public:
    CallbackMask callbackMask() const;
    void setCallbackMask(CallbackMask mask);
    void addCallbackMask(CallbackMask mask);
    void removeCallbackMask(CallbackMask mask);

protected:
    bool isEnabled(CallbackMask mask) const;
    bool isAnyEnabled(CallbackMask mask) const;

    void unresolved() const;

    void before(const FunctionCall & call) const;
    void after(const FunctionCall & call) const;

protected:

    struct State
    {
        State();

        ProcAddress address;
        bool initialized;

        CallbackMask callbackMask;
    };

    bool hasState() const;
    bool hasState(int pos) const;

    State & state() const;
    State & state(int pos) const;

    static void provideState(int pos);
    static void neglectState(int pos);

    static void setStatePos(int pos); // sets thread local state pos used to state access within every instance

protected:
    const char * m_name;

    mutable std::vector<State> m_states;
    
    // to reduce per instance hasState checks and provide/neglect states for all instances, 
    // max pos is used to provide m_states size, which is identical for all instances.
    static int s_maxpos;
};

} // namespace glbinding
