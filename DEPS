{
  "version": "1.3.1",
  "vars": {
    "PAG_GROUP": "https://github.com/libpag"
  },
  "repos": {
    "common": [
      {
        "url": "${PAG_GROUP}/vendor_tools.git",
        "commit": "b6933db85a6fbecb0d8e64796c1ce6508a235c13",
        "dir": "third_party/vendor_tools"
      },
      {
        "url": "${PAG_GROUP}/tgfx.git",
        "commit": "42b9966561d1247e053e345c41f216a33e08ea1c",
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
