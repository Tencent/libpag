#!/usr/bin/python

import sys, getopt

import xml.etree.ElementTree as ET

from classes.Feature import *
from classes.Enum import *
from classes.Command import *
from classes.Extension import *
from classes.Type import *

from binding import *

from gen_revision import *

from gen_features import *
from gen_bitfields import *
from gen_booleans import *
from gen_enums import *
from gen_values import *
from gen_types import *

from gen_extensions import *
from gen_functions import *

from gen_versions import *

from gen_meta import *
from gen_test import *

def generate(inputfile, patchfile, targetdir, revisionfile):

    # preparing

    print("")
    print("PREPARING")

    api     = "gl" # ToDo: other apis are untested yet

    print("checking revision")
    file = open(revisionfile, "r")
    revision = int(file.readline())
    file.close()    
    print(" revision is " + str(revision))

    print("loading " + inputfile)
    tree       = ET.parse(inputfile)
    registry   = tree.getroot()

    # parsing

    print("")
    print("PARSING (" + api + " API)")

    print("parsing features")
    features   = parseFeatures(registry, api)
    print(" # " + str(len(features)) + " features parsed")

    print("parsing types")
    types      = parseTypes(registry, api)
    print(" # " + str(len(types)) + " types parsed")

    print("parsing extensions")
    extensions = parseExtensions(registry, features, api)
    print(" # " + str(len(extensions)) + " extensions parsed")

    print("parsing commands")
    commands   = parseCommands(registry, features, extensions, api)
    print(" # " + str(len(commands)) + " commands parsed")
        
    print("parsing enums")
    enums      = parseEnums(registry, features, extensions, commands, api)
    print(" # " + str(len(enums)) + " enums parsed")

    print("parsing enum groups")
    groups     = parseGroups(registry, enums)
    print(" # " + str(len(groups)) + " enum groups parsed")

    # patching

    print("")
    print("PATCHING")

    if patchfile is not None:

        print("parsing " + patchfile)
        patchtree     = ET.parse(patchfile)
        patchregistry = patchtree.getroot()

        print("patching types")
        patch = parseTypes(patchregistry, api)
        patchTypes(types, patch)

        print("patching commands")
        patch = parseCommands(patchregistry, features, extensions, api)
        patchCommands(commands, patch)

        print("patching features")
        print(" WARNING: todo")

        print("patching enums")
        patch = parseEnums(patchregistry, features, extensions, commands, api)
        patchEnums(enums, patch, groups)

        print("patching groups")
        patch = parseGroups(patchregistry, enums)
        patchGroups(groups, patch)

    # resolving references for classes

    enumsByName    = dict([(enum.name, enum) for enum in enums])
    groupsByName   = dict([(group.name, group) for group in groups])
    commandsByName = dict([(command.name, command) for command in commands])

    print("")
    print("RESOLVING")

    print("resolving features")
    resolveFeatures(features, enumsByName, commandsByName)

    print("resolving extensions")
    resolveExtensions(extensions, enumsByName, commandsByName, features, api)

    print("resolving groups")
    resolveGroups(groups, enumsByName)
        
    print("resolving enums")
    resolveEnums(enums, enumsByName, groupsByName)

    # verifying 

    print("")
    print("VERIFYING")

    bitfGroups = [ g for g in groups 
        if len(g.enums) > 0 and any(enum.type == "GLbitfield" for enum in g.enums) ]

    print("verifying groups")
    verifyGroups(groups, enums)

    print("verifying commands")
    verifyCommands(commands, bitfGroups)

    # generating

    print("")
    print("GENERATING")

    includedir = targetdir + "/include/glbinding/"
    sourcedir  = targetdir + "/source/"
    testdir    = targetdir + "/../tests/glbinding-test/"

    includedir_api = includedir + api + "?/"
    sourcedir_api  = sourcedir  + api + "?/"

    # Generate API namespace classes (gl, gles1, gles2, ...) - ToDo: for now only gl

    genRevision                    (     revision,           sourcedir,      "glrevision.h")

    genExtensions                  (api, extensions,         includedir_api, "extension.h")

    genBooleans                    (api, enums,              includedir_api, "boolean.h")
    genBooleansFeatureGrouped      (api, enums, features,    includedir_api, "boolean?.h")

    genValues                      (api, enums,              includedir_api, "values.h")
    genValuesFeatureGrouped        (api, enums, features,    includedir_api, "values?.h")

    genTypes_h                     (api, types, bitfGroups,  includedir_api, "types.h") 
    genTypesFeatureGrouped         (api, types, bitfGroups,  features,  includedir_api, "types?.h")

    genBitfieldsAll                (api, enums,              includedir_api, "bitfield.h")
    genBitfieldsFeatureGrouped     (api, enums, features,    includedir_api, "bitfield?.h")

    genEnumsAll                    (api, enums,              includedir_api, "enum.h")
    genEnumsFeatureGrouped         (api, enums, features,    includedir_api, "enum?.h")

    genFunctionsAll                (api, commands,           includedir_api, "functions.h")
    genFunctionsFeatureGrouped     (api, commands, features, includedir_api, "functions?.h")
    
    genFeatures                    (api, features,           includedir_api, "gl?.h")

    genTypes_cpp                   (api, types, bitfGroups,  sourcedir_api,  "types.cpp")
    genFunctionImplementationsAll  (api, commands,           sourcedir_api,  "functions.cpp")
    
    genTest                        (api, features,           testdir,  "AllVersions_test.cpp")

    # Generate GLBINDING namespace classes

    genFunctionObjects_h           (commands,           includedir, "Binding.h")
    genFunctionObjects_cpp         (commands,           sourcedir,  "Binding_objects.cpp")

    genVersions                    (features,           sourcedir,  "Version_ValidVersions.cpp")

    # ToDo: the generation of enum to/from string will probably be unified...
    genMetaMaps		           (enums,              sourcedir,  "Meta_Maps.h",                bitfGroups)
    genMetaStringsByBitfield       (bitfGroups,         sourcedir,  "Meta_StringsByBitfield.cpp")
    genMetaBitfieldByString        (bitfGroups,         sourcedir,  "Meta_BitfieldsByString.cpp")
    genMetaStringsByEnum           (enums,              sourcedir,  "Meta_StringsByBoolean.cpp",  "GLboolean")
    genMetaEnumsByString           (enums,              sourcedir,  "Meta_BooleansByString.cpp",  "GLboolean")
    genMetaStringsByEnum           (enums,              sourcedir,  "Meta_StringsByEnum.cpp",     "GLenum")
    genMetaEnumsByString           (enums,              sourcedir,  "Meta_EnumsByString.cpp",     "GLenum")

    genMetaStringsByExtension      (extensions,         sourcedir,  "Meta_StringsByExtension.cpp")
    genMetaExtensionsByString      (extensions,         sourcedir,  "Meta_ExtensionsByString.cpp")

    genReqVersionsByExtension      (extensions,         sourcedir,  "Meta_ReqVersionsByExtension.cpp")

    genFunctionStringsByExtension  (extensions,         sourcedir,  "Meta_FunctionStringsByExtension.cpp")
    genExtensionsByFunctionString  (extensions,         sourcedir,  "Meta_ExtensionsByFunctionString.cpp")


    print("")


def main(argv):
    try:
        opts, args = getopt.getopt(argv[1:], "s:p:d:r:", ["spec=", "patch=", "directory=" , "revision="])
    except getopt.GetoptError:
        print("usage: %s -s <GL spec> [-p <patch spec file>] [-d <output directory>] [-r <revision file>]" % argv[0])
        sys.exit(1)
        
    targetdir = "."
    inputfile = None
    patchfile = None
    
    for opt, arg in opts:
        if opt in ("-s", "--spec"):
            inputfile = arg

        if opt in ("-p", "--patch"):
            patchfile = arg

        if opt in ("-d", "--directory"):
            targetdir = arg

        if opt in ("-r", "--revision"):
            revision  = arg
            
    if inputfile == None:
        print("no GL spec file given")
        sys.exit(1)

    Status.targetdir = targetdir

    generate(inputfile, patchfile, targetdir, revision)

if __name__ == "__main__":
    main(sys.argv)
