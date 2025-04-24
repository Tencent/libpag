from binding import *
from classes.Feature import *


def genFeatures(api, features, outputdir, outputfile):

    genFeature(api, None, outputdir, outputfile)
    
    # gen bitfields feature grouped
    for f in features:
        if f.api == "gl": # ToDo: probably seperate for all apis
            genFeature(api, f, outputdir, outputfile)
            if f.major > 3 or (f.major == 3 and f.minor >= 2):
                genFeature(api, f, outputdir, outputfile, True)

            # non core gl includes ext to support forward
            genFeature(api, f, outputdir, outputfile, False, True)


def genFeature(api, feature, outputdir, outputfile, core = False, ext = False):

    of_all = outputfile.replace("?", "F")

    version = versionBID(feature, core, ext)
    
    t = template(of_all).replace("%d", version).replace("%f", "").replace("%a", api)
    of = outputfile.replace("?", "")
    od = outputdir.replace("?", version)
    versionExtFile = ""
    versionExtDir = versionBID(feature, core, True)

    status(od + of)
    
    if not os.path.exists(od):
        os.makedirs(od)

    with open(od + of, 'w') as file:
        if core or ext or (not feature):
            file.write(t.replace("%f", version) % ("", "", "", "", "", ""))
        else:
            file.write(t.replace("%f", version) % (
                "\n#include <glbinding/" + api + versionExtDir + "/types"     + versionExtFile + ".h>",
                "\n#include <glbinding/" + api + versionExtDir + "/boolean"   + versionExtFile + ".h>",
                "\n#include <glbinding/" + api + versionExtDir + "/values"    + versionExtFile + ".h>",
                "\n#include <glbinding/" + api + versionExtDir + "/bitfield"  + versionExtFile + ".h>",
                "\n#include <glbinding/" + api + versionExtDir + "/enum"      + versionExtFile + ".h>",
                "\n#include <glbinding/" + api + versionExtDir + "/functions" + versionExtFile + ".h>"))
