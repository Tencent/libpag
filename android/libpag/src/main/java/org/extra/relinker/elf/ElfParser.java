
package org.extra.relinker.elf;

import java.io.Closeable;
import java.io.EOFException;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.nio.channels.FileChannel;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

public class ElfParser implements Closeable, Elf {
    private final int MAGIC = 0x464C457F;
    private final FileChannel channel;

    public ElfParser(final File file) throws FileNotFoundException {
        if (file == null || !file.exists()) {
            throw new IllegalArgumentException("File is null or does not exist");
        }

        final FileInputStream inputStream = new FileInputStream(file);
        this.channel = inputStream.getChannel();
    }

    public Elf.Header parseHeader() throws IOException {
        channel.position(0L);

        // Read in ELF identification to determine file class and endianness
        final ByteBuffer buffer = ByteBuffer.allocate(8);
        buffer.order(ByteOrder.LITTLE_ENDIAN);
        if (readWord(buffer, 0) != MAGIC) {
            throw new IllegalArgumentException("Invalid ELF Magic!");
        }

        final short fileClass = readByte(buffer, 0x4);
        final boolean bigEndian = (readByte(buffer, 0x5) == Header.ELFDATA2MSB);
        if (fileClass == Header.ELFCLASS32) {
            return new Elf32Header(bigEndian, this);
        } else if (fileClass == Header.ELFCLASS64) {
            return new Elf64Header(bigEndian, this);
        }

        throw new IllegalStateException("Invalid class type!");
    }

    public List<String> parseNeededDependencies() throws IOException {
        channel.position(0);
        final List<String> dependencies = new ArrayList<String>();
        final Elf.Header header = parseHeader();
        final ByteBuffer buffer = ByteBuffer.allocate(8);
        buffer.order(header.bigEndian ? ByteOrder.BIG_ENDIAN : ByteOrder.LITTLE_ENDIAN);

        long numProgramHeaderEntries = header.phnum;
        if (numProgramHeaderEntries == 0xFFFF) {
            /**
             * Extended Numbering
             *
             * If the real number of program header table entries is larger than
             * or equal to PN_XNUM(0xffff), it is set to sh_info field of the
             * section header at index 0, and PN_XNUM is set to e_phnum
             * field. Otherwise, the section header at index 0 is zero
             * initialized, if it exists.
             **/
            final Elf.SectionHeader sectionHeader = header.getSectionHeader(0);
            numProgramHeaderEntries = sectionHeader.info;
        }

        long dynamicSectionOff = 0;
        for (long i = 0; i < numProgramHeaderEntries; ++i) {
            final Elf.ProgramHeader programHeader = header.getProgramHeader(i);
            if (programHeader.type == ProgramHeader.PT_DYNAMIC) {
                dynamicSectionOff = programHeader.offset;
                break;
            }
        }

        if (dynamicSectionOff == 0) {
            // No dynamic linking info, nothing to load
            return Collections.unmodifiableList(dependencies);
        }

        int i = 0;
        final List<Long> neededOffsets = new ArrayList<Long>();
        long vStringTableOff = 0;
        Elf.DynamicStructure dynStructure;
        do {
            dynStructure = header.getDynamicStructure(dynamicSectionOff, i);
            if (dynStructure.tag == DynamicStructure.DT_NEEDED) {
                neededOffsets.add(dynStructure.val);
            } else if (dynStructure.tag == DynamicStructure.DT_STRTAB) {
                vStringTableOff = dynStructure.val; // d_ptr union
            }
            ++i;
        } while (dynStructure.tag != DynamicStructure.DT_NULL);

        if (vStringTableOff == 0) {
            throw new IllegalStateException("String table offset not found!");
        }

        // Map to file offset
        final long stringTableOff = offsetFromVma(header, numProgramHeaderEntries, vStringTableOff);
        for (final Long strOff : neededOffsets) {
            dependencies.add(readString(buffer, stringTableOff + strOff));
        }

        return dependencies;
    }

    private long offsetFromVma(final Elf.Header header, final long numEntries, final long vma)
            throws IOException {
        for (long i = 0; i < numEntries; ++i) {
            final Elf.ProgramHeader programHeader = header.getProgramHeader(i);
            if (programHeader.type == ProgramHeader.PT_LOAD) {
                // Within memsz instead of filesz to be more tolerant
                if (programHeader.vaddr <= vma
                        && vma <= programHeader.vaddr + programHeader.memsz) {
                    return vma - programHeader.vaddr + programHeader.offset;
                }
            }
        }

        throw new IllegalStateException("Could not map vma to file offset!");
    }

    @Override
    public void close() throws IOException {
        this.channel.close();
    }

    protected String readString(final ByteBuffer buffer, long offset) throws IOException {
        final StringBuilder builder = new StringBuilder();
        short c;
        while ((c = readByte(buffer, offset++)) != 0) {
            builder.append((char) c);
        }

        return builder.toString();
    }

    protected long readLong(final ByteBuffer buffer, final long offset) throws IOException {
        read(buffer, offset, 8);
        return buffer.getLong();
    }

    protected long readWord(final ByteBuffer buffer, final long offset) throws IOException {
        read(buffer, offset, 4);
        return buffer.getInt() & 0xFFFFFFFFL;
    }

    protected int readHalf(final ByteBuffer buffer, final long offset) throws IOException {
        read(buffer, offset, 2);
        return buffer.getShort() & 0xFFFF;
    }

    protected short readByte(final ByteBuffer buffer, final long offset) throws IOException {
        read(buffer, offset, 1);
        return (short) (buffer.get() & 0xFF);
    }

    protected void read(final ByteBuffer buffer, long offset, final int length) throws IOException {
        buffer.position(0);
        buffer.limit(length);
        long bytesRead = 0;
        while (bytesRead < length) {
            final int read = channel.read(buffer, offset + bytesRead);
            if (read == -1) {
                throw new EOFException();
            }

            bytesRead += read;
        }
        buffer.position(0);
    }
}
