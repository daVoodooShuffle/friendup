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


/*

	FCDB auth module code

*/

#include <core/types.h>
#include <core/library.h>
#include <util/log/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>
#include <util/string.h>
#include <openssl/sha.h>
#include <string.h>
#include <propertieslibrary.h>
#include <user/user.h>
#include <util/sha256.h>
#include <network/websocket_client.h>

#include <system/user/user_sessionmanager.h>
#include <system/systembase.h>
#include <system/user/user_manager.h>
#include <system/user/user_sessionmanager.h>

#define LIB_NAME "fcdb.authmod"
#define LIB_VERSION			1
#define LIB_REVISION		0

//
// init library
//


typedef struct SpecialData
{
	int test;
}SpecialData;

//
//
//

int libInit( AuthMod *l, void *sb )
{
	DEBUG("FCDB libinit\n");

	if( ( l->SpecialData = FCalloc( 1, sizeof( struct SpecialData ) ) ) == NULL )
	{
		FERROR("Cannot allocate memory for authmodule\n");
		return 1;
	}

	l->am_Name = LIB_NAME;
	l->am_Version = LIB_VERSION;
	l->sb = sb;

	return 0;
}

//
//
//

void libClose( struct AuthMod *l )
{
	if( l->SpecialData != NULL )
	{
		FFree( l->SpecialData );
	}
	
	DEBUG("FCDB close\n");
}

//
//
//

long GetVersion(void)
{
	return LIB_VERSION;
}

long GetRevision(void)
{
	return LIB_REVISION;
}

#define FUP_AUTHERR_PASSWORD	1
#define FUP_AUTHERR_TIMEOUT		2
#define FUP_AUTHERR_UPDATE		3
#define FUP_AUTHERR_USRNOTFOUND	4
#define FUP_AUTHERR_WRONGSESID	5
#define FUP_AUTHERR_USER		6
#define RANDOM_WAITING_TIME 20

//
// check password
//

FBOOL CheckPassword( struct AuthMod *l, Http *r, User *usr, char *pass, FULONG *blockTime )
{
	if( usr == NULL )
	{
		FERROR("Cannot check password for usr = NULL\n");
		return FALSE;
	}
	
	//
	// checking if last user login attempts failed, if yes, then we cannot continue
	//
	*blockTime = 0;
	
	SystemBase *sb = (SystemBase *)l->sb;
	{
		time_t tm;
		time_t tm_now = time( NULL );
		FBOOL access = sb->sl_UserManagerInterface.UMGetLoginPossibilityLastLogins( sb->sl_UM, usr->u_Name, l->am_BlockAccountAttempts, &tm );
		
		DEBUG("Authentication, access: %d, time difference between last login attempt and now %lu\n", (int)access, (unsigned long)( tm_now - tm ) );
		// if last 3 access failed you must wait one hour from last login attempt
		if( access == FALSE && ( (tm_now - tm ) < l->am_BlockAccountTimeout) )
		{
			int max = rand( )%RANDOM_WAITING_TIME;
			
			DEBUG("User not found, generate random loop, seconds: %d\n", max );
			// This "hack" will give login machine feeling that FC is doing something in DB
			
			//sleep( max );
			
			FERROR("User: %s was trying to login 3 times in a row (fail login attempts)\n", usr->u_Name );
			sb->sl_UserManagerInterface.UMStoreLoginAttempt( sb->sl_UM, usr->u_Name, "Login fail", "Last login attempts fail" );
			
			*blockTime = (FULONG) (tm_now + l->am_BlockAccountTimeout);
			
			return NULL;
		}
	}
	
	if( usr->u_Password[ 0 ] == '{' &&
		usr->u_Password[ 1 ] == 'S' &&
		usr->u_Password[ 2 ] == '6' &&
		usr->u_Password[ 3 ] == '}' )
	{
		if( pass != NULL )
		{
			FBOOL passS6 = FALSE;
			
			if( pass[ 0 ] == '{' &&
				pass[ 1 ] == 'S' &&
				pass[ 2 ] == '6' &&
				pass[ 3 ] == '}' )
			{
				passS6 = TRUE;
			}
			
			FCSHA256_CTX ctx;
			unsigned char hash[ 32 ];
			char hashTarget[ 64 ];
		
			if( passS6 == TRUE )
			{
				if( strcmp( usr->u_Password, pass ) == 0 )
				{
					DEBUG("Password is ok! Both are hashed\n");
					return TRUE;
				}
			}
			else
			{
				DEBUG("Checkpassword, password is in SHA256 format for user %s\n", usr->u_Name );
		
				Sha256Init( &ctx );
				Sha256Update( &ctx, (unsigned char *) pass, (unsigned int)strlen( pass ) ); //&(usr->u_Password[4]), strlen( usr->u_Password )-4 );
				Sha256Final( &ctx, hash );
		
				int i;
				int j=0;
		
				//for( i=0 ; i < 32 ; i++ )
				//{
					//printf( "%d - %02x\n", (char)(hash[ i ]), (char) hash[ i ] & 0xff );
				//}
		
				for( i = 0 ; i < 64 ; i += 2, j++ )
				{
					sprintf( &(hashTarget[ i ]), "%02x", (char )hash[ j ] & 0xff );
				}
		
				DEBUG("Checking provided password '%s' versus active password '%s'\n", hashTarget, usr->u_Password );
		
				if( strncmp( &(hashTarget[0]), &(usr->u_Password[4]), 64 ) == 0 )
				{
					DEBUG("Password is ok!\n");
					return TRUE;
				}
			}
		}
	}
	else
	{
		if( strcmp( usr->u_Password, pass ) == 0 )
		{
			return TRUE;
		}
	}
	return FALSE;
}

