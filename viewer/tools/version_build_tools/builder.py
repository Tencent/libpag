import os
import configparser


class Builder:
    output = ""

    def __init__(self, output_file):
        self.parser = Parser()
        self.output_file = output_file

    def build(self):
        with open(self.output_file, 'w', encoding='utf8') as fw:
            fw.write(self.output)

    def write(self, content):
        self.output += content

    def writeLine(self, content=""):
        self.output += content + "\n"


class Parser:
    def __init__(self):
        self.version_major = os.getenv('MajorVersion', '2')
        self.version_minor = os.getenv('MinorVersion', '1')
        self.version_fix = os.getenv('FixVersion', '93')
        self.build_number = os.getenv('BK_CI_BUILD_NO', '0')
        self.is_beta_version = os.getenv('isBetaVersion', True)
