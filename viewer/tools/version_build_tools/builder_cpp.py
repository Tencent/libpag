from builder import Builder


class CppBuilder(Builder):
    def __init__(self,  output_file):
        super().__init__(output_file)

    def build(self):
        # print("build version.h start")
        self.buildHead()
        self.buildBody()
        self.buildTail()
        super().build()
        # print("build version.h end")

    def buildHead(self):
        # print("build version.h head")
        self.write(cpp_head)

    def buildBody(self):
        # print("build version.h body")
        parser = self.parser
        section = "const std::string {} = \"{}\";\n"
        version = f'{parser.version_major}.{parser.version_minor}.{parser.version_fix}.{parser.build_number}'
        plugin_version = f'{parser.version_major}.{parser.version_minor}.{parser.version_fix}'
        self.write(section.format("AppVersion", version))
        self.write(section.format("UpdateChannel", "beta" if parser.is_beta_version else "stable"))
        self.write(section.format("AEPluginVersion", plugin_version))

    def buildTail(self):
        # print("build version.h tail")
        self.write(cpp_tail)


cpp_head = '''
#ifndef COMMON_VERSION_H_
#define COMMON_VERSION_H_

#include <string>

'''

cpp_tail = '''
#endif //COMMON_VERSION_H_
'''
