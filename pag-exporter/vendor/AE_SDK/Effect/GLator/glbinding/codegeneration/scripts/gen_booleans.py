from binding import *
from classes.Enum import *


def booleanDefinition(enum):

    return "%s = %s" % (enumBID(enum), enum.value)


def booleanImportDefinition(api, enum):

    qualifier = api + "::"
    return "using %s%s;" % (qualifier, enumBID(enum))


def forwardBoolean(enum):

    return "static const GLboolean %s = GLboolean::%s;" % (enumBID(enum), enumBID(enum))


def genBooleans(api, enums, outputdir, outputfile, forward = False):

    of_all = outputfile.replace("?", "F")

    t = template(of_all).replace("%a", api)
    of = outputfile.replace("?", "")
    od = outputdir.replace("?", "")

    status(od + of)

    tgrouped = groupEnumsByType(enums)
    pureBooleans = tgrouped["GLboolean"]
    
    if not os.path.exists(od):
        os.makedirs(od)

    with open(od + of, 'w') as file:

        if forward:

            file.write(t % (("\n").join([ forwardBoolean(e) for e in pureBooleans ])))

        else:

            file.write(t % (
                (",\n" + tab).join([ booleanDefinition(e) for e in pureBooleans ]),
                ("\n") .join([ forwardBoolean(e) for e in pureBooleans ])))

def genFeatureBooleans(api, enums, feature, outputdir, outputfile, core = False, ext = False):

    of_all = outputfile.replace("?", "F")
    
    version = versionBID(feature, core, ext)

    t = template(of_all).replace("%f", version).replace("%a", api)
    of = outputfile.replace("?", "")
    od = outputdir.replace("?", version)

    status(od + of)

    tgrouped = groupEnumsByType(enums)
    pureBooleans = tgrouped["GLboolean"]
    
    if not os.path.exists(od):
        os.makedirs(od)

    with open(od + of, 'w') as file:

        if not feature:

            file.write(t % (
                (",\n" + tab).join([ booleanDefinition(e) for e in pureBooleans ]),
                ("\n") .join([ forwardBoolean(e) for e in pureBooleans ])))

        else:

            file.write(t % (("\n").join([ booleanImportDefinition(api, e) for e in pureBooleans ])))

def genBooleansFeatureGrouped(api, enums, features, outputdir, outputfile):

    # gen functions feature grouped
    for f in features:
        if f.api == "gl": # ToDo: probably seperate for all apis
            genFeatureBooleans(api, enums, f, outputdir, outputfile)
            if f.major > 3 or (f.major == 3 and f.minor >= 2):
                genFeatureBooleans(api, enums, f, outputdir, outputfile, True)
            genFeatureBooleans(api, enums, f, outputdir, outputfile, False, True)
