from binding import *
from classes.Extension import *


def genExtensions(api, extensions, outputdir, outputfile):

    of = outputfile.replace("?", "")
    od = outputdir.replace("?", "")
    t = template(of).replace("%a", api)

    status(od + of)
    
    if not os.path.exists(od):
        os.makedirs(od)

    with open(od + of, 'w') as file:
        file.write(t % (",\n" + tab).join(
            [ extensionBID(e) for e in extensions ]))
