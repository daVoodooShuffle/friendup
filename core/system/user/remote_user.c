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
 * Remote User
 *
 * All functions related to User structure
 *
 *  @author PS (Pawel Stefanski)
 *  @date created 29/05/2017
 */
#include "remote_user.h"
#include <system/systembase.h>

/**
 * Create new Remote User
 *
 * @return new RemoteUser structure when success, otherwise NULL
 */
RemoteUser *RemoteUserNew( char *name, char *host )
{
	RemoteUser *u;
	if( ( u = FCalloc( 1, sizeof( RemoteUser ) ) ) != NULL )
	{
		u->ru_Name = StringDuplicate( name );
		u->ru_Host = StringDuplicate( host );
	}
	else
	{
		FERROR("Cannot allocate memory for user\n");
	}
	
	return u;
}

/**
 * Remove RemoteUser structure
 *
 * @param usr pointer to RemoteUser structure which will be deleted
 */
void RemoteUserDelete( RemoteUser *usr )
{
	if( usr != NULL )
	{
		RemoteDriveDeleteAll( usr->ru_RemoteDrives );
		
		if( usr->ru_Name != NULL )
		{
			FFree( usr->ru_Name );
		}
		
		if( usr->ru_Password != NULL )
		{
			FFree( usr->ru_Password );
		}
		
		if( usr->ru_Host != NULL )
		{
			FFree( usr->ru_Host );
		}
		
		if( usr->ru_SessionID != NULL )
		{
			FFree( usr->ru_SessionID );
		}
		
		FFree( usr );
	}
}

/**
 * Remove RemoteUser structures connected via node list
 *
 * @param usr pointer to root RemoteUser structure which will be deleted
 * @return 0 when success, otherwise error number
 */
int RemoteUserDeleteAll( RemoteUser *usr )
{
	RemoteUser *rem = usr;
	RemoteUser *next = usr;
	
	while( next != NULL )
	{
		rem = next;
		next = (RemoteUser *)next->node.mln_Succ;
		
		RemoteUserDelete( rem );
	}
	return 0;
}

//
//
//

int RemoteDriveAdd( RemoteUser *ru, char *name )
{
	if( ru != NULL )
	{
		RemoteDrive *rd = FCalloc( 1, sizeof(RemoteDrive) );
		if( rd != NULL )
		{
			rd->rd_Name = StringDuplicate( name );
			
			rd->node.mln_Succ = (MinNode *) ru->ru_RemoteDrives;
			ru->ru_RemoteDrives = rd;
		}
	}
	return 0;
}

//
//
//

int RemoteDriveRemove( RemoteUser *ru, char *name )
{
	if( ru != NULL )
	{
		RemoteDrive *ldrive = ru->ru_RemoteDrives;
		RemoteDrive *prevdrive = ldrive;
		
		while( ldrive != NULL )
		{
			if( ldrive->rd_Name != NULL && strcmp( ldrive->rd_Name, name ) == 0 )
			{
				DEBUG("RemoteDriveDelete: \n");
				break;
			}
			prevdrive = ldrive;
			ldrive = (RemoteDrive *)ldrive->node.mln_Succ;
		}
		
		if( ldrive != NULL )
		{
			prevdrive->node.mln_Succ = ldrive->node.mln_Succ;
			DEBUG("RemoteDriveDelete drive will be removed from memory\n");
			//UserDelete( ldrive );
			
			RemoteDriveDelete( ldrive );
			
			return 0;
		}
	}
	return 0;
}

//
//
//

RemoteDrive *RemoteDriveNew( char *localName, char *remoteName )
{
	RemoteDrive *rd = NULL;
	
	if( ( rd = FCalloc( 1, sizeof(RemoteDrive) ) ) != NULL )
	{
		rd->rd_LocalName = StringDuplicate( localName );
		rd->rd_RemoteName = StringDuplicate( remoteName );
	}
	
	return rd;
}

//
//
//

void RemoteDriveDelete( RemoteDrive *rd )
{
	if( rd != NULL )
	{
		if( rd->rd_LocalName != NULL )
		{
			FFree( rd->rd_LocalName );
		}
		
		if( rd->rd_RemoteName != NULL )
		{
			FFree( rd->rd_RemoteName );
		}
		
		if( rd->rd_Name != NULL )
		{
			FFree( rd->rd_Name );
		}
		
		FFree( rd );
	}
}

//
//
//

void RemoteDriveDeleteAll( RemoteDrive *rd )
{
	RemoteDrive *next = rd;
	while( next != NULL )
	{
		rd = next;
		next = (RemoteDrive *)next->node.mln_Succ;
		
		if( rd->rd_LocalName != NULL )
		{
			FFree( rd->rd_LocalName );
		}
		
		if( rd->rd_RemoteName != NULL )
		{
			FFree( rd->rd_RemoteName );
		}
		
		if( rd->rd_Name != NULL )
		{
			FFree( rd->rd_Name );
		}
		
		FFree( rd );
	}
}