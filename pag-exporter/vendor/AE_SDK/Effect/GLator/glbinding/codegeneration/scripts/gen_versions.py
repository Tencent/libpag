from binding import *
from classes.Feature import *


def version(feature):
	return '{ %s, %s }' % (feature.major, feature.minor)


def genVersions(features, outputdir, outputfile):
	status(outputdir + outputfile)

	with open(outputdir + outputfile, 'w') as file:
		file.write(template(outputfile) % ((",\n" + tab).join(
			[ version(f) for f in features if f.api == "gl"]) ,
			  version([ f for f in features if f.api == "gl"][-1])))
