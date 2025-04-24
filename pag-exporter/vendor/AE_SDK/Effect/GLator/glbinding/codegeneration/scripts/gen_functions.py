from binding import *
from classes.Command import *



functionForwardTemplate = """GLBINDING_API %s %s(%s);"""

importFunctionTemplate = """using %s%s;"""

functionInlineForwardTemplate = """inline GLBINDING_API %s %s(%s)
{
    return gl::%s(%s);
}
"""

functionInlineForwardTemplateRValueCast = """inline GLBINDING_API %s %s(%s)
{
    return static_cast<gl%s::%s>(gl::%s(%s));
}
"""

functionForwardImplementationTemplate = """%s %s(%s)
{
    return gl::%s(%s);
}
"""

functionForwardImplementationTemplateRValueCast = """%s %s(%s)
{
    return static_cast<gl%s::%s>(gl::%s(%s));
}
"""

functionImplementationTemplate = """%s %s(%s)
{
    return glbinding::Binding::%s(%s);
}
"""

functionImplementationTemplateRValueCast = """%s %s(%s)
{
    return static_cast<gl%s::%s>(glbinding::Binding::%s(%s));
}
"""


def namespacify(type, namespace):

    if " void" in type or type.startswith("void"):
        return type

    if type.startswith("const "):
        return "const " + namespace + "::" + type[6:]

    return namespace + "::" + type


def bitfieldType(param):

    return param.groupString if param.groupString else "GLbitfield" 


def paramSignature(param, forward):

    if param.type == "GLbitfield":
        return bitfieldType(param)

#    if forward and param.array is not "":
#        return param.type + " " + param.array
#    else:
    return param.type


def functionMember(function):

    params = ", ".join([function.returntype] + [ paramSignature(p, False) for p in function.params ])
    return 'Function<%s> Binding::%s("%s");' % (params, functionBID(function)[2:], function.name)


def functionDecl(api, function):

    params = ", ".join([namespacify(function.returntype, api)] + [ namespacify(paramSignature(p, True), api) for p in function.params ])
    return tab + "static Function<%s> %s;" % (params, functionBID(function)[2:])


# def functionSignature(api, function, extern = True):

#     params = ", ".join([namespacify(function.returntype, api)] + [ namespacify(paramSignature(p, True), api) for p in function.params ])
#     if extern:
#         return "extern template class Function<%s>;" % (params)
#     else:
#         return "template class Function<%s>;" % (params)


def functionForward(api, function, feature, version):

    params = ", ".join([paramSignature(p, False) + " " + paramPass(p) for p in function.params])
    # paramNames = ", ".join([p.name for p in function.params])

    if feature:
        qualifier = api + "::" 
        return importFunctionTemplate % (qualifier, functionBID(function))
    else:
        return functionForwardTemplate % (function.returntype, functionBID(function), params)


def functionInlineForwardImplementation(function, feature, version):

    params = ", ".join([paramSignature(p, False) + " " + paramPass(p) for p in function.params])
    paramNames = ", ".join([p.name for p in function.params])

    if feature and function.returntype in [ "GLenum", "GLbitfield" ]:
        return functionInlineForwardTemplateRValueCast % (function.returntype, functionBID(function), params,
            version, function.returntype, functionBID(function), paramNames)
    else:
        return functionInlineForwardTemplate % (function.returntype, functionBID(function), params,
            functionBID(function), paramNames)


def functionForwardImplementation(function, feature, version):

    params = ", ".join([paramSignature(p, False) + " " + paramPass(p) for p in function.params])
    paramNames = ", ".join([p.name for p in function.params])

    if feature and function.returntype in [ "GLenum", "GLbitfield" ]:
        return functionForwardImplementationTemplateRValueCast % (function.returntype, functionBID(function), params,
            version, function.returntype, functionBID(function), paramNames)
    else:
        return functionForwardImplementationTemplate % (function.returntype, functionBID(function), params,
            functionBID(function), paramNames)


def functionImplementation(function, feature, version):

    params = ", ".join([paramSignature(p, False) + " " + paramPass(p) for p in function.params])
    paramNames = ", ".join([p.name for p in function.params])

    if feature and function.returntype in [ "GLenum", "GLbitfield" ]:
        return functionImplementationTemplateRValueCast % (function.returntype, functionBID(function), params,
            version, function.returntype, functionBID(function)[2:], paramNames)
    else:
        return functionImplementationTemplate % (function.returntype, functionBID(function), params,
            functionBID(function)[2:], paramNames)


