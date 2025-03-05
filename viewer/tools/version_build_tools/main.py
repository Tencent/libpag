import os
import sys
import argparse
from builder_cpp import CppBuilder


def createBuilder(builder_type, output_file):
    if builder_type == 'cpp':
        return CppBuilder(output_file)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-t', '--type', required=True, help='语言类型', choices=['cpp', 'java', 'swift', 'dart'],)
    parser.add_argument('-o', '--output', default='', help='Output File', metavar='version.h')
    args = parser.parse_args()

    if args.output == '':
        if args.type == 'cpp':
            args.output = "version.h"

    builder = createBuilder(args.type, args.output)
    builder.build()
