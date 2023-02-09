
package org.extra.relinker;

import android.content.Context;
import android.util.Log;

import org.extra.relinker.elf.ElfParser;

import java.io.File;
import java.io.FilenameFilter;
import java.io.IOException;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

public class ReLinkerInstance {
    private static final String LIB_DIR = "lib";

    protected final Set<String> loadedLibraries = new HashSet<String>();
    protected final ReLinker.LibraryLoader libraryLoader;
    protected final ReLinker.LibraryInstaller libraryInstaller;

    protected boolean force;
    protected boolean recursive;
    protected ReLinker.Logger logger;

    protected ReLinkerInstance() {
        this(new SystemLibraryLoader(), new ApkLibraryInstaller());
    }

    protected ReLinkerInstance(final ReLinker.LibraryLoader libraryLoader,
                               final ReLinker.LibraryInstaller libraryInstaller) {
        if (libraryLoader == null) {
            throw new IllegalArgumentException("Cannot pass null library loader");
        }

        if (libraryInstaller == null) {
            throw new IllegalArgumentException("Cannot pass null library installer");
        }

        this.libraryLoader = libraryLoader;
        this.libraryInstaller = libraryInstaller;
    }

    /**
     * Logs debugging related information to the {@link ReLinker.Logger} instance given
     */
    public ReLinkerInstance log(final ReLinker.Logger logger) {
        this.logger = logger;
        return this;
    }

    /**
     * Forces any previously extracted / re-linked libraries to be cleaned up before loading
     */
    public ReLinkerInstance force() {
        this.force = true;
        return this;
    }

    /**
     * Enables recursive library loading to resolve and load shared object -> shared object
     * defined dependencies
     */
    public ReLinkerInstance recursively() {
        this.recursive = true;
        return this;
    }

    /**
     * Utilizes the regular system call to attempt to load a native library. If a failure occurs,
     * then the function extracts native .so library out of the app's APK and attempts to load it.
     * <p>
     *     <strong>Note: This is a synchronous operation</strong>
     */
    public void loadLibrary(final Context context, final String library) {
        loadLibrary(context, library, null, null);
    }

    /**
     * The same call as {@link #loadLibrary(Context, String)}, however if a {@code version} is
     * provided, then that specific version of the given library is loaded.
     */
    public void loadLibrary(final Context context, final String library, final String version) {
        loadLibrary(context, library, version, null);
    }

    /**
     * The same call as {@link #loadLibrary(Context, String)}, however if a
     * {@link ReLinker.LoadListener} is provided, the function is executed asynchronously.
     */
    public void loadLibrary(final Context context,
                            final String library,
                            final ReLinker.LoadListener listener) {
        loadLibrary(context, library, null, listener);
    }

    /**
     * Attemps to load the given library normally. If that fails, it loads the library utilizing
     * a workaround.
     *
     * @param context The {@link Context} to get a workaround directory from
     * @param library The library you wish to load
     * @param version The version of the library you wish to load, or {@code null}
     * @param listener {@link ReLinker.LoadListener} to listen for async execution, or {@code null}
     */
    public void loadLibrary(final Context context,
                            final String library,
                            final String version,
                            final ReLinker.LoadListener listener) {
        if (context == null) {
            throw new IllegalArgumentException("Given context is null");
        }

        if (TextUtils.isEmpty(library)) {
            throw new IllegalArgumentException("Given library is either null or empty");
        }

        log("Beginning load of %s...", library);
        if (listener == null) {
            loadLibraryInternal(context, library, version);
        } else {
            try {
                new Thread(new Runnable() {
                    @Override
                    public void run() {
                        try {
                            loadLibraryInternal(context, library, version);
                            listener.success();
                        } catch (UnsatisfiedLinkError e) {
                            listener.failure(e);
                        } catch (MissingLibraryException e) {
                            listener.failure(e);
                        }
                    }
                }).start();
            } catch (Exception | Error e) {
                listener.failure(e);
            }
        }
    }

