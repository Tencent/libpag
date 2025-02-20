from binding import *
from classes.Enum import *
from classes.Feature import *

enumGroupTemplate = """    // %s

%s
"""


def castEnumValue(value):

    if not value.startswith("-"):
        return value
    else:
        return "static_cast<unsigned int>(%s)" % value    


def enumDefinition(group, enum, maxlen, usedEnumsByName):

    spaces = " " * (maxlen - len(enumBID(enum)))
    if enum.name not in usedEnumsByName:
        usedEnumsByName[enum.name] = group
        return "    %s %s= %s," % (enumBID(enum), spaces, castEnumValue(enum.value))
    else:
        reuse = usedEnumsByName[enum.name]
        return "//  %s %s= %s, // reuse %s" % (enumBID(enum), spaces, castEnumValue(enum.value), reuse)

def enumImportDefinition(api, enum, group, usedEnumsByName):

    qualifier = api + "::"

    if enum.name not in usedEnumsByName:
        usedEnumsByName[enum.name] = group
        return "using %s%s;" % (qualifier, enumBID(enum))
    else:
        reuse = usedEnumsByName[enum.name]
        return "// using %s%s; // reuse %s" % (qualifier, enumBID(enum), reuse)


def forwardEnum(api, enum, group, usedEnumsByName):

    qualifier = "GLenum"

    if enum.name not in usedEnumsByName:
        usedEnumsByName[enum.name] = group
        return "static const %s %s = %s::%s;" % (
            qualifier, enumBID(enum), qualifier, enumBID(enum))
    else:
        reuse = usedEnumsByName[enum.name]
        return "// static const %s %s = %s::%s; // reuse %s" % (
            qualifier, enumBID(enum), qualifier, enumBID(enum), reuse)


def enumGroup(group, enums, usedEnumsByName):

    if len(enums) == 0:
        return

    maxlen = max([len(enum.name) for enum in enums]) if len(enums) > 0 else 0

    return enumGroupTemplate % (group, "\n".join(
        [ enumDefinition(group, e, maxlen, usedEnumsByName) for e in sorted(enums, key = lambda e: e.value) ]))


def genEnumsAll(api, enums, outputdir, outputfile):

    genFeatureEnums(api, enums, None, outputdir, outputfile, None)


def genEnumsFeatureGrouped(api, enums, features, outputdir, outputfile):

    # gen enums feature grouped
    for f in features:
        if f.api == "gl": # ToDo: probably seperate for all apis
            genFeatureEnums(api, enums, f, outputdir, outputfile)
            if f.major > 3 or (f.major == 3 and f.minor >= 2):
                genFeatureEnums(api, enums, f, outputdir, outputfile, True)
            genFeatureEnums(api, enums, f, outputdir, outputfile, False, True)


def genFeatureEnums(api, enums, feature, outputdir, outputfile, core = False, ext = False):

    of_all = outputfile.replace("?", "F")

    version = versionBID(feature, core, ext)

    t = template(of_all).replace("%f", version).replace("%a", api)
    of = outputfile.replace("?", "")
    od = outputdir.replace("?", version)

    status(od + of)

    tgrouped     = groupEnumsByType(enums)

    pureEnums    = [ e for e in tgrouped["GLenum"] if 
        (not ext and e.supported(feature, core)) or (ext and not e.supported(feature, False)) ]
    groupedEnums = groupEnumsByGroup(pureEnums)

    usedEnumsByName = dict()
    
    if feature:
        importToNamespace = [ ("\n// %s\n\n" + "%s") % (group, "\n".join(
        [ enumImportDefinition(api, e, group, usedEnumsByName) for e in enums ]))  
            for group, enums in sorted(groupedEnums.items()) ]
    else:        
        importToNamespace = [ ("\n// %s\n\n" + "%s") % (group, "\n".join(
        [ forwardEnum(api, e, group, usedEnumsByName) for e in enums ]))  
            for group, enums in sorted(groupedEnums.items()) ]

    usedEnumsByName.clear()
    
    if not os.path.exists(od):
        os.makedirs(od)

    with open(od + of, 'w') as file:
        if not feature:

            definitions = [ enumGroup(group, enums, usedEnumsByName) 
                for group, enums in sorted(groupedEnums.items(), key = lambda x: x[0]) ]

            file.write(t % ("\n".join(definitions), ("\n") .join(importToNamespace)))

        else:
            # the "original", non-featured enums are imported to the featured namespace
            file.write(t % (("\n") .join(importToNamespace)))
