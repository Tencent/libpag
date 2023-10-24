PAG_ROOT = __dir__
TGFX_ROOT = PAG_ROOT+"/tgfx"

tgfxVendors = "pathkit skcms libwebp"
commonCFlags = ["-DGLES_SILENCE_DEPRECATION -DTGFX_USE_WEBP_DECODE -DPAG_DLL -fvisibility=hidden -Wall -Wextra -Weffc++ -pedantic -Werror=return-type"]
# PAG_USE_FREETYPE=ON pod install
if ENV["PAG_USE_FREETYPE"] == 'ON' and ENV["PLATFORM"] == "mac"
  tgfxVendors += " freetype"
  commonCFlags += ["-DTGFX_USE_FREETYPE"]
  $rasterSourceFiles = ['tgfx/src/vectors/freetype/**/*.{h,cpp,mm}']

else
  commonCFlags += ["-DTGFX_USE_CORE_GRAPHICS"]
  $rasterSourceFiles = ['tgfx/src/vectors/coregraphics/**/*.{h,cpp,mm}']
end


if  ENV["PLATFORM"] == "mac"
  system("depsync mac")
  system("cd tgfx && node build_vendor #{tgfxVendors} -o #{PAG_ROOT}/mac/Pods/tgfx-vendor -p mac --xcframework")
else
  system("depsync ios")
  system("cd tgfx && node build_vendor #{tgfxVendors} -o #{PAG_ROOT}/ios/Pods/tgfx-vendor -p ios --xcframework")
end

ios_vendored_frameworks = ['ios/Pods/tgfx-vendor/libtgfx-vendor.xcframework']
mac_vendored_frameworks = ['mac/Pods/tgfx-vendor/libtgfx-vendor.xcframework']

# PAG_USE_LIBAVC=OFF pod install
if ENV["PAG_USE_LIBAVC"] != 'OFF'
  commonCFlags += ["-DPAG_USE_LIBAVC"]
  if  ENV["PLATFORM"] == "mac"
    system("node build_vendor libavc -o #{PAG_ROOT}/mac/Pods/pag-vendor -p mac --xcframework")
    mac_vendored_frameworks += ['mac/Pods/pag-vendor/libpag-vendor.xcframework']
  else
    system("node build_vendor libavc -o #{PAG_ROOT}/ios/Pods/pag-vendor -p ios --xcframework")
    ios_vendored_frameworks += ['ios/Pods/pag-vendor/libpag-vendor.xcframework']
  end
end


iosSourceFiles = ['src/platform/ios/*.{h,cpp,mm,m}',
                  'src/platform/ios/private/*.{h,cpp,mm,m}',
                  'src/platform/cocoa/**/*.{h,cpp,mm,m}',
                  'tgfx/src/opengl/eagl/*.{h,cpp,mm}',
                  'tgfx/src/platform/apple/**/*.{h,cpp,mm,m}']

if ENV["PAG_USE_QT"] == 'ON'
  macSourceFiles = ['src/platform/qt/**/*.{h,cpp,mm,m}',
                    'tgfx/src/platform/apple/*.{h,cpp,m,mm}',
                    'tgfx/src/opengl/qt/*.{h,cpp,mm}',
                    'tgfx/src/opengl/cgl/CGLHardwareTexture.mm',
                    'src/platform/mac/private/HardwareDecoder.mm']
else
  macSourceFiles = ['src/platform/mac/**/*.{h,cpp,mm,m}',
                    'src/platform/cocoa/**/*.{h,cpp,mm,m}',
                    'tgfx/src/opengl/cgl/*.{h,cpp,mm}',
                    'tgfx/src/platform/apple/**/*.{h,cpp,mm,m}']
end

cSourceFiles = []
if ENV["PAG_USE_C"] == 'ON'
  cSourceFiles += ["src/c/*.*"]
  iosSourceFiles += ["src/c/*.{h,cpp,mm,m}", "src/c/ext/*.{h,cpp,mm,m}"]
  macSourceFiles += ["src/c/*.{h,cpp,mm,m}", "src/c/ext/*.{h,cpp,mm,m}"]
end