//
// update password
//

int UpdatePassword( struct AuthMod *l, Http *r, User *usr, char *pass )
{
	if( l != NULL && usr != NULL )
	{
		if( usr->u_Password != NULL )
		{
			FFree( usr->u_Password );
		}
		usr->u_Password = StringDuplicate( pass );
		
		SystemBase *sb = (SystemBase *)l->sb;
		MYSQLLibrary *sqlLib = sb->LibraryMYSQLGet( sb );
		if( sqlLib != NULL )
		{
			char temptext[ 2048 ];
			
			sqlLib->SNPrintF( sqlLib, temptext, 2048, "UPDATE `FUser` f SET f.Password = '%s' WHERE`ID` = '%ld'",  pass, usr->u_ID );
			//snprintf( temptext, 2048, "UPDATE `FUser` f SET f.Password = '%s' WHERE`ID` = '%ld'", pass, usr->u_ID );
			
			MYSQL_RES *res = sqlLib->Query( sqlLib, temptext );
			if( res != NULL )
			{
				sqlLib->FreeResult( sqlLib, res );
			}
						
			sb->LibraryMYSQLDrop( sb, sqlLib );
		}
	}
	
	return 0;
}

//
//
//

int isApiUser( MYSQLLibrary *sqlLib, User *tmpusr )
{
	// If we are not an API user:
	// TODO: Just find the group on the user in memory when it syncs with the Users client application
	
	char sql[ 512 ];
	
	sqlLib->SNPrintF( sqlLib, sql, sizeof(sql), "SELECT u.ID FROM FUser u, FUserToGroup ug, FUserGroup g \
		WHERE u.ID = \'%ld\' AND u.ID = ug.UserID AND g.ID = ug.UserGroupID AND g.Name = \'API\'", tmpusr->u_ID );
	/*
	snprintf( sql, sizeof(sql), "\
		SELECT u.ID FROM FUser u, FUserToGroup ug, FUserGroup g \
		WHERE \
		u.ID = \'%ld\' AND u.ID = ug.UserID AND \
		g.ID = ug.UserGroupID AND g.Name = \'API\'\
	", tmpusr->u_ID );
	*/
	DEBUG( "AUTHENTICATE: Trying to see if user %s UserID  (%d) is apiusr \n", tmpusr->u_Name,  (int)tmpusr->u_ID );
	
	//
	// this code must be removed on the end
	// its here to help with fast rollback
	//
	/*
	int error = mysql_query( sqlLib->con.sql_Con, sql );
	
	DEBUG( "AUTHENTICATE: Trying to see if user %s is an API user %d  UserID  (%d)\n", tmpusr->u_Name, error, (int)tmpusr->u_ID );
	
	if( error == 0 ) 
	{
		// Store and clear, because it makes much sense!
		MYSQL_RES *res = mysql_store_result( sqlLib->con.sql_Con );
		if( res )
		{
			// Check if it was a real result
			if( (int)mysql_num_rows( res ) <= 0 )
			{
				error = -1;
			}
			mysql_free_result( res );
		}
		// No results!
		else
		{
			error = -1;
		}
	}
	*/
	int error = 0;
	
	if( sqlLib == NULL )
	{
		FERROR("Sqllibrary is equal to null\n");
		return -1;
	}
	
	MYSQL_RES *res = sqlLib->Query( sqlLib, sql );
	if( res != NULL )
	{
		if( sqlLib->NumberOfRows( sqlLib, res ) <= 0 )
		{
			error = -1;
		}
		sqlLib->FreeResult( sqlLib, res );
	}
	else
	{
		error = -1;
	}
	
	return error;
}

