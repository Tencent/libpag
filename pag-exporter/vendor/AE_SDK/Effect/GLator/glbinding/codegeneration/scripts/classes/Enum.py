from classes.Feature import *
from classes.Extension import *


def translateType(t, name):

    if name in [ "GL_TRUE", "GL_FALSE" ]:
        return "GLboolean"

    return { "u" : "GLuint", "ull" : "GLuint64"    }.get(t, "GLenum")


class Enum:

    def __init__(self, xml, features, extensions, groupString, groupType, api):

        self.api   = api

        self.name  = xml.attrib["name"]
        self.value = xml.attrib["value"]
        self.type  = "GLenum"

        self.aliasString = ""
        self.alias = None

        # an enum group is, if defined, defined specifically for an enum
        # but the enum itself might be reused by other groups as well.
        self.groups = set()
        self.groupString = None # ToDo: only supported for GLbitfield for now

        self.aliasString = xml.attrib.get("alias", None)

        if groupString == "SpecialNumbers":
            self.type = translateType(xml.attrib.get("type", ""), self.name)
        elif groupType == "bitmask":
            self.type = "GLbitfield"
            self.groupString = groupString

        self.reqFeatures   = []
        self.remFeatures   = [] # len(remF) should always be < 2
        self.reqExtensions = []

        for feature in features:
             if feature.api == api and self.name in feature.reqEnumStrings:
                 self.reqFeatures.append(feature)

        for feature in features:
            if feature.api == api and self.name in feature.remEnumStrings:
                self.remFeatures.append(feature)
            
        for extension in extensions:
            if extension.api == api and self.name in extension.reqEnumStrings:
                self.reqExtensions.append(extensions)


    def __str__(self):
        
        return "Enum(%s, %s)" % (self.name, self.value)
        

    def __lt__(self, other):

        return self.value < other.value or (self.value == other.value and self.name < other.name)


    # this compares the given feature with the lowest requiring feature
    def supported(self, feature, core):

        if feature is None:
            return True

        # ToDo: this might create a cyclic recursion if glm is errorneuos
        aliasSupported = self.alias.supported(feature, core) if self.alias else False

        # Note: design decission:
        # every featured functions include should not contain enums from extensions.

        #if len(self.reqFeatures) == 0 and len(self.reqExtensions) > 0:
        #    return True
        
        if len(self.reqFeatures) == 0:
            return aliasSupported

        if core:
            sSelf = min(self.reqFeatures) <= feature and (not self.remFeatures or min(self.remFeatures) > feature) 
            return sSelf or aliasSupported
        else:
            sSelf = min(self.reqFeatures) <= feature
            return sSelf or aliasSupported

class Group:    

    def __init__(self, xml):
    
        self.enums       = set()
        self.enumStrings = []

        if isinstance(xml, str):
            self.name = xml
            return

        self.name = xml.attrib["name"]
        for enum in xml.iter("enum"):
            self.enumStrings.append(enum.attrib["name"])

    
    def __str__(self):
    
        return "Group(%s, %s)" % (self.name, str(len(self.enumStrings)))

    
    def __lt__(self, other):
    
        return self.name < other.name
    

def parseGroups(xml, enums):
    
    groups = []
    groupsByName = dict()
    
    for G in xml.iter("groups"):
        for g in G.iter("group"):
            group = Group(g)
            groups.append(group)
            groupsByName[group.name] = group
            for e in g.iter("enum"):
                group.enumStrings.append(e.attrib["name"])

    # if groups are not listed in groups section 
    # they can be implicitly specified by enums

    for enum in enums:
        createGroup_ifImplicit(groups, groupsByName, enum)

    return sorted(groups)


def createGroup_ifImplicit(groups, groupsByName, enum):

    name = enum.groupString

    if name is None:
        return

    if name not in groupsByName:
        group = Group(name)
        groups.append(group)

        groupsByName[name] = group

    groupsByName[name].enumStrings.append(enum.name)


def resolveGroups(groups, enumsByName):

    for group in groups:

        group.enums = set([ enumsByName[e] for e 
            in group.enumStrings if e in enumsByName ])

        for enum in group.enums:
            enum.groups.add(group)


