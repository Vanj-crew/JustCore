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

#ifndef _BYTEBUFFER_H
#define _BYTEBUFFER_H

#include "Common.h"
#include "Log.h"
#include "ByteConverter.h"

class ByteBufferException
{
    public:
        ByteBufferException(bool add, size_t pos, size_t elementSize, size_t totalSize)
            : _add(add), _pos(pos), _elementSize(elementSize), _totalSize(totalSize)
        {
            PrintPosError();
        }

        void PrintPosError() const
        {
            sLog->outError("Attempted to %s in ByteBuffer (pos: " SIZEFMTD " size: "SIZEFMTD") value with size: " SIZEFMTD,
                (_add ? "put" : "get"), _pos, _totalSize, _elementSize);
        }
    private:
        bool _add;
        size_t _pos;
        size_t _totalSize;
        size_t _elementSize;
};

class ByteBuffer
{
    public:
        ByteBuffer() : rpos(0), wpos(0), bitpos(8), curbitval(0)
        {
            _newAllocation(NULL, 10, true);
        }

        ByteBuffer(size_t reserved) : rpos(0), wpos(0), bitpos(8), curbitval(0)
        {
            _newAllocation(NULL, reserved, true);
        }

        ByteBuffer(const ByteBuffer &buffer) : rpos(buffer.rpos), wpos(buffer.rpos), bitpos(buffer.bitpos),
            curbitval(buffer.curbitval)
        {
            _newAllocation(NULL, buffer.length, true);
            memcpy(data, buffer.data, length);
        }

        ~ByteBuffer()
        {
            free(data);
        }

        size_t size() const { return length; }
        size_t WritePos() const { return wpos; }
        size_t ReadPos() const { return rpos; }
        void* contents() const { return data; }

        void clear(size_t size = 10)
        {
            rpos = 0;
            wpos = 0;
            bitpos = 8;
            curbitval = 0;
            length = size;
            data = realloc(data, size);
            memset(data, '\0', size);
        }

        bool empty()
        {
            if (wpos > 0)
                return false;
            else
                return true;
        }

        void deAllocateLeftover()
        {
            data = realloc(data, wpos);
            length = wpos;
        }

        template <typename T> void append(T value, size_t size = 0)
        {
            if (!size)
                size = sizeof(value);

            flushBits();
            EndianConvert(value);

            if (length < wpos + size)
                _newAllocation(data, length + size, false);

            memcpy((char*)data + wpos, &value, size);
            wpos += size;
        }

        void append(uint8 *value, size_t len)
        {
            if (length < wpos + len)
                _newAllocation(data, length + len, false);

            memcpy((char*)data + wpos, value, len);
            wpos += len;
        }

        void append(ByteBuffer& buffer)
        {
            buffer.deAllocateLeftover();
            size_t len = buffer.length;

            if (length < wpos + len)
                _newAllocation(data, wpos + len, false);

            memcpy((char*)data + wpos, buffer.contents(), len);
            wpos += len;
        }

        template <typename T> T read()
        {
            T temp;
            size_t size = sizeof(T);

            if (size > length - rpos)
                throw ByteBufferException(false, rpos, size, length);

            memcpy(&temp, (char*)data + rpos, size);
            rpos += size;
            EndianConvert(temp);
            return temp;
        }

        template <typename T> void put(size_t pos, T value)
        {
            EndianConvert(value);
            memcpy((char*)data + pos, &value, sizeof(value)); // Data must be written before using this to replace data
        }

        template <typename T> T read(size_t pos) const
        {
            size_t size = sizeof(T);
            if (pos + size > length)
                throw ByteBufferException(false, pos, sizeof(T), length);

            T val;
            memcpy(&val, (char*)data + pos, size);
            EndianConvert(val);
            return val;
        }

        void read(uint8 *dest, size_t len)
        {
            if (rpos + len > length)
                throw ByteBufferException(false, rpos, len, length);

            memcpy(dest, (char*)data + rpos, len);
            rpos += len;
        }

        void flushBits()
        {
            if (bitpos == 8)
                return;

            bitpos = 8;
            append<uint8>(curbitval);
            curbitval = 0;
        }

        bool writeBit(uint32 bit)
        {
            --bitpos;
            if (bit)
                curbitval |= (1 << (bitpos));

            if (bitpos == 0)
            {
                bitpos = 8;
                append<uint8>(curbitval);
                curbitval = 0;
            }

            return (bit != 0);
        }

