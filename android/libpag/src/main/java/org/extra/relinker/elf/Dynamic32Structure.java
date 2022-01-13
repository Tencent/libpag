
package org.extra.relinker.elf;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class Dynamic32Structure extends Elf.DynamicStructure {
    public Dynamic32Structure(final ElfParser parser, final Elf.Header header,
                              long baseOffset, final int index) throws IOException {
        final ByteBuffer buffer = ByteBuffer.allocate(4);
        buffer.order(header.bigEndian ? ByteOrder.BIG_ENDIAN : ByteOrder.LITTLE_ENDIAN);

        baseOffset = baseOffset + (index * 8);
        tag = parser.readWord(buffer, baseOffset);
        val = parser.readWord(buffer, baseOffset + 0x4);
    }
}
