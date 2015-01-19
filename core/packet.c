/*******************************************************************************
 *
 * Copyright (c) 2013, 2014 Intel Corporation and others.
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
 *    Fabien Fleutot - Please refer to git log
 *    Fabien Fleutot - Please refer to git log
 *    Simon Bernard - Please refer to git log
 *    Toby Jaffey - Please refer to git log
 *
 *******************************************************************************/

/*
 Copyright (c) 2013, 2014 Intel Corporation

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

/*
 Contains code snippets which are:

 * Copyright (c) 2013, Institute for Pervasive Computing, ETH Zurich
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.

 */

#include "internals.h"

#include <stdlib.h>
#include <string.h>

#include <stdio.h>


static void handle_reset(lwm2m_context_t * contextP,
                         void * fromSessionH,
                         coap_packet_t * message)
{
#ifdef LWM2M_CLIENT_MODE
	cancel_observe(contextP, message->mid, fromSessionH);
#endif
}

static coap_status_t handle_request(lwm2m_context_t * contextP, lwm2m_uri_t * uriP, void * fromSessionH,
		coap_packet_t * message, coap_packet_t * response)
{
	coap_status_t result = NOT_FOUND_4_04;

	switch (uriP->flag & LWM2M_URI_MASK_TYPE ) {
#ifdef LWM2M_CLIENT_MODE
	case LWM2M_URI_FLAG_DM :
		// TODO: Authentify server
		result = handle_dm_request(contextP, uriP, fromSessionH, message, response);
		break;

	case LWM2M_URI_FLAG_BOOTSTRAP :
		result = NOT_IMPLEMENTED_5_01;
		break;
#endif

#ifdef LWM2M_SERVER_MODE
		case LWM2M_URI_FLAG_REGISTRATION:
		result = handle_registration_request(contextP, uriP, fromSessionH, message, response);
		break;
#endif
	default:
		result = BAD_REQUEST_4_00;
		break;
	}

	coap_set_status_code(response, result);

	return result;
}

/* This function is an adaptation of function coap_receive() from Erbium's er-coap-13-engine.c.
 * Erbium is Copyright (c) 2013, Institute for Pervasive Computing, ETH Zurich
 * All rights reserved.
 */
