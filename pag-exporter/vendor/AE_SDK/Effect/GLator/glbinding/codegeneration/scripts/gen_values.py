from binding import *
from classes.Enum import *


def valueDefinition(enum):

    return "static const %s %s = %s;" % (enum.type, enumBID(enum), enum.value)


def genValuesForwards(api, enums, outputdir, outputfile):

    genValues(api, enums, outputdir, outputfile, True)


def genValuesFeatureGrouped(api, values, features, outputdir, outputfile):

    # gen values feature grouped
    for f in features:
        if f.api == "gl": # ToDo: probably seperate for all apis
            genFeatureValues(api, values, f, outputdir, outputfile)
            if f.major > 3 or (f.major == 3 and f.minor >= 2):
                genFeatureValues(api, values, f, outputdir, outputfile, True)
            genFeatureValues(api, values, f, outputdir, outputfile, False, True)

def genFeatureValues(api, values, feature, outputdir, outputfile, core = False, ext = False):

    of_all = outputfile.replace("?", "F")

    version = versionBID(feature, core, ext)

    t = template(of_all).replace("%f", version).replace("%a", api)
    of = outputfile.replace("?", "")
    od = outputdir.replace("?", version)

    status(od + of)

    qualifier = api + "::"
    
    tgrouped = groupEnumsByType(values)
    del tgrouped["GLboolean"]
    del tgrouped["GLenum"]
    del tgrouped["GLbitfield"]
    
    groups = []
    for type in sorted(tgrouped.keys()):
        groups.append("\n".join([ ("using %s%s;" % (qualifier, (enumBID(c)))) for c in tgrouped[type] if (not ext and c.supported(feature, core)) or (ext and not c.supported(feature, False)) ]))

    if not os.path.exists(od):
        os.makedirs(od)
    
    with open(od + of, 'w') as file:

        file.write(t % ("\n".join(groups)))

def genValues(api, enums, outputdir, outputfile, forward = False):

    of_all = outputfile.replace("?", "F")

    t = template(of_all).replace("%a", api)
    of = outputfile.replace("?", "")
    od = outputdir.replace("?", "")

    status(od + of)


    tgrouped = groupEnumsByType(enums)
    del tgrouped["GLboolean"]
    del tgrouped["GLenum"]
    del tgrouped["GLbitfield"]

    groups = []    
    for type in sorted(tgrouped.keys()):
        groups.append("\n".join([ valueDefinition(c) for c in tgrouped[type] ]))
    
    if not os.path.exists(od):
        os.makedirs(od)

    with open(od + of, 'w') as file:
        file.write(t % ("\n\n".join(groups)))
