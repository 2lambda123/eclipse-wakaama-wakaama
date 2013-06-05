/*
Copyright (c) 2013, Intel Corporation

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    * Neither the name of Intel Corporation nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
THE POSSIBILITY OF SUCH DAMAGE.

David Navarro <david.navarro@intel.com>

*/

#include "internals.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define _PRV_64BIT_BUFFER_SIZE 8


static int prv_create_header(uint8_t * header,
                             lwm2m_tlv_type_t type,
                             uint16_t id,
                             size_t data_len)
{
    int header_len;

    header[0] = 0;
    switch (type)
    {
    case TLV_OBJECT_INSTANCE:
        header_len = 3;
        header[0] |= 0x30;
        header[1] = (id >> 8) & 0XFF;
        header[2] = id & 0XFF;
        break;
    case TLV_RESSOURCE_INSTANCE:
        header_len = 2;
        header[0] |= 0x40;
        header[1] = id & 0XFF;
        break;
    case TLV_MULTIPLE_INSTANCE:
        header_len = 3;
        header[0] |= 0x90;
        header[1] = (id >> 8) & 0XFF;
        header[2] = id & 0XFF;
        break;
    case TLV_RESSOURCE:
        header_len = 3;
        header[0] |= 0xB0;
        header[1] = (id >> 8) & 0XFF;
        header[2] = id & 0XFF;
        break;
    default:
        return 0;
    }
    if (data_len <= 7)
    {
        header[0] += data_len;
    }
    else if (data_len <= 0xFFFF)
    {
        header[0] |= 0x08;
        header[header_len] = (data_len >> 8) & 0XFF;
        header[header_len+1] = data_len & 0XFF;
        header_len += 2;
    }
    else if (data_len <= 0xFFFFFF)
    {
        header[0] |= 0x18;
        header[header_len] = (data_len >> 16) & 0XFF;
        header[header_len+1] = (data_len >> 8) & 0XFF;
        header[header_len+2] = data_len & 0XFF;
        header_len += 2;
    }

    return header_len;
}

int lwm2m_opaqueToTLV(lwm2m_tlv_type_t type,
                      uint8_t* dataP,
                      size_t data_len,
                      uint16_t id,
                      char * buffer,
                      size_t buffer_len)
{
    uint8_t header[LWM2M_TLV_HEADER_MAX_LENGTH];
    size_t header_len;

    header_len = prv_create_header(header, type, id, data_len);

    if (buffer_len < data_len + header_len) return 0;

    memmove(buffer, header, header_len);

    memmove(buffer + header_len, dataP, data_len);

    return header_len + data_len;
}

int lwm2m_boolToTLV(lwm2m_tlv_type_t type,
                    bool value,
                    uint16_t id,
                    char * buffer,
                    size_t buffer_len)
{
    return lwm2m_intToTLV(type, value?1:0, id, buffer, buffer_len);
}

int lwm2m_intToTLV(lwm2m_tlv_type_t type,
                   int64_t data,
                   uint16_t id,
                   char * buffer,
                   size_t buffer_len)
{
    uint8_t data_buffer[_PRV_64BIT_BUFFER_SIZE];
    size_t length = 0;
    uint64_t value;
    int negative = 0;

    if (type != TLV_RESSOURCE_INSTANCE && type != TLV_RESSOURCE)
        return 0;

    memset(data_buffer, 0, 8);

    if (data < 0)
    {
        negative = 1;
        value = 0 - data;
    }
    else
    {
        value = data;
    }

    while (value > ((1 << (7 * length)) - 1))
    {
        data_buffer[_PRV_64BIT_BUFFER_SIZE - length] = (value >> (8*length)) & 0xFF;
        length++;
    }

    if (1 == negative)
    {
        data_buffer[_PRV_64BIT_BUFFER_SIZE - length] |= 0x80;
    }

    return lwm2m_opaqueToTLV(type, data_buffer, length, id, buffer, buffer_len);
}