void lwm2m_handle_packet(lwm2m_context_t * contextP,
                        uint8_t * buffer,
                        int length,
                        void * fromSessionH)
{
	coap_status_t coap_error_code = NO_ERROR;
	static coap_packet_t message[1];
	static coap_packet_t response[1];

	coap_error_code = coap_parse_message(message, buffer, (uint16_t) length);
	if (coap_error_code == NO_ERROR)
	{
		LOG("  Parsed: ver %u, type %u, tkl %u, code %u, mid %u\r\n", message->version, message->type,
				message->token_len, message->code, message->mid);
		LOG("  Payload: %.*s\r\n\n", message->payload_len, message->payload);

		if (message->code >= COAP_GET && message->code <= COAP_DELETE)
		{
			uint32_t block_num = 0;
			uint16_t block_size = LWM2M_MAX_PAYLOAD_SIZE /* REST_MAX_CHUNK_SIZE*/;
			uint32_t block_offset = 0;
			uint8_t *payload = NULL;
			lwm2m_uri_t * uriP = NULL;
			lwm2m_blockwise_t * blockwiseP = NULL;

			/* prepare response */
			if (message->type == COAP_TYPE_CON)
			{
				/* Reliable CON requests are answered with an ACK. */
				coap_init_message(response, COAP_TYPE_ACK, CONTENT_2_05, message->mid);
            }
            else
			{
				/* Unreliable NON requests are answered with a NON as well. */
				coap_init_message(response, COAP_TYPE_NON, CONTENT_2_05, contextP->nextMID++);
			}

			/* mirror token */
			if (message->token_len)
			{
				coap_set_header_token(response, message->token, message->token_len);
			}

			/* get offset for blockwise transfers */
			if (coap_get_header_block2(message, &block_num, NULL, &block_size, &block_offset))
			{
				LOG("Blockwise: block request %u (%u/%u) @ %u bytes\n", block_num, block_size, REST_MAX_CHUNK_SIZE,
						block_offset);
				block_size = MIN(block_size, REST_MAX_CHUNK_SIZE);
			}
			uriP = lwm2m_decode_uri(message->uri_path);
			if (uriP == NULL)
				coap_error_code = BAD_REQUEST_4_00;
			else
			{
				// check for pending blockwise transfer
				if (!IS_OPTION(message, COAP_OPTION_OBSERVE))
				{
					blockwiseP = blockwise_get(contextP, uriP);
				}
				if (NULL == blockwiseP)
				{
					coap_error_code = handle_request(contextP, uriP, fromSessionH, message, response);
					payload = response->payload;
					if (coap_error_code < BAD_REQUEST_4_00 && block_size < response->payload_len)
					{
						blockwiseP = blockwise_new(contextP, uriP, response, false);
						if (NULL != blockwiseP)
						{
							// hand over payload to blockwise, freed with end of blockwise
							payload = NULL;
						} else
						{
							coap_error_code = INTERNAL_SERVER_ERROR_5_00;
						}
					}
				}
#ifdef LWM2M_CLIENT_MODE
				else if (COAP_204_CHANGED == handle_observe_request(contextP, uriP, fromSessionH, message, response))
				{
					coap_set_status_code(response, COAP_204_CHANGED);
				}
#endif
				if (coap_error_code < BAD_REQUEST_4_00)
				{
					if (NULL != blockwiseP)
					{
						blockwise_prepare(blockwiseP, block_num, block_size, response);
					}

					if ( IS_OPTION(message,
							COAP_OPTION_BLOCK1) && response->code<BAD_REQUEST_4_00 && !IS_OPTION(response, COAP_OPTION_BLOCK1))
					{
						LOG("Block1 NOT IMPLEMENTED\n");

						coap_error_code = NOT_IMPLEMENTED_5_01;
						coap_error_message = "NoBlock1Support";
					}

					coap_error_code = message_send(contextP, response, fromSessionH);
				}
				lwm2m_free(uriP);
				lwm2m_free(payload);
			}
		}
        else
		{
			/* Responses */
			if (message->type == COAP_TYPE_ACK)
			{
				LOG("Received ACK\n");
            }
            else if (message->type==COAP_TYPE_RST)
			{
				LOG("Received RST\n");
				/* Cancel possible subscriptions. */
				handle_reset(contextP, fromSessionH, message);
			}

#ifdef LWM2M_SERVER_MODE
			if (message->code == COAP_204_CHANGED
					&& IS_OPTION(message, COAP_OPTION_OBSERVE))
			{
				handle_observe_notify(contextP, fromSessionH, message);
			}
			else
#endif
			{
				transaction_handle_response(contextP, fromSessionH, message);
			}
		} /* Request or Response */

		coap_free_header(message);

	} /* if (parsed correctly) */
	else
	{
		LOG("Message parsing failed %d\r\n", coap_error_code);
	}

	if (coap_error_code != NO_ERROR)
	{
		LOG("ERROR %u: %s\n", coap_error_code, coap_error_message);

		/* Set to sendable error code. */
		if (coap_error_code >= 192)
		{
			coap_error_code = INTERNAL_SERVER_ERROR_5_00;
		}
		/* Reuse input buffer for error message. */
		coap_init_message(message, COAP_TYPE_ACK, coap_error_code, message->mid);
		coap_set_payload(message, coap_error_message, strlen(coap_error_message));
		message_send(contextP, message, fromSessionH);
	}
}


coap_status_t message_send(lwm2m_context_t * contextP,
                           coap_packet_t * message,
                           void * sessionH)
{
	coap_status_t result = INTERNAL_SERVER_ERROR_5_00;
	uint8_t pktBuffer[COAP_MAX_PACKET_SIZE + 1];
	size_t pktBufferLen = 0;

	pktBufferLen = coap_serialize_message(message, pktBuffer);
	if (0 != pktBufferLen)
	{
		LOG("Send message mid %u, %lu bytes, %u payload\r\n", message->mid, pktBufferLen, message->payload_len);
		result = contextP->bufferSendCallback(sessionH, pktBuffer, pktBufferLen, contextP->userData);
		LOG("Send message result: %d\r\n", result);
	}

	return result;
}

