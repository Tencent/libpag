from binding import *
from classes.Feature import *


def genTest(api, features, outputdir, outputfile):

    of = outputfile.replace("?", "")
    od = outputdir.replace("?", "")
    t = template(of)

    status(od + of)
    
    dirWildcard = "?"
    fileWildcard = ""

    featured_include = "#include <glbinding/" + api + dirWildcard +"/" + api + fileWildcard + ".h>"
    featured_includes = list()

    featured_includes.append(featured_include.replace("?", versionBID(None)))
    for f in features:
        if f.api == "gl": # ToDo: probably separate for all apis
            featured_includes.append(featured_include.replace("?", versionBID(f)))
            if f.major > 3 or (f.major == 3 and f.minor >= 2):
                featured_includes.append(featured_include.replace("?", versionBID(f, True)))
            featured_includes.append(featured_include.replace("?", versionBID(f, False, True)))
    
    if not os.path.exists(od):
        os.makedirs(od)

    with open(od + of, 'w') as file:
        file.write(t % ("\n").join(featured_includes))