        bool readBit()
        {
            ++bitpos;
            if (bitpos > 7)
            {
                bitpos = 0;
                curbitval = read<uint8>();
            }
            bool bit = ((curbitval >> (7 - bitpos)) & 1) != 0;
            return bit;
        }

        template <typename T> void writeBits(T value, size_t bits)
        {
            for (int32 i = bits - 1; i >= 0; --i)
                writeBit((value >> i) & 1);
        }

        uint32 readBits(size_t bits)
        {
            uint32 value = 0;
            for (int32 i = bits - 1; i >= 0; --i)
            {
                if (readBit())
                {
                    value |= (1 << i);
                }
            }
            return value;
        }

        void ReadByteMask(uint8& b)
        {
            b = readBit() ? 1 : 0;
        }

        void ReadByteSeq(uint8& b)
        {
            if (b != 0)
                b ^= read<uint8>();
        }

        void WriteByteMask(uint8 b)
        {
            writeBit(b);
        }

        void WriteByteSeq(uint8 b)
        {
            if (b != 0)
                append<uint8>(b ^ 1);
        }

        ByteBuffer &operator<<(uint8 value)
        {
            append<uint8>(value);
            return *this;
        }

        ByteBuffer &operator<<(uint16 value)
        {
            append<uint16>(value);
            return *this;
        }

        ByteBuffer &operator<<(uint32 value)
        {
            append<uint32>(value);
            return *this;
        }

        ByteBuffer &operator<<(uint64 value)
        {
            append<uint64>(value);
            return *this;
        }

        ByteBuffer &operator<<(int8 value)
        {
            append<int8>(value);
            return *this;
        }

        ByteBuffer &operator<<(int16 value)
        {
            append<int16>(value);
            return *this;
        }

        ByteBuffer &operator<<(int32 value)
        {
            append<int32>(value);
            return *this;
        }

        ByteBuffer &operator<<(int64 value)
        {
            append<int64>(value);
            return *this;
        }

        ByteBuffer &operator<<(float value)
        {
            append<float>(value);
            return *this;
        }

        ByteBuffer &operator<<(double value)
        {
            append<double>(value);
            return *this;
        }

        ByteBuffer &operator<<(const std::string &value)
        {
            size_t elements = value.size() + 1; // null character must be counted too

            if (wpos + elements > length)
                _newAllocation(data, wpos + elements, false);

            memcpy((char*)data + wpos, value.c_str(), elements);
            wpos += elements;
            return *this;
        }

        ByteBuffer &operator<<(const char *str)
        {
            size_t elements = strlen(str) + 1;

            if (wpos + elements > length)
                _newAllocation(data, wpos + elements, false);

            memcpy((char*)data + wpos, str, str ? elements : 0);
            wpos += elements;
            return *this;
        }

        ByteBuffer &operator>>(bool &value)
        {
            value = read<char>() > 0 ? true : false;
            return *this;
        }

        ByteBuffer &operator>>(uint8 &value)
        {
            value = read<uint8>();
            return *this;
        }

        ByteBuffer &operator>>(uint16 &value)
        {
            value = read<uint16>();
            return *this;
        }

        ByteBuffer &operator>>(uint32 &value)
        {
            value = read<uint32>();
            return *this;
        }

        ByteBuffer &operator>>(uint64 &value)
        {
            value = read<uint64>();
            return *this;
        }

        ByteBuffer &operator>>(int8 &value)
        {
            value = read<int8>();
            return *this;
        }

        ByteBuffer &operator>>(int16 &value)
        {
            value = read<int16>();
            return *this;
        }

        ByteBuffer &operator>>(int32 &value)
        {
            value = read<int32>();
            return *this;
        }

        ByteBuffer &operator>>(int64 &value)
        {
            value = read<int64>();
            return *this;
        }

        ByteBuffer &operator>>(float &value)
        {
            value = read<float>();
            return *this;
        }

        ByteBuffer &operator>>(double &value)
        {
            value = read<double>();
            return *this;
        }

        ByteBuffer &operator>>(std::string& value)
        {
            value.clear();
            while (ReadPos() < size())
            {
                char c = read<char>();

                if (c == 0)
                    break;

                value += c;
            }
            return *this;
        }

        uint8 operator[](size_t pos) const
        {
            if (pos > length)
                throw ByteBufferException(false, pos, sizeof(uint8), length);

            uint8 value;
            memcpy(&value, (char*)data + pos, 1);
            EndianConvert(value);
            return value;
        }

        void rfinish() { rpos = wpos; }

        template<typename T> void read_skip() { read_skip(sizeof(T)); }

