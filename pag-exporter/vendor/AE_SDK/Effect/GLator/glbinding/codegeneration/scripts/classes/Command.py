import sys

import xml.etree.ElementTree as ET

from classes.Feature import *
from classes.Extension import *

# near and far are defined by windows.h ... :( 
exceptions = ["GetProcAddress", "near", "far"]

class Parameter:

    def __init__(self, xml):

        self.name = xml.find("name").text

        # check for additional params
        if list(xml.itertext())[-1] != self.name:
            print(" WARNING: unexpected parameter format for " + self.name)

        self.type = " ".join([t.strip() for t in xml.itertext()][:-1]).strip()

        if self.name in exceptions:
            self.name += "_"

        if self.type.startswith("struct "):
            self.type = self.type[7:]

        self.groupString = xml.attrib.get("group", None)


    def __str__(self):

        return "%s %s" % (self.type, self.name)


class Command:

    def __init__(self, xml, features, extensions, api):

        self.api = api 

        proto = xml.find("proto")

        self.name       = proto.find("name").text
        self.returntype = " ".join([t.strip() for t in proto.itertext()][:-1]).strip()

        self.params = []

        for param in xml.iter("param"):
            self.params.append(Parameter(param))

        self.reqFeatures   = []
        self.remFeatures   = [] # len(remF) should always be < 2
        self.reqExtensions = []
        
        for feature in features:
            if feature.api == api and self.name in feature.reqCommandStrings:
                self.reqFeatures.append(feature)

        for feature in features:
            if feature.api == api and self.name in feature.remCommandStrings:
                self.remFeatures.append(feature)

        for extension in extensions:
            if extension.api == api and self.name in extension.reqCommandStrings:
                self.reqExtensions.append(extensions)
        
    def __str__(self):

        return "%s %s ( %s )" % (self.returntype, self.name, ", ".join([str(p) for p in self.params]))


    def __lt__(self, other):

        return self.name < other.name

    # this compares the given feature with the lowest requiring feature
    def supported(self, feature, core):

        if feature is None:
            return True

        # Note: design decission:
        # every featured functions include should not contain commands from extensions.

        #if len(self.reqFeatures) == 0 and len(self.reqExtensions) > 0:
        #    return True
        
        if len(self.reqFeatures) == 0:
            return False

        if core:
            return min(self.reqFeatures) <= feature and (not self.remFeatures or min(self.remFeatures) > feature) 
        else:
            return min(self.reqFeatures) <= feature

        
def parseCommands(xml, features, extensions, api):

    commands = []

    for C in xml.iter("commands"):

        # only parse command if 
        # (1) at least one feature or extension requires this command of requested api

        for command in C.iter("command"):

            proto = command.find("proto")
            name = proto.find("name").text

            # enforce constraint (1)
            if not any(name in feature.reqCommandStrings \
                for feature in features if len(feature.reqCommandStrings) > 0) \
                and \
                not any(name in extension.reqCommandStrings \
                for extension in extensions if len(extension.reqCommandStrings) > 0):
                    continue

            if "api" in command.attrib and command.attrib["api"] != api:
                continue

            commands.append(Command(command, features, extensions, api))

    return sorted(commands)
    

def patchCommands(commands, patches):

    commandsByName = dict([(command.name, command) for command in commands])

    for patch in patches:

        if patch.name not in commandsByName:
            # ToDo: could/should extend the list of commands here
            continue

        command = commandsByName[patch.name]

        for param in command.params:

            patchedParam = next((p for p in patch.params if p.name == param.name), None)

            if patchedParam is not None:
                param.groupString = patchedParam.groupString
                param.type = patchedParam.type


def verifyCommands(commands, bitfGroups):

    bitfGroupsByName = dict([(group.name, group) for group in bitfGroups])

    # all non verified commands should be patched

    missing    = dict()
    unresolved = dict()

    for command in commands:
        for param in (param for param in command.params):

            # check for bitfield groups (other enum groups not yet used in gl.xml)
            if param.type != "GLbitfield":
                continue

            if   param.groupString is None:
                missing[param]    = command
            elif param.groupString not in bitfGroupsByName:
                unresolved[param] = command

    if len(missing) > 0:
        print(" WARNING: " + str(len(missing)) + " missing group specification (defaulting to GLbitfield):")
        for param, command in missing.items():
            print("  %s (in %s)" % (param.name, command.name))

    if len(unresolved) > 0:
        print(" WARNING: " + str(len(unresolved)) + " unresolved groups:")
        for param, command in unresolved.items():
            print("  %s (in %s)" % (param.groupString, command.name))

