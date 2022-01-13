package org.extra.relinker.elf;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class Program64Header extends Elf.ProgramHeader {
    public Program64Header(final ElfParser parser, final Elf.Header header, final long index)
            throws IOException {
        final ByteBuffer buffer = ByteBuffer.allocate(8);
        buffer.order(header.bigEndian ? ByteOrder.BIG_ENDIAN : ByteOrder.LITTLE_ENDIAN);

        final long baseOffset = header.phoff + (index * header.phentsize);
        type = parser.readWord(buffer, baseOffset);
        offset = parser.readLong(buffer, baseOffset + 0x8);
        vaddr = parser.readLong(buffer, baseOffset + 0x10);
        memsz = parser.readLong(buffer, baseOffset + 0x28);
    }
}
