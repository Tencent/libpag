
package org.extra.relinker.elf;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class Section64Header extends Elf.SectionHeader {
    public Section64Header(final ElfParser parser, final Elf.Header header, final int index)
            throws IOException {
        final ByteBuffer buffer = ByteBuffer.allocate(8);
        buffer.order(header.bigEndian ? ByteOrder.BIG_ENDIAN : ByteOrder.LITTLE_ENDIAN);

        info = parser.readWord(buffer, header.shoff + (index * header.shentsize) + 0x2C);
    }
}
