/*
 * Copyright (C) 2005 - 2011 MaNGOS <http://www.getmangos.org/>
 *
 * Copyright (C) 2008 - 2011 TrinityCore <http://www.trinitycore.org/>
 *
 * Copyright (C) 2011 TrilliumEMU <http://www.arkania.net/>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRILLIUMEMU_WORLDPACKET_H
#define TRILLIUMEMU_WORLDPACKET_H

#include "Common.h"
#include "ByteBuffer.h"

class WorldPacket : public ByteBuffer
{
    public:
        WorldPacket() : ByteBuffer(), m_opcode(UNKNOWN_OPCODE) { }
        WorldPacket(Opcodes opcode, size_t res = 200) : ByteBuffer(res), m_opcode(opcode) { }

        // copy constructor
        WorldPacket(const WorldPacket &packet) : ByteBuffer(packet), m_opcode(packet.m_opcode)
        {
        }

        Opcodes GetOpcode() const { return m_opcode; }
        void SetOpcode(Opcodes opcode) { m_opcode = opcode; }
        void compress(Opcodes opcode);
        void Initialize(Opcodes opcode, size_t size)
        {
            m_opcode = opcode;
            clear(size);
        }

    protected:
        Opcodes m_opcode;
        void _compress(void* dst, uint32 *dst_size, const void* src, int src_size);
};
#endif

