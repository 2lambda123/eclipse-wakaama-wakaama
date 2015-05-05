/*******************************************************************************
 *
 * Copyright (c) 2015 Intel Corporation and others.
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
 *    domedambrosio - Please refer to git log
 *    Simon Bernard - Please refer to git log
 *    Toby Jaffey - Please refer to git log
 *    Julien Vermillard - Please refer to git log
 *    Bosch Software Innovations GmbH - Please refer to git log
 *
 *******************************************************************************/


#include "liblwm2m.h"

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <inttypes.h>

#include "commandline.h"
#include "connection.h"

/*
 * ensure sync with: er_coap_13.h COAP_MAX_PACKET_SIZE!
 * or internals.h LWM2M_MAX_PACKET_SIZE!
 */
#define MAX_PACKET_SIZE 198

static int g_quit = 0;

static uint8_t prv_buffer_send(void * sessionH,
                               uint8_t * buffer,
                               size_t length,
                               void * userdata)
{
    connection_t * connP = (connection_t*) sessionH;

    if (-1 == connection_send(connP, buffer, length))
    {
        return COAP_500_INTERNAL_SERVER_ERROR;
    }
    return COAP_NO_ERROR;
}

static void prv_quit(char * buffer,
                     void * user_data)
{
    g_quit = 1;
}

void handle_sigint(int signum)
{
    prv_quit(NULL, NULL);
}

void print_usage(char * port)
{
    fprintf(stderr, "Usage: bootstap_server\r\n");
    fprintf(stderr, "Launch a LWM2M Bootstrap Server on localhost port %s.\r\n\n", port);
}


static int prv_bootstrap_callback(void * sessionH,
                                  uint8_t status,
                                  lwm2m_uri_t * uriP,
                                  char * name,
                                  void * userData)
{
    lwm2m_context_t * lwm2mH = (lwm2m_context_t *)userData;
    uint8_t result;

    switch (status)
    {
    case COAP_NO_ERROR:
        // Display
        fprintf(stdout, "\r\nBootstrap request from \"%s\"\r\n", name);

        if (strcmp(name, "testlwm2mclient") != 0)
        {
            fprintf(stdout, "Unknown client.\r\n");
            return COAP_404_NOT_FOUND;
        }
        fprintf(stdout, "Deleting /\r\n");
        // Process
        result = lwm2m_bootstrap_delete(lwm2mH, sessionH, NULL);
        if (result != COAP_NO_ERROR)
        {
            fprintf(stdout, "lwm2m_bootstrap_delete(): ");
            print_status(stdout, status);
            fprintf(stdout, "\r\n");
        }
        return COAP_204_CHANGED;

    default:
        // Display
        fprintf(stdout, "\r\n Received status ");
        print_status(stdout, status);
        fprintf(stdout, " for URI /");
        if (uriP != NULL)
        {
            printf("%hu", uriP->objectId);
            if (LWM2M_URI_IS_SET_INSTANCE(uriP))
            {
                fprintf(stdout, "/%d", uriP->instanceId);
                if (LWM2M_URI_IS_SET_RESOURCE(uriP))
                    fprintf(stdout, "/%d", uriP->resourceId);
            }
        }
        printf("\r\n");
        // Process
        if (status == COAP_202_DELETED)
        {
            lwm2m_uri_t uri;
            uint8_t instTlv[9] = {0xC1, 0x01, 0x14, 0xC4, 0x03, 0xC1, 0xF0, 0x00, 0x00};

            uri.flag = LWM2M_URI_FLAG_OBJECT_ID | LWM2M_URI_FLAG_INSTANCE_ID;
            uri.objectId = 1024;
            uri.instanceId = 1;
            result = lwm2m_bootstrap_write(lwm2mH, sessionH, &uri, instTlv, 9);
            if (result != COAP_NO_ERROR)
            {
                fprintf(stdout, "lwm2m_bootstrap_write(): ");
                print_status(stdout, result);
                fprintf(stdout, "\r\n");
            }
        }
        break;
    }

    return COAP_NO_ERROR;
}