//
// authenticate user
//

UserSession *Authenticate( struct AuthMod *l, Http *r, struct UserSession *logsess, const char *name, const char *pass, const char *devname, const char *sessionId, FULONG *blockTime )
{
	if( l == NULL )
	{
		return NULL;
	}
	DEBUG("[FCDB] Authenticate START (%s)\n", name );
	
	//struct User *user = NULL;
	UserSession *uses = NULL;
	User *tmpusr = NULL;
	SystemBase *sb = (SystemBase *)l->sb;
	FBOOL userFromDB = FALSE;
	FULONG userid = 0;
	*blockTime = 0;
	
	//
	// checking if last user login attempts failed, if yes, then we cannot continue
	//
	
	{
		time_t tm = 0;
		time_t tm_now = time( NULL );
		FBOOL access = sb->sl_UserManagerInterface.UMGetLoginPossibilityLastLogins( sb->sl_UM, name, l->am_BlockAccountAttempts, &tm );
		
		DEBUG("Authentication, access: %d, time difference between last login attempt and now %lu\n", access, ( tm_now - tm ) );
		// if last 3 access failed you must wait one hour from last login attempt
		if( access == FALSE && ( (tm_now - tm ) < l->am_BlockAccountTimeout) )
		{
			int max = rand( )%RANDOM_WAITING_TIME;
			
			DEBUG("User not found, generate random loop, seconds: %d\n", max );
			// This "hack" will give login machine feeling that FC is doing something in DB
			
			//sleep( max );
			
			FERROR("User: %s was trying to login 3 times in a row (fail login attempts)\n", name );
			sb->sl_UserManagerInterface.UMStoreLoginAttempt( sb->sl_UM, name, "Login fail", "Last login attempts fail" );
			
			*blockTime = (FULONG) (tm_now + l->am_BlockAccountTimeout);
			
			return NULL;
		}
	}
	
	//
	// when loguser is not null , it says that session is in memory, we should use it
	//
	
	if( logsess == NULL  )
	{
		DEBUG("[FCDB] Usersession not provided, will be taken from DB\n");

		tmpusr = sb->sl_UserManagerInterface.UMUserGetByNameDB( sb->sl_UM, name );
		userFromDB = TRUE;
		
		if( tmpusr != NULL )
		{
			uses = USMGetSessionByDeviceIDandUser( sb->sl_USM, devname, tmpusr->u_ID );
		}
		else
		{
			int max = rand( )%RANDOM_WAITING_TIME;
			
			DEBUG("User not found, generate random loop, seconds: %d\n", max );
			// This "hack" will give login machine feeling that FC is doing something in DB

			//sleep( max );
			
			goto loginfail;
		}
	}
	else
	{
		if( logsess->us_User == NULL )
		{
			DEBUG("[FCDB] User is not connected to session, I will load it from DB\n");
			tmpusr = sb->sl_UserManagerInterface.UMUserGetByNameDB( sb->sl_UM, name );
			userFromDB = TRUE;
		}
		DEBUG("[FCDB] User taken from provided session\n");
		tmpusr = logsess->us_User;
		uses = logsess;
	}
	
	DEBUG("[FCDB] Authenticate getting timestamp\n");
	time_t timestamp = time ( NULL );
	
	if( tmpusr == NULL )
	{
		DEBUG("[ERROR] Cannot find user by name %s\n", name );
		goto loginfail;
	}
	
	userid = tmpusr->u_ID;
	
	DEBUG("[FCDB] User found ID %lu\n", tmpusr->u_ID );
	
	//
	// if sessionid is provided
	// we are trying to find it and update
	//
	
	if( sessionId != NULL )
	{
		DEBUG("[FCDB] Sessionid != NULL\n");
		if( logsess == NULL )
		{
			FERROR("[FCDB] provided Sessionid is empty\n");
			//UserDelete( tmpusr );
			//tmpusr = NULL;
		}
		
		//
		// use remote session
		//
		
		if( strcmp( sessionId, "remote" ) == 0 )
		{
			DEBUG("[FCDB] remote connection found\n");
			if( l->CheckPassword( l, r, tmpusr, (char *)pass, blockTime ) == FALSE )
			{
				//tmpusr->u_Error = FUP_AUTHERR_PASSWORD;
				if(  tmpusr != NULL && userFromDB == TRUE ){ UserDelete( tmpusr );	tmpusr =  NULL; }
				goto loginfail;
			}
			
			UserSessList *usl = tmpusr->u_SessionsList;
			while( usl != NULL )
			{
				UserSession *s = (UserSession *)usl->us;
				if( strcmp( s->us_SessionID, "remote" ) == 0 )
				{
					break;
				}
				usl = (UserSessList *) usl->node.mln_Succ;
			}
			
			if( usl == NULL )
			{
				if(  tmpusr != NULL && userFromDB == TRUE ){ UserDelete( tmpusr );	tmpusr =  NULL; }
				UserSession *ses = UserSessionNew( "remote", "remote" );
				if( ses != NULL )
				{
					ses->us_UserID = tmpusr->u_ID;
					ses->us_LoggedTime = timestamp;
				}
				DEBUG("[FCDB] remotefs returning session ptr %p\n", ses );

				uses = ses;
				goto loginok;
			}
			
			if(  tmpusr != NULL && userFromDB == TRUE ){ UserDelete( tmpusr );	tmpusr =  NULL; }
			uses = usl->us;
			goto loginok;
		}
		
		//
		// getting user session from memory via sessionid
		//
		
		DEBUG( "[FCDB] Provided sessionid to function %s\n", sessionId );
		uses = sb->sl_UserSessionManagerInterface.USMGetSessionBySessionID( sb->sl_USM, sessionId );
		
		if( uses != NULL )
		{
			uses->us_LoggedTime = timestamp;
			DEBUG("[FCDB] Check password %s  -  %s\n", pass, uses->us_User->u_Password );
			
			if( l->CheckPassword( l, r, uses->us_User, (char *)pass, blockTime ) == FALSE )
			{
				//uses->us_User->u_Error = FUP_AUTHERR_PASSWORD;
				if(  tmpusr != NULL && userFromDB == TRUE ){ UserDelete( tmpusr );	tmpusr =  NULL; }
				goto loginfail;
			}
		
			if( strcmp( name, uses->us_User->u_Name ) != 0 )
			{
				//uses->us_User->u_Error = FUP_AUTHERR_USER;
				if(  tmpusr != NULL && userFromDB == TRUE ){ UserDelete( tmpusr );	tmpusr =  NULL; }
				goto loginfail;
			}
		
			//
			// session is valid
			//
		
		//DEBUG("\n\n\n============================================================ tmiest %%lld usertime %lld logouttst %lld\n\n
		//==========================================================================\n");
			// TODO: reenable timeout when it works!
			if( 1 == 1 || ( timestamp - uses->us_LoggedTime ) < LOGOUT_TIME )
			{	// session timeout
	
				DEBUG("[FCDB] checking login time %ld < LOGOUTTIME %d\n", timestamp - uses->us_LoggedTime, LOGOUT_TIME );
	
				// same session, update login time
				
				if(  tmpusr != NULL && userFromDB == TRUE ){ UserDelete( tmpusr );	tmpusr =  NULL; }
				goto loginok;
			}
			else
				
				//
				// session not found in memory
				//
				
			{
				//user->u_Error = FUP_AUTHERR_TIMEOUT;
				FERROR("[ERROR] FUP AUTHERR_TIMEOUT\n");
				if(  tmpusr != NULL && userFromDB == TRUE ){ UserDelete( tmpusr );	tmpusr =  NULL; }
				goto loginok;
			}
		}
		else
		{
			DEBUG("[ERROR] User cannot be found by session %s\n", sessionId );
			if(  tmpusr != NULL && userFromDB == TRUE ){ UserDelete( tmpusr );	tmpusr =  NULL; }
			goto loginfail;
		}
	}
	
	//
	// provided sessionid is null
	//
	
	else
	{
		
		DEBUG("[FCDB] AUTHENTICATE sessionid was not provided\n");
		
		if( l->CheckPassword( l, r, tmpusr, (char *)pass, blockTime ) == FALSE )
		{
			DEBUG( "[FCDB] Password does not match! %s (provided password) != %s (user from session or db)\n", pass, tmpusr->u_Password );
			// if user is in global list, it means that that it didnt come  from outside
			//user->u_Error = FUP_AUTHERR_PASSWORD;
			if( sb->sl_UserManagerInterface.UMUserCheckExistsInMemory( sb->sl_UM, tmpusr ) == NULL )
			{
				DEBUG( "[FCDB] User used for tests is not in list. Safely remove it.\n" );
			}
			if(  tmpusr != NULL && userFromDB == TRUE ){ UserDelete( tmpusr );	tmpusr =  NULL; }
			goto loginfail;
		}
		
		//
		// if session is not provided we must create one
		//
		
		if( uses == NULL )
		{
			DEBUG("[FCDB] Create new session\n");
			//USMGetSessionByDeviceIDandUser()
			uses = UserSessionNew( NULL, (const char *)devname );
			uses->us_UserID = tmpusr->u_ID;
		}
		else
		{
			DEBUG("[FCDB] Found old session, using it %s\n", uses->us_SessionID );
			if( uses->us_User->u_MainSessionID != NULL )
			{
			}
		}
		
		//USMSessionSaveDB( sb->sl_USM, uses );
		
		DEBUG( "[FCDB] The password comparison is: %s, %s\n", pass, tmpusr->u_Password );
		
		SystemBase *sb = (SystemBase *)l->sb;
		
		DEBUG("[FCDB] SB %p\n", sb );
		MYSQLLibrary *sqlLib = sb->LibraryMYSQLGet( sb );
		if( sqlLib != NULL )
		{
			int testAPIUser = isApiUser( sqlLib, tmpusr );
			
			if( uses->us_SessionID == NULL || testAPIUser != 0 )
			{
				DEBUG( "[FCDB] : We got a response on: \nAUTHENTICATE: SessionID = %s\n", uses->us_SessionID ? uses->us_SessionID : "No session id" );
				DEBUG("\n\n\n1============================================================ tmiest %lld usertime %lld logouttst %lld\n\
		==========================================================================\n", timestamp, uses->us_LoggedTime , LOGOUT_TIME);
				
				//char tmpQuery[ 512 ];
				//
				// user was not logged out
				//
				if(  (timestamp - uses->us_LoggedTime) < LOGOUT_TIME )
				{
					DEBUG("User was not logged out\n");
					
				}
				else		// user session is no longer active
				{
					// Create new session hash
					// for user  session
					
					DEBUG("[FCDB] Generate new sessionid\n");
					char *hashBase = MakeString( 255 );
					sprintf( hashBase, "%ld%s%d", timestamp, tmpusr->u_FullName, ( rand() % 999 ) + ( rand() % 999 ) + ( rand() % 999 ) );
					HashedString( &hashBase );
				
					// Remove old one and update
					if( uses->us_SessionID )
					{
						FFree( uses->us_SessionID );
					}
					uses->us_SessionID = hashBase;
				}

				DEBUG("[FCDB] Update filesystems\n");
				uses->us_LoggedTime = timestamp;

				sb->LibraryMYSQLDrop( sb, sqlLib );
				// TODO: Generate sessionid the first time if it is empty!!
				INFO("Auth return user %s with sessionid %s\n", tmpusr->u_Name,  uses->us_SessionID );
				if(  tmpusr != NULL && userFromDB == TRUE ){ UserDelete( tmpusr );	tmpusr =  NULL; }
				goto loginok;
			}
			else 
			{
				// We have an API user!
				if( testAPIUser == 0 && uses->us_SessionID )
				{
					
					DEBUG("APIUSR!\n");
					// Generate sessionid
					if( uses->us_SessionID == NULL || !strlen( uses->us_SessionID ) )
					{
						DEBUG("\n\n\n============================================================\n \
											user name %s current timestamp %%lld login time %lld logout time %lld\n\
											============================================================\n", tmpusr->u_Name, timestamp, uses->us_LoggedTime , LOGOUT_TIME);
						
						char *hashBase = MakeString( 255 );
						sprintf( hashBase, "%ld%s%d", timestamp, tmpusr->u_FullName, ( rand() % 999 ) + ( rand() % 999 ) + ( rand() % 999 ) );
						HashedString( &hashBase );
				
						// Remove old one and update
						if( uses->us_SessionID )
						{
							FFree( uses->us_SessionID );
						}
						uses->us_SessionID = hashBase;
					}
					sb->LibraryMYSQLDrop( sb, sqlLib );
					DEBUG( "AUTHENTICATE: We found an API user! sessionid=%s\n", uses->us_SessionID );
					if(  tmpusr != NULL && userFromDB == TRUE ){ UserDelete( tmpusr );	tmpusr =  NULL; }
					goto loginok;
				}
			}
			sb->LibraryMYSQLDrop( sb, sqlLib );
		}
		if(  tmpusr != NULL && userFromDB == TRUE ){ UserDelete( tmpusr );	tmpusr =  NULL; }
		goto loginfail;
	}

	DEBUG("AUTHENTICATE END\n");

	if(  tmpusr != NULL && userFromDB == TRUE ){ UserDelete( tmpusr );	tmpusr =  NULL; }
	// next request, if session id exist then user is logged in
	
loginok:
	DEBUG("Login ok Stored\n");
	sb->sl_UserManagerInterface.UMStoreLoginAttempt( sb->sl_UM, name, "Login success", NULL );
	return uses;
	
loginfail:
	{
		time_t tm;
		sb->sl_UserManagerInterface.UMStoreLoginAttempt( sb->sl_UM, name, "Login fail", "Login fail" );
	}
// if login fail, goto must be used
	return NULL;
}

