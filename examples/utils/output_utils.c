/*******************************************************************************
 *
 * Copyright (c) 2013, 2014, 2015 Intel Corporation and others.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Eclipse Public License v1.0
 * and Eclipse Distribution License v1.0 which accompany this distribution.
 *
 * The Eclipse Public License is available at
 *    http://www.eclipse.org/legal/epl-v10.html
 * The Eclipse Distribution License is available at
 *    http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * Contributors:
 *    David Navarro, Intel Corporation - initial API and implementation
 *    Fabien Fleutot - Please refer to git log
 *    
 *******************************************************************************/

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <inttypes.h>
#include "output_utils.h"


void print_indent(FILE * stream,
                         int num)
{
    int i;

    for ( i = 0 ; i < num ; i++)
        fprintf(stream, "    ");
}

void output_buffer(FILE * stream,
                   const uint8_t * buffer,
                   int length,
                   int indent)
{
    int i;

    if (length == 0) fprintf(stream, "\n");

    i = 0;
    while (i < length)
    {
        uint8_t array[16];
        int j;

        print_indent(stream, indent);
        memcpy(array, buffer+i, 16);
        for (j = 0 ; j < 16 && i+j < length; j++)
        {
            fprintf(stream, "%02X ", array[j]);
            if (j%4 == 3) fprintf(stream, " ");
        }
        if (length > 16)
        {
            while (j < 16)
            {
                fprintf(stream, "   ");
                if (j%4 == 3) fprintf(stream, " ");
                j++;
            }
        }
        fprintf(stream, " ");
        for (j = 0 ; j < 16 && i+j < length; j++)
        {
            if (isprint(array[j]))
                fprintf(stream, "%c", array[j]);
            else
                fprintf(stream, ".");
        }
        fprintf(stream, "\n");
        i += 16;
    }
}

void output_tlv(FILE * stream,
                const uint8_t * buffer,
                size_t buffer_len,
                int indent)
{
    lwm2m_tlv_type_t type;
    uint16_t id;
    size_t dataIndex;
    size_t dataLen;
    int length = 0;
    int result;

    while (0 != (result = lwm2m_decodeTLV((uint8_t*)buffer + length, buffer_len - length, &type, &id, &dataIndex, &dataLen)))
    {
        print_indent(stream, indent);
        fprintf(stream, "{\r\n");
        print_indent(stream, indent+1);
        fprintf(stream, "ID: %d", id);

        fprintf(stream, " type: ");
        switch (type)
        {
        case LWM2M_TYPE_OBJECT_INSTANCE:
            fprintf(stream, "Object Instance");
            break;
        case LWM2M_TYPE_RESOURCE_INSTANCE:
            fprintf(stream, "Resource Instance");
            break;
        case LWM2M_TYPE_MULTIPLE_RESOURCE:
            fprintf(stream, "Multiple Instances");
            break;
        case LWM2M_TYPE_RESOURCE:
            fprintf(stream, "Resource");
            break;
        default:
            printf("unknown (%d)", (int)type);
            break;
        }
        fprintf(stream, "\n");

        print_indent(stream, indent+1);
        fprintf(stream, "{\n");
        if (type == LWM2M_TYPE_OBJECT_INSTANCE || type == LWM2M_TYPE_MULTIPLE_RESOURCE)
        {
            output_tlv(stream, buffer + length + dataIndex, dataLen, indent+1);
        }
        else
        {
            int64_t intValue;
            double floatValue;

            print_indent(stream, indent+2);
            fprintf(stream, "data (%ld bytes):\r\n", dataLen);
            output_buffer(stream, (uint8_t*)buffer + length + dataIndex, dataLen, indent+2);

            if (0 < lwm2m_opaqueToInt(buffer + length + dataIndex, dataLen, &intValue))
            {
                print_indent(stream, indent+2);
                fprintf(stream, "data as Integer: %" PRId64 "\r\n", intValue);
            }
            if (0 < lwm2m_opaqueToFloat(buffer + length + dataIndex, dataLen, &floatValue))
            {
                print_indent(stream, indent+2);
                fprintf(stream, "data as Float: %.16g\r\n", floatValue);
            }
        }
        print_indent(stream, indent+1);
        fprintf(stream, "}\r\n");
        length += result;
        print_indent(stream, indent);
        fprintf(stream, "}\r\n");
    }
}

