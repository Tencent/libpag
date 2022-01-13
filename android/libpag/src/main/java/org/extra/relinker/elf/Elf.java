
package org.extra.relinker.elf;

import java.io.IOException;

public interface Elf {
    abstract class Header {
        public static final int ELFCLASS32 = 1; // 32 Bit ELF
        public static final int ELFCLASS64 = 2; // 64 Bit ELF
        public static final int ELFDATA2MSB = 2; // Big Endian, 2s complement

        public boolean bigEndian;
        public int type;
        public long phoff;
        public long shoff;
        public int phentsize;
        public int phnum;
        public int shentsize;
        public int shnum;
        public int shstrndx;

        abstract public SectionHeader getSectionHeader(int index) throws IOException;
        abstract public org.extra.relinker.elf.Elf.ProgramHeader getProgramHeader(long index) throws IOException;
        abstract public org.extra.relinker.elf.Elf.DynamicStructure getDynamicStructure(long baseOffset, int index)
                throws IOException;
    }

    abstract class ProgramHeader {
        public static final int PT_LOAD = 1; // Loadable segment
        public static final int PT_DYNAMIC = 2; // Dynamic linking information

        public long type;
        public long offset;
        public long vaddr;
        public long memsz;
    }

    abstract class SectionHeader {
        public long info;
    }

    abstract class DynamicStructure {
        public static final int DT_NULL = 0; // Marks end of structure list
        public static final int DT_NEEDED = 1; // Needed library
        public static final int DT_STRTAB = 5; // String table

        public long tag;
        public long val; // Union with d_ptr
    }
}