//
// logout
//

void Logout( struct AuthMod *l, Http *r, const char *name )
{
	SystemBase *sb = (SystemBase *)l->sb;
	UserSession *users = sb->sl_UserSessionManagerInterface.USMGetSessionBySessionID( sb->sl_USM, name );
	
	MYSQLLibrary *sqlLib = sb->LibraryMYSQLGet( sb );
	if( sqlLib != NULL )
	{
		char tmpQuery[ 1024 ];
		
		sqlLib->SNPrintF( sqlLib, tmpQuery, sizeof(tmpQuery), "DELETE FROM FUserSession WHERE SessionID = '%s'", name );
		
		sqlLib->QueryWithoutResults(  sqlLib, tmpQuery );
	
		sb->LibraryMYSQLDrop( sb, sqlLib );
	}
}

//
//
//

UserSession *IsSessionValid( struct AuthMod *l, Http *r, const char *sessionId )
{
	SystemBase *sb = (SystemBase *)l->sb;
	// to see if the session has lastupdated date less then 2 hours old
	UserSession *users = sb->sl_UserSessionManagerInterface.USMGetSessionBySessionID( sb->sl_USM, sessionId );
	time_t timestamp = time ( NULL );
	
	
	MYSQLLibrary *sqlLib = sb->LibraryMYSQLGet( sb );
	if( sqlLib == NULL )
	{
		FERROR("Cannot get mysql.library slot!\n");
		return NULL;
	}

	if( users == NULL )
	{
		//FUP_AUTHERR_USRNOTFOUND
		return NULL;
	}

	// we check if user is already logged in
	if( ( timestamp - users->us_LoggedTime ) < LOGOUT_TIME )
	{	// session timeout
		// we set timeout

		if( strcmp( users->us_SessionID, sessionId ) == 0 )
		{
			DEBUG( "IsSessionValid: Session is valid! %s\n", sessionId );
			char tmpQuery[ 512 ];
			
			sqlLib->SNPrintF( sqlLib, tmpQuery, sizeof(tmpQuery), "UPDATE FUserSession SET `LoggedTime` = '%ld' WHERE `SessionID` = '%s'", timestamp, sessionId );
			//sprintf( tmpQuery, "UPDATE FUserSession SET `LoggedTime` = '%ld' WHERE `SessionID` = '%s'", timestamp, sessionId );

			MYSQL_RES *res = sqlLib->Query( sqlLib, tmpQuery );
			if( res != NULL )
			{
			//users->us_User->u_Error = FUP_AUTHERR_UPDATE;
				sqlLib->FreeResult( sqlLib, res );
				sb->LibraryMYSQLDrop( sb, sqlLib );
				return users;
			}
		}
		else
		{
			DEBUG( "IsSessionValid: Wrong sessionid! %s\n", sessionId );
			
			// same session, update loggedtime
			//user->u_Error = FUP_AUTHERR_WRONGSESID;
			sb->LibraryMYSQLDrop( sb, sqlLib );
			return users;
		}
	}
	else
	{
		DEBUG( "IsSessionValid: Session has timed out! %s\n", sessionId );
		//user->u_Error = FUP_AUTHERR_TIMEOUT;
		sb->LibraryMYSQLDrop( sb, sqlLib );
		return users;
	}
	sb->LibraryMYSQLDrop( sb, sqlLib );
	return users;
}

