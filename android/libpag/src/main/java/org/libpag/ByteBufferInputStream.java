package org.libpag;

import java.io.IOException;
import java.io.InputStream;
import java.nio.ByteBuffer;

class ByteBufferInputStream extends InputStream {
    ByteBuffer buffer;

    public ByteBufferInputStream(ByteBuffer buf) {
        this.buffer = buf;
        this.buffer.position(0);
    }

    public int read() throws IOException {
        if (!buffer.hasRemaining()) {
            return -1;
        }
        return buffer.get() & 0xFF;
    }

    public int read(byte[] bytes, int off, int length)
            throws IOException {
        if (!buffer.hasRemaining()) {
            return -1;
        }

        length = Math.min(length, buffer.remaining());
        buffer.get(bytes, off, length);
        return length;
    }
}