void output_data(FILE * stream,
                 lwm2m_media_type_t format,
                 const uint8_t * data,
                 int dataLength,
                 int indent)
{
    int i;

    if (data == NULL) return;

    print_indent(stream, indent);
    fprintf(stream, "%d bytes received of type ", dataLength);
    switch (format)
    {
    case LWM2M_CONTENT_TEXT:
        fprintf(stream, "text/plain:\r\n");
        output_buffer(stream, data, dataLength, indent);
        break;

    case LWM2M_CONTENT_OPAQUE:
        fprintf(stream, "application/octet-stream:\r\n");
        output_buffer(stream, data, dataLength, indent);
        break;

    case LWM2M_CONTENT_TLV:
        fprintf(stream, "application/vnd.oma.lwm2m+tlv:\r\n");
        output_tlv(stream, data, dataLength, indent);
        break;

    case LWM2M_CONTENT_JSON:
        fprintf(stream, "application/vnd.oma.lwm2m+json:\r\n");
        print_indent(stream, indent);
        for (i = 0 ; i < dataLength ; i++)
        {
            fprintf(stream, "%c", data[i]);
        }
        fprintf(stream, "\n");
        break;

    default:
        fprintf(stream, "Unknown (%d):\r\n", format);
        output_buffer(stream, data, dataLength, indent);
        break;
    }
}

