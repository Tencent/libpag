
class Feature:

	def __init__(self, xml, api):

		self.api    = api if xml.attrib["api"] == api else None

		self.name   = xml.attrib["name"]   # e.g., GL_VERSION_1_1
		self.number = xml.attrib["number"] # e.g., 1.1

		self.major  = int(self.number[-3:-2])
		self.minor  = int(self.number[-1:])

		self.requireComments   = []

		self.reqEnums          = []
		self.reqCommands       = []

		self.remEnums          = []
		self.remCommands       = []

		self.reqEnumStrings    = []
		self.reqCommandStrings = []

		self.remEnumStrings    = []
		self.remCommandStrings = []

		for require in xml.findall("require"):

			if "api" in require.attrib and require.attrib["api"] != api:
				continue

			if "comment" in require.attrib:
				self.requireComments.append(require.attrib["comment"])

			for child in require:
				if   child.tag == "enum":
					self.reqEnumStrings.append(child.attrib["name"])
				elif child.tag == "command":
					self.reqCommandStrings.append(child.attrib["name"])

		for remove in xml.findall("remove"):

			if "api" in require.attrib and require.attrib["api"] != api:
				continue

			for child in remove:
				if   child.tag == "enum":
					self.remEnumStrings.append(child.attrib["name"])
				elif child.tag == "command":
					self.remCommandStrings.append(child.attrib["name"])


	def __str__(self):

		return "Feature (%s:%s.%s)" % (self.api, self.major, self.minor)


	def __lt__(self, other):

		if not other:
			return False
		else:
			return self.major < other.major or (self.major == other.major and self.minor < other.minor)

	def __ge__(self, other):

		if not other:
			return False
		else:
			return self.major > other.major or (self.major == other.major and self.minor >= other.minor)


def parseFeatures(xml, api):

	features = []
	for feature in xml.iter("feature"):

		if "api" in feature.attrib and feature.attrib["api"] != api:
			continue

		features.append(Feature(feature, api))

		# ToDo: there might be none requiring/removing features

	return sorted(features)


def resolveFeatures(features, enumsByName, commandsByName):

	for f in features:

		f.reqEnums = [enumsByName[e] for e in f.reqEnumStrings]
		f.remEnums = [enumsByName[e] for e in f.remEnumStrings]

		f.reqCommands = [commandsByName[c] for c in f.reqCommandStrings]
		f.remCommands = [commandsByName[c] for c in f.remCommandStrings]
