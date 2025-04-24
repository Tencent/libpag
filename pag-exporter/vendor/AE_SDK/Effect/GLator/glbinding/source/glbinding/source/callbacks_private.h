#pragma once

namespace glbinding 
{

class AbstractFunction;
struct FunctionCall;

void unresolved(const AbstractFunction * function);
void before(const FunctionCall & call);
void after(const FunctionCall & call);

} // namespace glbinding
