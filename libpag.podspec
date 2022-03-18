PAG_ROOT = __dir__

vendorNames = "pathkit skcms libwebp"
commonCFlags = ["-DGLES_SILENCE_DEPRECATION -DTGFX_USE_WEBP_DECODE -DPAG_DLL -fvisibility=hidden -Wall -Wextra -Weffc++ -pedantic -Werror=return-type"]
# PAG_USE_FREETYPE=ON pod install
if ENV["PAG_USE_FREETYPE"] == 'ON' and ENV["PLATFORM"] == "mac"
  vendorNames += " freetype"
  commonCFlags += ["-DTGFX_USE_FREETYPE"]
  $rasterSourceFiles = ['tgfx/src/core/vectors/freetype/**/*.{h,cpp,mm}']

else
  commonCFlags += ["-DTGFX_USE_CORE_GRAPHICS"]
  $rasterSourceFiles = ['tgfx/src/core/vectors/coregraphics/**/*.{h,cpp,mm}']
end
# PAG_USE_LIBAVC=OFF pod install
if ENV["PAG_USE_LIBAVC"] != 'OFF'
  vendorNames += " libavc"
  commonCFlags += ["-DPAG_USE_LIBAVC"]
end

if  ENV["PLATFORM"] == "mac"
  system("depsync mac")
  system("node build_vendor #{vendorNames} -o #{PAG_ROOT}/mac/Pods/pag-vendor -p mac --xcframework")
else
  system("depsync ios")
  system("node build_vendor #{vendorNames} -o #{PAG_ROOT}/ios/Pods/pag-vendor -p ios --xcframework")
end