        void read_skip(size_t skip)
        {
            if(rpos + skip > size())
                throw ByteBufferException(false, rpos, skip, length);

            rpos += skip;
        }

        void readPackGUID(uint64& guid)
        {
            if(rpos + 1 > size())
                throw ByteBufferException(false, rpos, 1, length);

            guid = 0;

            uint8 guidmark = 0;
            (*this) >> guidmark;

            for (int i = 0; i < 8; ++i)
            {
                if (guidmark & (uint8(1) << i))
                {
                    if (rpos + 1 > size())
                        throw ByteBufferException(false, rpos, 1, length);

                    uint8 bit;
                    (*this) >> bit;
                    guid |= (uint64(bit) << (i * 8));
                }
            }
        }

        void appendPackXYZ(float x, float y, float z)
        {
            uint32 packed = 0;
            packed |= ((int)(x / 0.25f) & 0x7FF);
            packed |= ((int)(y / 0.25f) & 0x7FF) << 11;
            packed |= ((int)(z / 0.25f) & 0x3FF) << 22;
            *this << packed;
        }

        void appendPackGUID(uint64 guid)
        {
            uint8 packGUID[8+1];
            packGUID[0] = 0;
            size_t size = 1;
            for (uint8 i = 0;guid != 0;++i)
            {
                if (guid & 0xFF)
                {
                    packGUID[0] |= uint8(1 << i);
                    packGUID[size] =  uint8(guid & 0xFF);
                    ++size;
                }

                guid >>= 8;
            }

            append(&packGUID, size);
            deAllocateLeftover();
        }

        void print_storage() const
        {
            if(!sLog->IsOutDebug()) // optimize disabled debug output
                return;

            sLog->outDebug(LOG_FILTER_NETWORKIO, "STORAGE_SIZE: %lu", (unsigned long)size());

            for (uint32 i = 0; i < size(); ++i)
                sLog->outDebugInLine("%u - ", read<uint8>(i));

            sLog->outDebug(LOG_FILTER_NETWORKIO, " ");
        }

        void textlike() const
        {
            if(!sLog->IsOutDebug()) // optimize disabled debug output
                return;

            sLog->outDebug(LOG_FILTER_NETWORKIO, "STORAGE_SIZE: %lu", (unsigned long)size());

            for (uint32 i = 0; i < size(); ++i)
                sLog->outDebugInLine("%c", read<uint8>(i));

            sLog->outDebug(LOG_FILTER_NETWORKIO, " ");
        }

        void hexlike() const
        {
            if(!sLog->IsOutDebug()) // optimize disabled debug output
                return;

            uint32 j = 1, k = 1;
            sLog->outDebug(LOG_FILTER_NETWORKIO, "STORAGE_SIZE: %lu", (unsigned long)length);

            for (uint32 i = 0; i < length; ++i)
            {
                if ((i == (j * 8)) && ((i != (k * 16))))
                {
                    if (read<uint8>(i) < 0x10)
                        sLog->outDebugInLine("| 0%X ", read<uint8>(i) );

                    else
                        sLog->outDebugInLine("| %X ", read<uint8>(i) );

                    ++j;
                }
                else if (i == (k * 16))
                {
                    if (read<uint8>(i) < 0x10)
                    {
                        sLog->outDebugInLine("\n");
                        sLog->outDebugInLine("0%X ", read<uint8>(i) );
                    }
                    else
                    {
                        sLog->outDebugInLine("\n");
                        sLog->outDebugInLine("%X ", read<uint8>(i) );
                    }

                    ++k;
                    ++j;
                }
                else
                {
                    if (read<uint8>(i) < 0x10)
                        sLog->outDebugInLine("0%X ", read<uint8>(i) );

                    else
                        sLog->outDebugInLine("%X ", read<uint8>(i) );
                }
            }
            sLog->outDebugInLine("\n");
        }

    protected:
        size_t rpos;
        size_t wpos;     // Free index
        size_t bitpos;
        uint8 curbitval;
        void *data;
        size_t length;

    private:
        void _newAllocation(void *ptr, size_t newSize, bool firstAllocation)
        {
            if (firstAllocation)
            {
                if (ptr)
                    free(ptr);

                data = malloc(newSize);
                ASSERT(data);
                length = newSize;
                return;
            }

            if (!ptr || newSize < length)
                return;

            newSize += 10; // Allocate another 10 bytes to decrease reallocation amount
            data = realloc(ptr, newSize);
            length = newSize;
        }
};

#endif
