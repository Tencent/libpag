
package org.extra.relinker;

public class MissingLibraryException extends RuntimeException {
    public MissingLibraryException(final String library) {
        super(library);
    }
}
