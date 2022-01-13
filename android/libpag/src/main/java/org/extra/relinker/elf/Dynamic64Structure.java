
package org.extra.relinker.elf;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class Dynamic64Structure extends Elf.DynamicStructure {
    public Dynamic64Structure(final ElfParser parser, final Elf.Header header,
                              long baseOffset, final int index) throws IOException {
        final ByteBuffer buffer = ByteBuffer.allocate(8);
        buffer.order(header.bigEndian ? ByteOrder.BIG_ENDIAN : ByteOrder.LITTLE_ENDIAN);

        baseOffset = baseOffset + (index * 16);
        tag = parser.readLong(buffer, baseOffset);
        val = parser.readLong(buffer, baseOffset + 0x8);
    }
}