    private void loadLibraryInternal(final Context context,
                                     final String library,
                                     final String version) {
        if (loadedLibraries.contains(library) && !force) {
            log("%s already loaded previously!", library);
            return;
        }

        try {
            libraryLoader.loadLibrary(library);
            loadedLibraries.add(library);
            log("%s (%s) was loaded normally!", library, version);
            return;
        } catch (final UnsatisfiedLinkError e) {
            // :-(
            log("Loading the library normally failed: %s", Log.getStackTraceString(e));
        }

        log("%s (%s) was not loaded normally, re-linking...", library, version);
        final File workaroundFile = getWorkaroundLibFile(context, library, version);
        if (!workaroundFile.exists() || force) {
            if (force) {
                log("Forcing a re-link of %s (%s)...", library, version);
            }

            cleanupOldLibFiles(context, library, version);
            libraryInstaller.installLibrary(context, libraryLoader.supportedAbis(),
                    libraryLoader.mapLibraryName(library), workaroundFile, this);
        }

        try {
            if (recursive) {
                ElfParser parser = null;
                final List<String> dependencies;
                try {
                    parser = new ElfParser(workaroundFile);
                    dependencies = parser.parseNeededDependencies();
                }finally {
                    parser.close();
                }
                for (final String dependency : dependencies) {
                    loadLibrary(context, libraryLoader.unmapLibraryName(dependency));
                }
            }
        } catch (IOException ignored) {
            // This a redundant step of the process, if our library resolving fails, it will likely
            // be picked up by the system's resolver, if not, an exception will be thrown by the
            // next statement, so its better to try twice.
        }

        libraryLoader.loadPath(workaroundFile.getAbsolutePath());
        loadedLibraries.add(library);
        log("%s (%s) was re-linked!", library, version);
    }

    /**
     * @param context {@link Context} to describe the location of it's private directories
     * @return A {@link File} locating the directory that can store extracted libraries
     * for later use
     */
    protected File getWorkaroundLibDir(final Context context) {
        return context.getDir(LIB_DIR, Context.MODE_PRIVATE);
    }

    /**
     * @param context {@link Context} to retrieve the workaround directory from
     * @param library The name of the library to load
     * @param version The version of the library to load or {@code null}
     * @return A {@link File} locating the workaround library file to load
     */
    protected File getWorkaroundLibFile(final Context context,
                                        final String library,
                                        final String version) {
        final String libName = libraryLoader.mapLibraryName(library);

        if (TextUtils.isEmpty(version)) {
            return new File(getWorkaroundLibDir(context), libName);
        }

        return new File(getWorkaroundLibDir(context), libName + "." + version);
    }

    /**
     * Cleans up any <em>other</em> versions of the {@code library}. If {@code force} is used, all
     * versions of the {@code library} are deleted
     *
     * @param context {@link Context} to retrieve the workaround directory from
     * @param library The name of the library to load
     * @param currentVersion The version of the library to keep, all other versions will be deleted.
     *                       This parameter is ignored if {@code force} is used.
     */
    protected void cleanupOldLibFiles(final Context context,
                                      final String library,
                                      final String currentVersion) {
        final File workaroundDir = getWorkaroundLibDir(context);
        final File workaroundFile = getWorkaroundLibFile(context, library, currentVersion);
        final String mappedLibraryName = libraryLoader.mapLibraryName(library);
        final File[] existingFiles = workaroundDir.listFiles(new FilenameFilter() {
            @Override
            public boolean accept(File dir, String filename) {
                return filename.startsWith(mappedLibraryName);
            }
        });

        if (existingFiles == null) return;

        for (final File file : existingFiles) {
            if (force || !file.getAbsolutePath().equals(workaroundFile.getAbsolutePath())) {
                try {
                    file.delete();
                } catch (SecurityException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    public void log(final String format, final Object... args) {
        log(String.format(Locale.US, format, args));
    }

    public void log(final String message) {
        if (logger != null) {
            logger.log(message);
        }
    }
}
