#!/usr/bin/python
#concatenate OpenCL strings into blob. Use char literals, not string literal, due to string literal length limits

#
# ADOBE CONFIDENTIAL
#
# Copyright 2017 Adobe
# All Rights Reserved.
#
# NOTICE: Adobe permits you to use, modify, and distribute this file in
# accordance with the terms of the Adobe license agreement accompanying
# it. If you have received this file from a source other than Adobe,
# then your use, modification, or distribution of it requires the prior
# written permission of Adobe.
#

import sys, getopt

def parse(argv):
	inputfile = ''
	outputfile = ''
	name = 'kKernelString'
	try:
		opts, args = getopt.getopt(argv[1:],"hi:o:n:",["ifile=","ofile=","name="])
	except getopt.GetoptError:
		printhelp(argv[0])
	for opt, arg in opts:
		if opt == '-h':
			printhelp()
		elif opt in ("-i", "--ifile"):
			inputfile = arg
		elif opt in ("-o", "--ofile"):
			outputfile = arg
		elif opt in ("-n", "--name"):
			name = arg
	return inputfile, outputfile, name

def printhelp(command):
	print(command + '-i <inputFile> -o <outputFile> -n <nameOfString>')
	sys.exit(2)
	
def escapeNL(inChar):
    return {'\n':"\\n", '\\':'\\\\', "'":"\\'"}.get(inChar, inChar)

def enquote(inChars):
    return "'" + "','".join(escapeNL(c) for c in inChars) + "',"

inputfile, outputfile, name = parse(sys.argv)

with open(outputfile, 'w') if len(outputfile) > 0 else sys.stdout as output:
	output.write("namespace {{ char const {}[] = {{\n".format(name))
	with open(inputfile, 'r') if len(inputfile) > 0 else sys.stdin as f:
		for line in f:
			lineLenMax = 100        #wrap long lines to avoid overflow of compiler input line
			for group in range(0, len(line), lineLenMax):
				output.write(enquote(line[group:group+lineLenMax])+'\n')
	output.write("0};}\n")