//
// get user by session
//
/*
struct User *UserGetBySession( struct AuthMod *l, Http *r, const char *sessionId )
{
	SystemBase *sb = (SystemBase *)l->sb;
	MYSQLLibrary *sqlLib = sb->LibraryMYSQLGet( sb );
	
	if( !sqlLib )
	{
		FERROR("Cannot get user, mysql.library was not open\n");
		return NULL;
	}

	struct User *user = NULL;
	char tmpQuery[ 1024 ];
	sprintf( tmpQuery, " SessionId = '%s'", sessionId );
	int entries;
	
	user = ( struct User *)sqlLib->Load( sqlLib, UserDesc, tmpQuery, &entries );
	sb->LibraryMYSQLDrop( sb, sqlLib );
		
	if( user != NULL )
	{
		int res = UserInit( &user );
		if( res == 0 )
		{
			AssignGroupToUser( l, user );
			AssignApplicationsToUser( l, user );
			user->u_MountedDevs = NULL;
		}
	}
	
	DEBUG("UserGetBySession end\n");
	return user;
}
*/

//
// Set User Full Name
//

void SetAttribute( struct AuthMod *l, Http *r, struct User *u, const char *param, void *val )
{
	if( param != NULL && u != NULL )
	{
		if( strcmp("fname",param) == 0 )
		{
			if( u->u_FullName != NULL )
			{
				FFree( u->u_FullName );
			}
			if( val != NULL )
			{
				u->u_FullName = StringDuplicate( (char *) val );
			}
		}
		else if( strcmp("email",param) == 0 )
		{
			if( u->u_Email != NULL )
			{
				FFree( u->u_Email );
			}
			if( val != NULL )
			{
				u->u_Email = StringDuplicate( (char *) val );
			}
		}
	}
}

