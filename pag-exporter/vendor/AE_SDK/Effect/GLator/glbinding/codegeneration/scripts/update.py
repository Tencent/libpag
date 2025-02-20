#!/usr/bin/python

import os, sys, getopt, io, subprocess

def update(outputfile, revisionfile):
    url = "https://cvs.khronos.org/svn/repos/ogl/trunk/doc/registry/public/api/gl.xml"
    
    revision = 0

    if os.path.isfile(revisionfile):

        revisionfilehandle = open(revisionfile, "rt")
    
        if not revisionfilehandle.closed:
            revision = int(revisionfilehandle.readline())
    
        revisionfilehandle.close()

        print("Local revision of " + outputfile + " is " + str(revision))
    
    else:
        print("Local revision file not found")

        

    print("Retrieving latest revision of gl.xml")

    svninfo = subprocess.Popen(["svn", "info", url], stdout=subprocess.PIPE)
    newrevisioninfo = svninfo.communicate()[0]
    
    if sys.version_info < (3,):
        stream = io.StringIO(unicode(newrevisioninfo))
    else:
        stream = io.StringIO(str(newrevisioninfo, 'utf-8'))  

    newrevision = 0
    
    line = str(stream.readline())
    while line:
        if line.startswith("Last Changed Rev:"):
            newrevision = int(''.join(ch for ch in line if ch.isdigit()))
            break
        line = str(stream.readline())

    if newrevision <= revision:
        print (outputfile + " is up to date, skipping update")
        sys.exit(0)
    
    print ("Updating " + outputfile + " to " + str(newrevision))
    
    revisionfilehandle = open(revisionfile, "wt")
    revisionfilehandle.write(str(newrevision))
    
    print ("Local revision of " + outputfile + " is now " + str(newrevision))
    
    os.system("svn export --force --quiet " + url + " " + outputfile)
    os.system("git diff " + outputfile)


def main(argv):
    try:
        opts, args = getopt.getopt(argv[1:], "f:r:", ["file=", "revision="])
    except getopt.GetoptError:
        print("usage: %s -f <GL XML> -r <revision file>" % argv[0])
        sys.exit(1)
    
    outputfile = None
    revisionfile = None
    
    for opt, arg in opts:
        if opt in ("-f", "--file"):
            outputfile = arg

        if opt in ("-r", "--revision"):
            revisionfile = arg

    if outputfile == None:
        print("no GL XML file given")
        sys.exit(1)

    if revisionfile == None:
        print("no revision file given")
        sys.exit(1)

    update(outputfile, revisionfile)


if __name__ == "__main__":
    main(sys.argv)
