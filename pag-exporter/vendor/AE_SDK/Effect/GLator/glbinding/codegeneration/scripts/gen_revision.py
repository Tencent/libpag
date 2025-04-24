from binding import *


def genRevision(revision, outputdir, outputfile):	
	status(outputdir + outputfile)

	with open(outputdir + outputfile, 'w') as file:		
		file.write(template(outputfile) % (str(revision)))
