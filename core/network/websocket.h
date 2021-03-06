/*©mit**************************************************************************
*                                                                              *
* This file is part of FRIEND UNIFYING PLATFORM.                               *
* Copyright 2014-2017 Friend Software Labs AS                                  *
*                                                                              *
* Permission is hereby granted, free of charge, to any person obtaining a copy *
* of this software and associated documentation files (the "Software"), to     *
* deal in the Software without restriction, including without limitation the   *
* rights to use, copy, modify, merge, publish, distribute, sublicense, and/or  *
* sell copies of the Software, and to permit persons to whom the Software is   *
* furnished to do so, subject to the following conditions:                     *
*                                                                              *
* The above copyright notice and this permission notice shall be included in   *
* all copies or substantial portions of the Software.                          *
*                                                                              *
* This program is distributed in the hope that it will be useful,              *
* but WITHOUT ANY WARRANTY; without even the implied warranty of               *
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                 *
* MIT License for more details.                                                *
*                                                                              *
*****************************************************************************©*/

/** @file
 * 
 *  Websocket structure
 *
 *  @author PS (Pawel Stefanski)
 *  @date created 2016
 */

#ifndef __NETWORK_WEBSOCKET_H__
#define __NETWORK_WEBSOCKET_H__

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/time.h>
#include <poll.h>
#include <core/types.h>

#include <libwebsockets.h>
#include <core/thread.h>
#include <time.h>

#define MAX_MESSAGE_QUEUE 64

#define MAX_POLL_ELEMENTS 256

//
// main WebSocket structure
//

typedef struct WebSocket
{
	char                                            *ws_CertPath;
	char                                            *ws_KeyPath;
	int                                             ws_Port;
	FBOOL                                           ws_UseSSL;
	FBOOL                                           ws_AllowNonSSL;

	struct lws_context                              *ws_Context;
	char                                            ws_InterfaceName[128];
	char                                            *ws_Interface;
	struct lws_context_creation_info                ws_Info;
	int                                             ws_DebugLevel;
	int                                             ws_OldTime;
	int                                             ws_Opts;
	
	unsigned char                                   ws_Buf[LWS_SEND_BUFFER_PRE_PADDING + 1024 + LWS_SEND_BUFFER_POST_PADDING];
						  
	// connection epoll
	struct lws_pollfd                               ws_Pollfds[ MAX_POLL_ELEMENTS ];
	int                                             ws_CountPollfds;
	
	FThread                                         *ws_Thread;
	
	FBOOL                                           ws_Quit;
	void                                            *ws_FCM;
} WebSocket;


//
// FriendCoreWebsocketData structure
//

typedef struct FCWSData 
{
	//int fcd_Number;
	void								*fcd_ActiveSession;
	void								*fcd_WSClient;
	void								*fcd_SystemBase;
	
	struct timeval				fcd_Timer;
}FCWSData;

//
//
//

WebSocket *WebSocketNew( void *sb,  int port, FBOOL useSSL );

//
//
//

void WebSocketDelete( WebSocket *ws );

//
//
//

int WebSocketStart( WebSocket *ws );

#endif // __NETWORK_WEBSOCKET_H__


