
package org.extra.relinker;

import android.os.Build;

@SuppressWarnings("deprecation")
final class SystemLibraryLoader implements ReLinker.LibraryLoader {
    @Override
    public void loadLibrary(final String libraryName) {
        System.loadLibrary(libraryName);
    }

    @Override
    public void loadPath(final String libraryPath) {
        System.load(libraryPath);
    }

    @Override
    public String mapLibraryName(final String libraryName) {
        if (libraryName.startsWith("lib") && libraryName.endsWith(".so")) {
            // Already mapped
            return libraryName;
        }

        return System.mapLibraryName(libraryName);
    }

    @Override
    public String unmapLibraryName(String mappedLibraryName) {
        // Assuming libname.so
        return mappedLibraryName.substring(3, mappedLibraryName.length() - 3);
    }

    @Override
    public String[] supportedAbis() {
        if (Build.VERSION.SDK_INT >= 21 && Build.SUPPORTED_ABIS.length > 0) {
            return Build.SUPPORTED_ABIS;
        } else if (!TextUtils.isEmpty(Build.CPU_ABI2)) {
            return new String[] {Build.CPU_ABI, Build.CPU_ABI2};
        } else {
            return new String[] {Build.CPU_ABI};
        }
    }
}
