/**
 * Copyright 2015 - 2016 KeepSafe Software, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.extra.relinker.elf;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.nio.ByteOrder;

public class Elf32Header extends Elf.Header {
    private final ElfParser parser;

    public Elf32Header(final boolean bigEndian, final ElfParser parser) throws IOException {
        this.bigEndian = bigEndian;
        this.parser = parser;

        final ByteBuffer buffer = ByteBuffer.allocate(4);
        buffer.order(bigEndian ? ByteOrder.BIG_ENDIAN : ByteOrder.LITTLE_ENDIAN);

        type = parser.readHalf(buffer, 0x10);
        phoff = parser.readWord(buffer, 0x1C);
        shoff = parser.readWord(buffer, 0x20);
        phentsize = parser.readHalf(buffer, 0x2A);
        phnum = parser.readHalf(buffer, 0x2C);
        shentsize = parser.readHalf(buffer, 0x2E);
        shnum = parser.readHalf(buffer, 0x30);
        shstrndx = parser.readHalf(buffer, 0x32);
    }

    @Override
    public Elf.SectionHeader getSectionHeader(final int index) throws IOException {
        return new Section32Header(parser, this, index);
    }

    @Override
    public Elf.ProgramHeader getProgramHeader(final long index) throws IOException {
        return new Program32Header(parser, this, index);
    }

    @Override
    public Elf.DynamicStructure getDynamicStructure(final long baseOffset, final int index)
            throws IOException {
        return new Dynamic32Structure(parser, this, baseOffset, index);
    }
}