def paramPass(param): 

    return param.name # + param.array

    # this returns a string used for passing the param by its name to a function object.
    # if this is inside a featured function, the param will be cast from featured GLenum 
    # and GLbitfield to gl::GLenum and gl::GLbitfield, required for function object.
    # t = param.type
    #
    # if not feature:
    #    return param.name
    # elif t == "GLenum":
    #    return "static_cast<gl::GLenum>(" + param.name + ")"
    # elif "GLenum" in t and "*" in t:
    #    return ("reinterpret_cast<" + t + ">").replace("GLenum", "gl::GLenum") + "(" + param.name + ")"
    # elif t == "GLbitfield":
    #    return "static_cast<gl::GLbitfield>(" + param.name + ")"
    # elif "GLbitfield" in t and "*" in t:
    #    return ("reinterpret_cast<" + t + ">").replace("GLbitfield", "gl::GLbitfield") + "(" + param.name + ")"
    # else:
    #    return param.name


def functionList(commands):

    #return "std::vector<AbstractFunction*>(&%s, %s)" % (commands[0].name, len(commands))
    return (",\n" + tab).join([ "&"+ functionBID(f)[2:] for f in commands ])


def genFunctionObjects_h(commands, outputdir, outputfile):    

    of = outputfile
    t = template(of)

    status(outputdir + of)

    # extern_templates = set([ functionSignature("gl", f) for f in commands])

    with open(outputdir + of, 'w') as file:
        file.write(t % (
            #"\n".join(sorted(extern_templates)), 
            len(commands),
            "\n".join([ functionDecl("gl", f) for f in commands ])))


def genFunctionObjects_cpp(commands, outputdir, outputfile):

    of = outputfile
    t = template(of)

    status(outputdir + of)

    #extern_templates = set([ functionSignature("gl", f, False) for f in commands])

    with open(outputdir + of, 'w') as file:
        file.write(t.replace("%b", functionBID(commands[0])[2:]).replace("%e", functionBID(commands[-1])[2:]) % (
            #"\n".join(sorted(extern_templates)), 
            #len(commands),
            "\n".join([ functionMember(f) for f in commands ]),
            functionList(commands)
        ))


def genFunctionsAll(api, commands, outputdir, outputfile):

    genFeatureFunctions(api, commands, None, outputdir, outputfile, None)


def genFunctionImplementationsAll(api, commands, outputdir, outputfile):

    genFeatureFunctionImplementations(api, commands, None, outputdir, outputfile, None)


def genFunctionsFeatureGrouped(api, commands, features, outputdir, outputfile):

    # gen functions feature grouped
    for f in features:
        if f.api == "gl": # ToDo: probably seperate for all apis
            genFeatureFunctions(api, commands, f, outputdir, outputfile)
            if f.major > 3 or (f.major == 3 and f.minor >= 2):
                genFeatureFunctions(api, commands, f, outputdir, outputfile, True)
            genFeatureFunctions(api, commands, f, outputdir, outputfile, False, True)


def genFunctionImplementationsFeatureGrouped(api, commands, features, outputdir, outputfile):

    # gen functions feature grouped
    for f in features:
        if f.api == "gl": # ToDo: probably seperate for all apis
            genFeatureFunctionImplementations(api, commands, f, outputdir, outputfile)
            if f.major > 3 or (f.major == 3 and f.minor >= 2):
                genFeatureFunctionImplementations(api, commands, f, outputdir, outputfile, True)
            genFeatureFunctionImplementations(api, commands, f, outputdir, outputfile, False, True)


def genFeatureFunctions(api, commands, feature, outputdir, outputfile, core = False, ext = False):

    of_all = outputfile.replace("?", "F")

    version = versionBID(feature, core, ext)

    t = template(of_all).replace("%f", version).replace("%a", api)
    of = outputfile.replace("?", "")
    od = outputdir.replace("?", version)

    status(od + of)

    pureCommands = [ c for c in commands if
        (not ext and c.supported(feature, core)) or (ext and not c.supported(feature, False)) ]

    if not os.path.exists(od):
        os.makedirs(od)

    with open(od + of, 'w') as file:
        if not feature:
            file.write(t % ("\n".join(
                [ functionForward(api, c, feature, version) for c in pureCommands ])))
        else:
            file.write(t % ("\n".join(
                [ functionForward(api, c, feature, version) for c in pureCommands ])))


def genFeatureFunctionImplementations(api, commands, feature, outputdir, outputfile, core = False, ext = False):

    of_all = outputfile.replace("?", "F")

    version = versionBID(feature, core, ext)

    t = template(of_all).replace("%f", version).replace("%a", api)
    of = outputfile.replace("?", "")
    od = outputdir.replace("?", version)

    status(od + of)

    pureCommands = [ c for c in commands if
        (not ext and c.supported(feature, core)) or (ext and not c.supported(feature, False)) ]

    if not os.path.exists(od):
        os.makedirs(od)

    with open(od + of, 'w') as file:
        if not feature:
            file.write(t % ("\n".join(
                [ functionImplementation(c, feature, version) for c in pureCommands ])))
        else:
            file.write(t % ("\n".join(
                [ functionForwardImplementation(c, feature, version) for c in pureCommands ])))