int main(int argc, char *argv[])
{
    int sock;
    fd_set readfds;
    struct timeval tv;
    int result;
    lwm2m_context_t * lwm2mH = NULL;
    int i;
    connection_t * connList = NULL;
    char * port = "5685";

    command_desc_t commands[] =
    {
            {"q", "Quit the server.", NULL, prv_quit, NULL},

            COMMAND_END_LIST
    };

    sock = create_socket(port);
    if (sock < 0)
    {
        fprintf(stderr, "Error opening socket: %d\r\n", errno);
        return -1;
    }

    lwm2mH = lwm2m_init(NULL, prv_buffer_send, NULL);
    if (NULL == lwm2mH)
    {
        fprintf(stderr, "lwm2m_init() failed\r\n");
        return -1;
    }

    signal(SIGINT, handle_sigint);

    for (i = 0 ; commands[i].name != NULL ; i++)
    {
        commands[i].userData = (void *)lwm2mH;
    }
    fprintf(stdout, "> "); fflush(stdout);

    lwm2m_set_bootstrap_callback(lwm2mH, prv_bootstrap_callback, (void *)lwm2mH);

    while (0 == g_quit)
    {
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        FD_SET(STDIN_FILENO, &readfds);

        tv.tv_sec = 60;
        tv.tv_usec = 0;

        result = lwm2m_step(lwm2mH, &(tv.tv_sec));
        if (result != 0)
        {
            fprintf(stderr, "lwm2m_step() failed: 0x%X\r\n", result);
            return -1;
        }

        result = select(FD_SETSIZE, &readfds, 0, 0, &tv);

        if ( result < 0 )
        {
            if (errno != EINTR)
            {
              fprintf(stderr, "Error in select(): %d\r\n", errno);
            }
        }
        else if (result > 0)
        {
            uint8_t buffer[MAX_PACKET_SIZE];
            int numBytes;

            if (FD_ISSET(sock, &readfds))
            {
                struct sockaddr_storage addr;
                socklen_t addrLen;

                addrLen = sizeof(addr);
                numBytes = recvfrom(sock, buffer, MAX_PACKET_SIZE, 0, (struct sockaddr *)&addr, &addrLen);

                if (numBytes == -1)
                {
                    fprintf(stderr, "Error in recvfrom(): %d\r\n", errno);
                }
                else
                {
                    char s[INET6_ADDRSTRLEN];
                    in_port_t port;
                    connection_t * connP;

					s[0] = 0;
                    if (AF_INET == addr.ss_family)
                    {
                        struct sockaddr_in *saddr = (struct sockaddr_in *)&addr;
                        inet_ntop(saddr->sin_family, &saddr->sin_addr, s, INET6_ADDRSTRLEN);
                        port = saddr->sin_port;
                    }
                    else if (AF_INET6 == addr.ss_family)
                    {
                        struct sockaddr_in6 *saddr = (struct sockaddr_in6 *)&addr;
                        inet_ntop(saddr->sin6_family, &saddr->sin6_addr, s, INET6_ADDRSTRLEN);
                        port = saddr->sin6_port;
                    }

                    fprintf(stderr, "%d bytes received from [%s]:%hu\r\n", numBytes, s, ntohs(port));

                    output_buffer(stderr, buffer, numBytes, 0);

                    connP = connection_find(connList, &addr, addrLen);
                    if (connP == NULL)
                    {
                        connP = connection_new_incoming(connList, sock, (struct sockaddr *)&addr, addrLen);
                        if (connP != NULL)
                        {
                            connList = connP;
                        }
                    }
                    if (connP != NULL)
                    {
                        lwm2m_handle_packet(lwm2mH, buffer, numBytes, connP);
                    }
                }
            }
            else if (FD_ISSET(STDIN_FILENO, &readfds))
            {
                numBytes = read(STDIN_FILENO, buffer, MAX_PACKET_SIZE - 1);

                if (numBytes > 1)
                {
                    buffer[numBytes] = 0;
                    handle_command(commands, (char*)buffer);
                }
                if (g_quit == 0)
                {
                    fprintf(stdout, "\r\n> ");
                    fflush(stdout);
                }
                else
                {
                    fprintf(stdout, "\r\n");
                }
            }
        }
    }

    lwm2m_close(lwm2mH);
    close(sock);
    connection_free(connList);

    return 0;
}
