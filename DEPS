{
  "version": "1.2.2",
  "vars": {
    "PAG_GROUP": "https://github.com/libpag"
  },
  "repos": {
    "common": [
      {
        "url": "${PAG_GROUP}/vendor_tools.git",
        "commit": "27837194cf1834bcdf2bc2643655bdebcf2a67e9",
        "dir": "vendor_tools"
      },
      {
        "url": "${PAG_GROUP}/pathkit.git",
        "commit": "cb2e612b0fe03c56bd3b8ea3d87ceee35d438c4a",
        "dir": "third_party/pathkit"
      },
      {
        "url": "${PAG_GROUP}/skcms.git",
        "commit": "22e4cd22d87ff9074cda4af6b4121059dbf9a90e",
        "dir": "third_party/skcms"
      },
      {
        "url": "${PAG_GROUP}/libavc.git",
        "commit": "dfc550bc75eef789eae8f0b0bfb954e11f7740e4",
        "dir": "third_party/libavc"
      },
      {
        "url": "${PAG_GROUP}/swiftshader.git",
        "commit": "8d781475d99e8c1582f4c0da44b54ba105f8b79a",
        "dir": "third_party/swiftshader"
      },
      {
        "url": "https://github.com/madler/zlib.git",
        "commit": "cacf7f1d4e3d44d871b605da3b647f07d718623f",
        "dir": "third_party/zlib"
      },
      {
        "url": "https://github.com/glennrp/libpng.git",
        "commit": "a40189cf881e9f0db80511c382292a5604c3c3d1",
        "dir": "third_party/libpng"
      },
      {
        "url": "https://github.com/webmproject/libwebp.git",
        "commit": "9ce5843dbabcfd3f7c39ec7ceba9cbeb213cbfdf",
        "dir": "third_party/libwebp"
      },
      {
        "url": "https://github.com/libjpeg-turbo/libjpeg-turbo.git",
        "commit": "0a9b9721782d3a60a5c16c8c9a7abf3d4b1ecd42",
        "dir": "third_party/libjpeg-turbo"
      },
      {
        "url": "https://github.com/freetype/freetype.git",
        "commit": "86bc8a95056c97a810986434a3f268cbe67f2902",
        "dir": "third_party/freetype"
      },
      {
        "url": "https://github.com/rttrorg/rttr.git",
        "commit": "7edbd580cfad509a3253c733e70144e36f02ecd4",
        "dir": "third_party/rttr"
      },
      {
        "url": "https://github.com/google/googletest.git",
        "commit": "e2239ee6043f73722e7aa812a459f54a28552929",
        "dir": "third_party/googletest"
      },
      {
        "url": "https://github.com/nlohmann/json.git",
        "commit": "fec56a1a16c6e1c1b1f4e116a20e79398282626c",
        "dir": "third_party/json"
      }
    ]
  },
  "files": {
    "mac": [
      {
        "url": "${PAG_GROUP}/swiftshader/releases/download/opengl/mac.zip",
        "dir": "third_party/out/swiftshader",
        "unzip": true
      },
      {
        "url": "${PAG_GROUP}/ffavc/releases/download/0.9.0/ffavc_0.9.0_release_mac_x64.zip",
        "dir": "vendor/ffavc",
        "unzip": true
      }
    ],
    "win": [
      {
        "url": "${PAG_GROUP}/swiftshader/releases/download/opengl/win.zip",
        "dir": "third_party/out/swiftshader",
        "unzip": true
      },
      {
        "url": "${PAG_GROUP}/angle/releases/download/chromium_4763/win.zip",
        "dir": "vendor/angle",
        "unzip": true
      }
    ]
  },
  "actions": {
    "common": [
      {
        "command": "depsync --clean",
        "dir": "third_party"
      }
    ]
  }
}