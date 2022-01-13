
package org.extra.relinker.elf;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class Program32Header extends Elf.ProgramHeader {
    public Program32Header(final ElfParser parser, final Elf.Header header, final long index)
            throws IOException {
        final ByteBuffer buffer = ByteBuffer.allocate(4);
        buffer.order(header.bigEndian ? ByteOrder.BIG_ENDIAN : ByteOrder.LITTLE_ENDIAN);

        final long baseOffset = header.phoff + (index * header.phentsize);
        type = parser.readWord(buffer, baseOffset);
        offset = parser.readWord(buffer, baseOffset + 0x4);
        vaddr = parser.readWord(buffer, baseOffset + 0x8);
        memsz = parser.readWord(buffer, baseOffset + 0x14);
    }
}
