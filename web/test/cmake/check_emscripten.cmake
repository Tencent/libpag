# Verify the Emscripten environment for web test builds. Homebrew's emscripten package differs
# from emsdk in toolchain layout, sysroot paths, and default compiler flags, causing build
# failures and runtime crashes. Always use emsdk with `source <path-to-emsdk>/emsdk_env.sh`.
#
# The version window mirrors web/emsdk-versions.txt (read by the root CMakeLists.txt for the
# product build): -sUSE_WEBGPU was removed in 4.0.18 in favor of --use-port=emdawnwebgpu.
if (EMSCRIPTEN)
    if (NOT DEFINED ENV{EMSDK})
        message(FATAL_ERROR
            "EMSDK environment variable not set. "
            "You appear to be using a system-installed Emscripten (e.g. from Homebrew) "
            "which is not compatible with this project.\n"
            "Please install and activate emsdk 4.0.15:\n"
            "  node web/script/setup.emsdk.js\n"
            "  source web/emsdk/emsdk_env.sh\n"
            "See web/test/README.md for details.")
    elseif (EMSCRIPTEN_VERSION VERSION_LESS "4.0.15")
        message(FATAL_ERROR
            "Emscripten ${EMSCRIPTEN_VERSION} is too old. "
            "This project requires emsdk >= 4.0.15.\n"
            "  node web/script/setup.emsdk.js\n"
            "  source web/emsdk/emsdk_env.sh")
    elseif (EMSCRIPTEN_VERSION VERSION_GREATER_EQUAL "4.0.18")
        message(FATAL_ERROR
            "Emscripten ${EMSCRIPTEN_VERSION} is not supported. "
            "Emscripten >= 4.0.18 removed -sUSE_WEBGPU in favor of --use-port=emdawnwebgpu. "
            "Please use emsdk 4.0.15:\n"
            "  node web/script/setup.emsdk.js\n"
            "  source web/emsdk/emsdk_env.sh")
    endif ()
endif ()
