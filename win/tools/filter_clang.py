"""Strips GCC-only warning flags that clang-cl rejects as unknown arguments.

tgfx and libpag append -Werror/-Weffc++/-Wconversion/-pedantic unconditionally. The clang-cl
autotest lane keeps those diagnostics out of the build instead of editing third-party CMake.
"""

import subprocess
import sys

DROP_FLAGS = {"-pedantic", "-Weffc++", "-Wconversion", "-Werror"}

arguments = [argument for argument in sys.argv[1:] if argument not in DROP_FLAGS]
sys.exit(subprocess.call(arguments))
