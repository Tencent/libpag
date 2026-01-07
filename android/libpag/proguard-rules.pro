# Add project specific ProGuard rules here.
# By default, the flags in this file are appended to flags specified
# in /Users/dom/Library/Android/sdk/tools/proguard/proguard-android.txt
# You can edit the include path and order by changing the proguardFiles
# directive in build.gradle.
#
# For more details, see
#   http://developer.android.com/guide/developing/tools/proguard.html

# Add any project specific keep options here:

# If your project uses WebView with JS, uncomment the following
# and specify the fully qualified class name to the JavaScript interface
# class:
#-keepclassmembers class fqcn.of.javascript.interface.for.webview {
#   public *;
#}

# Uncomment this to preserve the line number information for
# debugging stack traces.
-keepattributes SourceFile,LineNumberTable

# If you keep the line number information, uncomment this to
# hide the original source file name.
-renamesourcefileattribute SourceFile

-keep class **.R$* {
    *;
}

-keeppackagenames org.libpag.**

-keeppackagenames org.extra.**

-keepclasseswithmembers class ** {
    native <methods>;
}

-keepclasseswithmembers public class org.libpag.** {
    public <methods>;
}

-keepclasseswithmembers public class org.libpag.** {
    public <fields>;
}

-keepclasseswithmembers class org.libpag.** {
    long nativeContext;
}

-keepclasseswithmembers class org.libpag.** {
    long nativeSurface;
}

-keepclasseswithmembers class org.libpag.PAGFile {
   private <init>(long);
}

-keepclasseswithmembers class org.libpag.PAGFont {
    private static void RegisterFallbackFonts();
}

-keepclasseswithmembers class org.libpag.PAGDiskCache {
    private static java.lang.String GetDefaultCacheDir();
}

-keepclasseswithmembers class org.libpag.VideoSurface {
    <methods>;
}

-keepclasseswithmembers class org.libpag.DisplayLink {
    <methods>;
}

-keepclasseswithmembers class org.libpag.PAGAnimator {
    private void onAnimationStart();
    private void onAnimationEnd();
    private void onAnimationCancel();
    private void onAnimationRepeat();
    private void onAnimationUpdate();
}
