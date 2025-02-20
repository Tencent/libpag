from binding import *
from classes.Enum import *
from classes.Feature import *

#bitfieldGroupTemplate = """enum class %s : unsigned int
bitfieldGroupTemplate = """enum class %s : unsigned int
{
%s
};

"""


def bitfieldDefinition(enum, group, maxlen, usedBitfsByName):

    spaces = " " * (maxlen - len(enumBID(enum)))
    if enum.name not in usedBitfsByName:
        usedBitfsByName[enum.name] = group
        return "    %s %s= %s," % (enumBID(enum), spaces, enum.value)
    else:
        reuse = usedBitfsByName[enum.name]
        return "    %s %s= %s, // reuse from %s" % (enumBID(enum), spaces, enum.value, reuse)


def bitfieldImportDefinition(api, enum):
    qualifier = api + "::"
    group = sorted(enum.groups)[0].name

    return "using %s%s;" % (qualifier, enumBID(enum))


def forwardBitfield(api, enum):
    qualifier = ""
    group = sorted(enum.groups)[0].name

    if len(enum.groups) == 1:
        return "static const %s %s = %s::%s;" % (qualifier+group, enumBID(enum), qualifier+group, enumBID(enum))
    else:
        groups = ", ".join([ qualifier+g.name for g in sorted(enum.groups) ])
        return "static const glbinding::SharedBitfield<%s> %s = %s::%s;" % (groups, enumBID(enum), qualifier+group, enumBID(enum))



def bitfieldGroup(group, enums, usedBitfsByName):

    if len(enums) == 0:
        return

    maxlen = max([len(enum.name) for enum in enums]) if len(enums) > 0 else 0

    enumGroupString = [ bitfieldDefinition(e, group, maxlen, usedBitfsByName) for e in enums ]
    
    spaces = " " * (maxlen - len("GL_NONE_BIT"))
    enumGroupString.insert(0, "    GL_NONE_BIT " + spaces + "= 0x0, // Generic GL_NONE_BIT")
    
    return bitfieldGroupTemplate % (group, "\n".join(enumGroupString))


def genBitfieldsAll(api, enums, outputdir, outputfile):

    genFeatureBitfields(api, enums, None, outputdir, outputfile, None)


def genBitfieldsFeatureGrouped(api, enums, features, outputdir, outputfile):

    # gen bitfields feature grouped
    for f in features:
        if f.api == "gl": # ToDo: probably seperate for all apis
            genFeatureBitfields(api, enums, f, outputdir, outputfile)
            if f.major > 3 or (f.major == 3 and f.minor >= 2):
                genFeatureBitfields(api, enums, f, outputdir, outputfile, True)
            genFeatureBitfields(api, enums, f, outputdir, outputfile, False, True)


def genFeatureBitfields(api, enums, feature, outputdir, outputfile, core = False, ext = False):

    of_all = outputfile.replace("?", "F")
    
    version = versionBID(feature, core, ext)
    
    t = template(of_all).replace("%f", version).replace("%a", api)
    of = outputfile.replace("?", "")
    od = outputdir.replace("?", version)

    status(od + of)

    tgrouped  = groupEnumsByType(enums)

    pureBitfields    = [ b for b in tgrouped["GLbitfield"] if 
      (not ext and b.supported(feature, core)) or (ext and not b.supported(feature, False)) ]
    groupedBitfields = groupEnumsByGroup(pureBitfields)

    usedBitfsByName = dict()
    
    qualifier = api + "::"
    
    if feature:
        importToNamespace = [ bitfieldImportDefinition(api, e) for e in sorted(pureBitfields) ]
        if len(importToNamespace):
            importToNamespace.insert(0, "using gl::GL_NONE_BIT;")
    else:
        groups = ", ".join([ qualifier+g for g in sorted(groupedBitfields.keys()) ])
        importToNamespace = [ forwardBitfield(api, e) for e in sorted(pureBitfields) ]
        if len(importToNamespace):
            importToNamespace.insert(0, "static const glbinding::SharedBitfield<%s> GL_NONE_BIT = %s::%s::GL_NONE_BIT;" % (groups, api, sorted(groupedBitfields.keys())[0]))

    usedBitfsByName.clear()
    
    if not os.path.exists(od):
        os.makedirs(od)

    with open(od + of, 'w') as file:

        if not feature:

            #definitions = [ bitfieldGroup(group, enums, maxlen) for group, enums in groupedBitfields.items() ]
            definitions = [ bitfieldGroup(group, enums, usedBitfsByName) 
                for group, enums in sorted(groupedBitfields.items()) ]

            file.write(t % (("\n") .join(definitions), ("\n") .join(importToNamespace)))

        else:
            # the "original", non-featured bitfields are imported to the featured namespace
            file.write(t % (("\n") .join(importToNamespace)))