def verifyGroups(groups, enums):

    # all non verified enums/grups should be patched

    unreferenced = set()

    # (1) check that every referenced group exists (resolveEnums)

    groupsByName = dict([(group.name, group) for group in groups])
    
    for enum in enums:
        if enum.groupString is not None and enum.groupString not in groupsByName:
            unreferenced.add(enum)

    if len(unreferenced) > 0:
        print(" WARNING: " + str(len(unreferenced)) + " unreferenced groups:")
        for enum in unreferenced:
            print("  %s (in %s)" % (enum.groupString, enum.name))

    # (2) check that every enum referencing a group, 
    # is actually referenced in that group

    # ToDo

    # (3) check that every enum of type GLbitfield 
    # has only one group (important for namespace import) 

    # Note: (3) is deprecated since glbinding supports groups 

    #overflows = set()
    
    #for enum in enums:
    #   if enum.type == "GLbitfield" and len(enum.groups) > 1:
    #       overflows.add(enum)

    #if len(overflows) > 0:
    #    print " WARNING: " + str(len(overflows)) + " enums are in multiple groups:"
    #    for enum in overflows:
    #        print ("  %s groups for %s (%s)" % (str(len(enum.groups)), enum.name, ", ".join([g.name for g in enum.groups])))


def parseEnums(xml, features, extensions, commands, api):

    # create utility string sets to simplify application of constraints

    groupsUsed = set()
    for command in (command for command in commands if len(command.params) > 0):
        for param in (param for param in command.params if param.groupString is not None):
            groupsUsed.add(param.groupString)

    enumsRequired = set()
    for feature in features:
        if len(feature.reqEnumStrings) > 0:
            enumsRequired |= set(feature.reqEnumStrings)
    for extension in extensions:
        if len(extension.reqEnumStrings) > 0:
            enumsRequired |= set(extension.reqEnumStrings)


    enums = set()

    for E in xml.iter("enums"):

        groupString = E.attrib.get("group", None)
        groupType   = E.attrib.get("type", None)

        # only parse enum if 
        # (1) no comment attribute exists for <enum> starting with "Not an API enum. ..."
        # (2) at least one feature or extension of the requested api requires the enum of requested api
        # (3) if the enum has a group and at least one command has a parameter of that group

        for enum in E.findall("enum"):

            # enorce constraint (1)
            if "comment" in enum.attrib and enum.attrib["comment"].startswith("Not an API enum."):
                continue

            name = enum.attrib["name"]

            # enorce constraint (2) and (3)
            if name not in enumsRequired and groupString not in groupsUsed:
                continue

            if "api" in enum.attrib and enum.attrib["api"] != api:
                continue

            enums.add(Enum(enum, features, extensions, groupString, groupType, api))

    return sorted(enums)


def resolveEnums(enums, enumsByName, groupsByName):

    aliases = dict()
    groups = dict()

    for enum in enums:
        # aliases might be from other api, but are not added 
        # since enums by name only includes api enums
        if enum.aliasString is not None:
            if enum.aliasString in enumsByName:
                enum.alias = enumsByName[enum.aliasString]
            else:
                aliases[enum.aliasString] = enum

        if enum.groupString is not None:

            if enum.groupString in groupsByName:

                group = groupsByName[enum.groupString]
                enum.groups.add(group)
                group.enums.add(enum)
            else:
                groups[enum.groupString] = enum

    if len(aliases) > 0:
        print(" WARNING: " + str(len(aliases)) + " unresolved aliases:")
        for alias, enum in aliases.items():
            print("  %s (of %s)" % (alias, enum.name))

    if len(groups) > 0:
        print(" WARNING: " + str(len(groups)) + " unresolved groups:")
        for group, enum in groups.items():
            print("  %s (in %s)" % (group, enum.name))


def patchEnums(enums, patches, groups):

    enumsByName  = dict([(enum.name, enum) for enum in enums])
    groupsByName = dict([(group.name, group) for group in groups])

    for patch in patches:
        if patch.name not in enumsByName:
            createGroup_ifImplicit(groups, groupsByName, patch)
            enums.append(patch)
        elif len(patch.aliasString) > 0:
            enumsByName[patch.name].aliasString = patch.aliasString
            enumsByName[patch.name].alias = enumsByName[patch.aliasString]

        # ToDo: probably more fixes might be appropriate


def patchGroups(groups, patches):

    groupsByName = dict([(group.name, group) for group in groups])

    for patch in patches:
        if patch.name not in groupsByName:
            groups.append(patch)
        else:
            g = groupsByName[patch.name]
            for e in patch.enumStrings:
                g.enumStrings.append(e)

def groupEnumsByType(enums):

    d = dict()
    
    for e in enums:
        if not e.type in d:
            d[e.type] = []
        d[e.type].append(e)
        
    return d


def groupEnumsByGroup(enums):

    d = dict()
    
    ungroupedName = "__UNGROUPED__"
    
    for e in enums:
        if len(e.groups)==0:
            if not ungroupedName in d:
                d[ungroupedName] = []
            d[ungroupedName].append(e)
            continue
        for g in e.groups:
            if not g.name in d:
                d[g.name] = []
            d[g.name].append(e)

    for key in d.keys():
        d[key] = sorted(d[key], key = lambda e: e.value)

    return d    