void dump_data_t(FILE * stream,
              int size,
              const lwm2m_data_t * dataP,
              int indent)
{
    int i;

    for(i= 0 ; i < size ; i++)
    {
        print_indent(stream, indent);
        fprintf(stream, "{\r\n");
        print_indent(stream, indent+1);
        fprintf(stream, "id: %d\r\n", dataP[i].id);

        print_indent(stream, indent+1);
        fprintf(stream, "type: ");
        switch (dataP[i].type)
        {
        case LWM2M_TYPE_OBJECT:
            fprintf(stream, "LWM2M_TYPE_OBJECT\r\n");
            break;
        case LWM2M_TYPE_OBJECT_INSTANCE:
            fprintf(stream, "LWM2M_TYPE_OBJECT_INSTANCE\r\n");
            break;
        case LWM2M_TYPE_RESOURCE_INSTANCE:
            fprintf(stream, "LWM2M_TYPE_RESOURCE_INSTANCE\r\n");
            break;
        case LWM2M_TYPE_MULTIPLE_RESOURCE:
            fprintf(stream, "LWM2M_TYPE_MULTIPLE_RESOURCE\r\n");
            break;
        case LWM2M_TYPE_RESOURCE:
            fprintf(stream, "LWM2M_TYPE_RESOURCE\r\n");
            break;
        default:
            fprintf(stream, "unknown (%d)\r\n", (int)dataP[i].type);
            break;
        }

        print_indent(stream, indent+1);
        fprintf(stream, "flags: ");
        if (dataP[i].flags & LWM2M_TLV_FLAG_STATIC_DATA)
        {
            fprintf(stream, "STATIC_DATA");
            if (dataP[i].flags & LWM2M_TLV_FLAG_TEXT_FORMAT)
            {
                fprintf(stream, " | TEXT_FORMAT");
            }
        }
        else if (dataP[i].flags & LWM2M_TLV_FLAG_TEXT_FORMAT)
        {
            fprintf(stream, "TEXT_FORMAT");
        }
        fprintf(stream, "\r\n");

        print_indent(stream, indent+1);
        fprintf(stream, "data length: %d\r\n", (int) dataP[i].length);

        if (dataP[i].type == LWM2M_TYPE_OBJECT_INSTANCE
         || dataP[i].type == LWM2M_TYPE_OBJECT
         || dataP[i].type == LWM2M_TYPE_MULTIPLE_RESOURCE)
        {
            dump_data_t(stream, dataP[i].length, (lwm2m_data_t *)(dataP[i].value), indent+1);
        }
        else
        {
            print_indent(stream, indent+1);
            fprintf(stream, "data type: ");
            switch (dataP[i].dataType)
            {
            case LWM2M_TYPE_INTEGER:
                fprintf(stream, "Integer");
                if ((dataP[i].flags & LWM2M_TLV_FLAG_TEXT_FORMAT) == 0)
                {
                    int64_t value;
                    if (1 == lwm2m_data_decode_int(dataP + i, &value))
                    {
                        fprintf(stream, " (%" PRId64 ")", value);
                    }
                }
                break;
            case LWM2M_TYPE_STRING:
                fprintf(stream, "String");
                break;
            case LWM2M_TYPE_FLOAT:
                fprintf(stream, "Float");
                if ((dataP[i].flags & LWM2M_TLV_FLAG_TEXT_FORMAT) == 0)
                {
                    double value;
                    if (1 == lwm2m_data_decode_float(dataP + i, &value))
                    {
                        fprintf(stream, " (%f)", value);
                    }
                }
                break;
            case LWM2M_TYPE_BOOLEAN:
                fprintf(stream, "Boolean");
                if ((dataP[i].flags & LWM2M_TLV_FLAG_TEXT_FORMAT) == 0)
                {
                    bool value;
                    if (1 == lwm2m_data_decode_bool(dataP + i, &value))
                    {
                        fprintf(stream, " (%s)", value?"true":"false");
                    }
                }
                break;
            case LWM2M_TYPE_TIME:
                fprintf(stream, "Time");
                break;
            case LWM2M_TYPE_OBJECT_LINK:
                fprintf(stream, "Object Link");
                break;
            case LWM2M_TYPE_OPAQUE:
                fprintf(stream, "Opaque");
                break;
            case LWM2M_TYPE_UNDEFINED:
                fprintf(stream, "Undefined");
                break;
            }
            fprintf(stream, "\r\n");
            output_buffer(stream, dataP[i].value, dataP[i].length, indent+1);
        }
        print_indent(stream, indent);
        fprintf(stream, "}\r\n");
    }
}

#define CODE_TO_STRING(X)   case X : return #X

static const char* prv_status_to_string(int status)
{
    switch(status)
    {
    CODE_TO_STRING(COAP_NO_ERROR);
    CODE_TO_STRING(COAP_IGNORE);
    CODE_TO_STRING(COAP_201_CREATED);
    CODE_TO_STRING(COAP_202_DELETED);
    CODE_TO_STRING(COAP_204_CHANGED);
    CODE_TO_STRING(COAP_205_CONTENT);
    CODE_TO_STRING(COAP_400_BAD_REQUEST);
    CODE_TO_STRING(COAP_401_UNAUTHORIZED);
    CODE_TO_STRING(COAP_404_NOT_FOUND);
    CODE_TO_STRING(COAP_405_METHOD_NOT_ALLOWED);
    CODE_TO_STRING(COAP_406_NOT_ACCEPTABLE);
    CODE_TO_STRING(COAP_500_INTERNAL_SERVER_ERROR);
    CODE_TO_STRING(COAP_501_NOT_IMPLEMENTED);
    CODE_TO_STRING(COAP_503_SERVICE_UNAVAILABLE);
    default: return "";
    }
}

void print_status(FILE * stream,
                  uint8_t status)
{
    fprintf(stream, "%d.%02d (%s)", (status&0xE0)>>5, status&0x1F, prv_status_to_string(status));
}