Pod::Spec.new do |s|
  s.name     = 'libpag'
  s.version  = '4.0.0'
  s.ios.deployment_target   = '9.0'
  s.osx.deployment_target   = '10.13'
  s.summary  = 'The pag library on iOS and macOS'
  s.homepage = 'https://pag.io'
  s.author   = { 'dom' => 'dom@idom.me' }
  s.requires_arc = false
  s.source   = {
    :git => 'https://github.com/tencent/libpag.git'
  }

  $source_files =   'include/pag/defines.h',
                    'include/pag/types.h',
                    'include/pag/gpu.h',
                    'include/pag/audio.h',
                    'include/pag/decoder.h',
                    'include/pag/file.h',
                    'include/pag/pag.h',
                    'src/base/**/*.{h,cpp}',
                    'src/codec/**/*.{h,cpp}',
                    'src/video/**/*.{h,cpp}',
                    'src/rendering/**/*.{h,cpp}',
                    'src/platform/*.{h,cpp}',
                    'tgfx/src/core/*.{h,cpp}',
                    'tgfx/src/core/images/*.{h,cpp}',
                    'tgfx/src/core/vectors/*.{h,cpp}',
                    'tgfx/src/gpu/*.{h,cpp}',
                    'tgfx/src/platform/*.{h,cpp}',
                    'tgfx/src/core/utils/**/*.{h,cpp}',
                    'tgfx/src/gpu/gradients/**/*.{h,cpp}',
                    'tgfx/src/gpu/opengl/*.{h,cpp,mm}',
                    'tgfx/src/core/images/webp/**/*.{h,cpp,mm}',
                    'tgfx/src/core/vectors/coregraphics/**/*.{h,cpp,mm}',
                    'vendor/parser/include/**/*.{h}'

   $source_files_audio = 'vendor/sonic/*.{h,c}',
                        'src/audio/**/*.{h,cpp}'		    

  s.source_files = $source_files + $rasterSourceFiles;

  s.compiler_flags = '-Wno-documentation'

  s.osx.public_header_files = 'src/platform/mac/*.h',
                              'src/platform/cocoa/*.h'
  s.osx.source_files =  'src/platform/mac/**/*.{h,cpp,mm,m}',
                        'src/platform/cocoa/**/*.{h,cpp,mm,m}',
                        'tgfx/src/gpu/opengl/cgl/*.{h,cpp,mm}',
                        'tgfx/src/platform/mac/**/*.{h,cpp,mm,m}',
                        'tgfx/src/platform/apple/**/*.{h,cpp,mm,m}'

  s.osx.frameworks   = ['ApplicationServices', 'AGL', 'OpenGL', 'QuartzCore', 'Cocoa', 'Foundation', 'VideoToolbox', 'CoreMedia']
  s.osx.libraries = ["iconv", "c++"]

  s.ios.public_header_files = 'src/platform/ios/*.h',
                              'src/platform/cocoa/*.h'
                              
  s.ios.source_files =  'src/platform/ios/*.{h,cpp,mm,m}',
                          'src/platform/ios/private/report/*.{h,cpp,mm,m}',
                          'src/platform/ios/private/*.{h,cpp,mm,m}',
                          'src/platform/cocoa/**/*.{h,cpp,mm,m}',
                          'tgfx/src/gpu/opengl/eagl/*.{h,cpp,mm}',
                          'tgfx/src/platform/ios/**/*.{h,cpp,mm,m}',
                          'tgfx/src/platform/apple/**/*.{h,cpp,mm,m}'

  s.ios.frameworks   = ['UIKit', 'CoreFoundation', 'QuartzCore', 'CoreGraphics', 'CoreText', 'OpenGLES', 'VideoToolbox', 'CoreMedia']
  s.ios.libraries = ["iconv"]
  
   #具体使用
  # 默认打开MOVIE选项，默认打开FFMPEG
  # libpag_config=NONE pod install 关闭FILE_MOVIE、FFMPEG宏， 不带PAGMovie的版本编译arm64、armv7时使用
  # libpag_config=FFMPEG pod install 打开FFMPEG宏，不带PAGMovie的版本编译x86_64时使用
  # libpag_config=PUBLIC_NO_FFMPEG pod install 打开FFMPEG宏,关闭统计上报,关闭FFmpeg，编译arm64、armv7时使用
  # libpag_config=PUBLIC pod install 打开FFMPEG宏,关闭统计上报，编译外网版本时使用
  # libpag_config=MOVIE_NOFFMPEG pod install 打开FILE_MOVIE，关闭FFMPEG宏，PAGMovie的版本使用，不内置软解
  # pod install 默认配置,打开FILE_MOVI、FFMPEG，PAGMovie的版本使用，内置软解
  
  commonCFlags += ["-DGLES_SILENCE_DEPRECATION -DHARDWARE_DECODER"]
  COMMON_C_FLAGS_MAC = ["-DHARDWARE_DECODER"]
  if $lib_name == 'NONE'
    OPTION_C_FLAGS=[""]

  elsif $lib_name == 'FFMPEG'
    OPTION_C_FLAGS = ["-DFFMPEG"]
    $FFMPEG_ios_Library = 'vendor/ffmpeg/ios/sequence/*.a'
      
  elsif $lib_name == 'PUBLIC'
    OPTION_C_FLAGS = ["-DFFMPEG"]
    $FFMPEG_ios_Library = 'vendor/ffmpeg/ios/sequence/*.a'
    s.ios.source_files = $source_files_noreport

  elsif $lib_name == 'PUBLIC_NO_FFMPEG'
    OPTION_C_FLAGS = [""]
    s.ios.source_files = $source_files_noreport
      
  elsif $lib_name == 'MOVIE_NOFFMPEG'
    OPTION_C_FLAGS = ["-DFILE_MOVIE -DFFMPEG"]
    $FFMPEG_ios_Library = 'vendor/ffmpeg/ios/movie_no_videodecoder/*.a'
    $FFMPEG_mac_Library = 'vendor/ffmpeg/mac/*.a'
    s.source_files = $source_files + $source_files_audio
    s.ios.public_header_files = 'src/platform/ios/*.h',
                                    'src/platform/cocoa/*.h',
                                    'src/platform/ios/private/report/PAGReporter.h'

  else
    OPTION_C_FLAGS = ["-DFILE_MOVIE -DFFMPEG"]
    $FFMPEG_ios_Library = 'vendor/ffmpeg/ios/movie/*.a'
    $FFMPEG_mac_Library = 'vendor/ffmpeg/mac/*.a'
    s.source_files = $source_files + $source_files_audio
  end

  commonCFlags += OPTION_C_FLAGS;
  OTHER_C_FLAGS_MAC = COMMON_C_FLAGS_MAC + OPTION_C_FLAGS;

  armv7CFlags = commonCFlags + ["-fno-aligned-allocation"]
  s.xcconfig = {'CLANG_CXX_LANGUAGE_STANDARD' => 'c++17','CLANG_CXX_LIBRARY' => 'libc++',"HEADER_SEARCH_PATHS" => "#{PAG_ROOT}/src #{PAG_ROOT}/include #{PAG_ROOT}/tgfx/src #{PAG_ROOT}/tgfx/include #{PAG_ROOT}/third_party/pathkit #{PAG_ROOT}/third_party/skcms #{PAG_ROOT}/third_party/freetype/include #{PAG_ROOT}/third_party/libwebp/src #{PAG_ROOT}/third_party/libavc/common #{PAG_ROOT}/third_party/libavc/decoder ${PODS_ROOT}/#{s.name}/vendor/ffmpeg/ios/include ${PODS_ROOT}/../../vendor/ffmpeg/ios/include ${PODS_ROOT}/../../libpag/vendor/ffmpeg/ios/include ${PODS_ROOT}/../../vendor/parser/include/** ${PODS_ROOT}/../../libpag/vendor/parser/include/**"}
  s.ios.xcconfig = {"OTHER_CFLAGS" => commonCFlags.join(" "),"OTHER_CFLAGS[sdk=iphoneos*][arch=armv7]" => armv7CFlags.join(" "),"EXPORTED_SYMBOLS_FILE" => "${PODS_ROOT}/../libpag.lds","OTHER_LDFLAGS" => "-w","VALIDATE_WORKSPACE_SKIPPED_SDK_FRAMEWORKS" => "OpenGLES"}
  s.osx.xcconfig = {"OTHER_CFLAGS" => commonCFlags.join(" ")}
  s.ios.vendored_libraries  = $FFMPEG_ios_Library
  s.ios.vendored_frameworks = 'ios/Pods/pag-vendor/libpag-vendor.xcframework'
  s.osx.vendored_frameworks = 'mac/Pods/pag-vendor/libpag-vendor.xcframework'

end
