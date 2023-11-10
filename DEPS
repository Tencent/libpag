{
  "version": "1.3.1",
  "vars": {
    "PAG_GROUP": "https://github.com/libpag"
  },
  "repos": {
    "common": [
      {
        "url": "${PAG_GROUP}/vendor_tools.git",
        "commit": "feae2a0b3090225e5f03426e67821007aaa1f30c",
        "dir": "third_party/vendor_tools"
      },
      {
        "url": "${PAG_GROUP}/tgfx.git",
        "commit": "4aa5a597530a3ea81595e78b0c0c9d3b03408d04",
        "dir": "third_party/tgfx",
        "keeps": [
          "third_party",
          "out"
        ]
      },
      {
        "url": "${PAG_GROUP}/libavc.git",
        "commit": "c2bf4c84a6d39788929e59514417e819185af98e",
        "dir": "third_party/libavc"
      },
      {
        "url": "https://github.com/rttrorg/rttr.git",
        "commit": "7edbd580cfad509a3253c733e70144e36f02ecd4",
        "dir": "third_party/rttr"
      },
      {
        "url": "https://github.com/harfbuzz/harfbuzz.git",
        "commit": "f1f2be776bcd994fa9262622e1a7098a066e5cf7",
        "dir": "third_party/harfbuzz"
      },
      {
        "url": "https://github.com/lz4/lz4.git",
        "commit": "5ff839680134437dbf4678f3d0c7b371d84f4964",
        "dir": "third_party/lz4"
      }
    ]
  },
  "actions": {
    "common": [
      {
        "command": "depsync --clean",
        "dir": "third_party"
      },
      {
        "command": "git lfs pull",
        "dir": "./"
      }
    ]
  }
}