//
// Check if user has a textual permission, like module or filesystem
// in concert with applications
//

int UserAppPermission( struct AuthMod *l, Http *r, int userId, int applicationId, const char *permission )
{

	return -1;
}



//
// network handler
//

Http* WebRequest( struct AuthMod *l, Http *r, char **urlpath, Http* request )
{
	Http* response = NULL;
	/*
	if( strcmp( urlpath[ 0 ], "Authenticate" ) == 0 )
	{
		struct TagItem tags[] = {
			{ HTTP_HEADER_CONTENT_TYPE, (FULONG)  StringDuplicate( "text/html" ) },
			{	HTTP_HEADER_CONNECTION, (FULONG)StringDuplicate( "close" ) },
			{TAG_DONE, TAG_DONE}
		};
		
		response = HttpNewSimple( HTTP_200_OK,  tags );

						//request->query;
						//
						// PARAMETERS SHOULD BE TAKEN FROM
						// POST NOT GET
						
		if( request->parsedPostContent != NULL )
		{
			char *usr = NULL;
			char *pass = NULL;
			char *devname = NULL;
							
			HashmapElement *el =  HashmapGet( request->parsedPostContent, "username" );
			if( el != NULL )
			{
				usr = (char *)el->data;
			}
							
			el =  HashmapGet( request->parsedPostContent, "password" );
			if( el != NULL )
			{
				pass = (char *)el->data;
			}
			
			el =  HashmapGet( request->parsedPostContent, "password" );
			if( el != NULL )
			{
				pass = (char *)el->data;
			}
							
			if( usr != NULL && pass != NULL )
			{
				UserSession *loggedUser = l->Authenticate( l, r, NULL, usr, pass, devname, NULL );
				if( loggedUser != NULL )
				{
					char tmp[ 20 ];
					sprintf( tmp, "LERR: %d\n", loggedUser->us_User->u_Error );	// check user.library to display errors
					HttpAddTextContent( response, tmp );
				}
				else
				{
					HttpAddTextContent( response, "LERR: -1" );			// out of memory/user not found
				}
			}
		}
		DEBUG("user login response\n");

		//HttpWriteAndFree( response );
	}
	else
	{
		struct TagItem tags[] = {
			{	HTTP_HEADER_CONNECTION, (FULONG)StringDuplicate( "close" ) },
			{TAG_DONE, TAG_DONE}
		};
		
		response = HttpNewSimple(  HTTP_404_NOT_FOUND,  tags );
	
		//HttpWriteAndFree( response );
		return response;
	}
	*/
	
	return response;
}