Pod::Spec.new do |s|
  s.name     = 'libpag'
  s.version  = '4.0.0'
  s.ios.deployment_target   = '9.0'
  s.osx.deployment_target   = '10.13'
  s.summary  = 'The pag library on iOS and macOS'
  s.homepage = 'https://pag.art'
  s.author   = { 'dom' => 'dom@idom.me' }
  s.requires_arc = false
  s.source   = {
    :git => 'https://github.com/tencent/libpag.git'
  }

  $source_files =   'include/pag/defines.h',
                    'include/pag/types.h',
                    'include/pag/gpu.h',
                    'include/pag/file.h',
                    'include/pag/pag.h',
                    'src/base/**/*.{h,cpp}',
                    'src/codec/**/*.{h,cpp}',
                    'src/video/**/*.{h,cpp}',
                    'src/rendering/**/*.{h,cpp}',
                    'src/platform/*.{h,cpp}',
                    'tgfx/src/core/*.{h,cpp}',
                    'tgfx/src/codecs/*.{h,cpp}',
                    'tgfx/src/effects/**/*.{h,cpp}',
                    'tgfx/src/images/**/*.{h,cpp}',
                    'tgfx/src/shaders/**/*.{h,cpp}',
                    'tgfx/src/shapes/**/*.{h,cpp}',
                    'tgfx/src/utils/**/*.{h,cpp}',
                    'tgfx/src/vectors/*.{h,cpp}',
                    'tgfx/src/gpu/**/*.{h,cpp}',
                    'tgfx/src/opengl/*.{h,cpp,mm}',
                    'tgfx/src/opengl/processors/**/*.{h,cpp}',
                    'tgfx/src/platform/*.{h,cpp}',
                    'tgfx/src/codecs/webp/**/*.{h,cpp,mm}'

  s.source_files = $source_files + $rasterSourceFiles + cSourceFiles;

  s.compiler_flags = '-Wno-documentation'

  if ENV["PAG_USE_QT"] == 'ON'
    s.osx.public_header_files = ['src/platform/qt/*.h', 'tgfx/src/opengl/*.h']
  else
    s.osx.public_header_files = 'src/platform/mac/*.h',
                                'src/platform/cocoa/*.h'
  end
  
  s.osx.source_files = macSourceFiles

  s.osx.frameworks   = ['ApplicationServices', 'AGL', 'OpenGL', 'QuartzCore', 'Cocoa', 'Foundation', 'VideoToolbox', 'CoreMedia']
  s.osx.libraries = ["iconv", "c++", "compression"]

  s.ios.public_header_files = 'src/platform/ios/*.h',
                              'src/platform/cocoa/*.h'
                              
  s.ios.source_files = iosSourceFiles

  s.ios.frameworks   = ['UIKit', 'CoreFoundation', 'QuartzCore', 'CoreGraphics', 'CoreText', 'OpenGLES', 'VideoToolbox', 'CoreMedia']
  s.ios.libraries = ["iconv", "compression"]

  armv7CFlags = commonCFlags + ["-fno-aligned-allocation"]
  s.xcconfig = {'CLANG_CXX_LANGUAGE_STANDARD' => 'c++17','CLANG_CXX_LIBRARY' => 'libc++',"HEADER_SEARCH_PATHS" => "#{PAG_ROOT}/src #{PAG_ROOT}/include #{TGFX_ROOT}/src #{TGFX_ROOT}/include #{TGFX_ROOT}/third_party/pathkit #{TGFX_ROOT}/third_party/skcms #{TGFX_ROOT}/third_party/freetype/include #{TGFX_ROOT}/third_party/libwebp/src #{PAG_ROOT}/third_party/libavc/common #{PAG_ROOT}/third_party/libavc/decoder"}
  s.ios.xcconfig = {"OTHER_CFLAGS" => commonCFlags.join(" "),"OTHER_CFLAGS[sdk=iphoneos*][arch=armv7]" => armv7CFlags.join(" "),"EXPORTED_SYMBOLS_FILE" => "${PODS_ROOT}/../libpag.lds","OTHER_LDFLAGS" => "-w -ld_classic","VALIDATE_WORKSPACE_SKIPPED_SDK_FRAMEWORKS" => "OpenGLES"}
  s.osx.xcconfig = {"OTHER_CFLAGS" => commonCFlags.join(" ")}
  s.ios.vendored_frameworks = ios_vendored_frameworks
  s.osx.vendored_frameworks = mac_vendored_frameworks

end
