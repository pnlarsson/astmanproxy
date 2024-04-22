/* 	Asterisk Manager Proxy
	Copyright (c) 2005-2008 David C. Troy <dave@popvox.com>

	This program is free software, distributed under the terms of
	the GNU General Public License.

	config_perms.c
	routines to read and parse the astmanproxy.users file
*/

#include "astmanproxy.h"

extern pthread_mutex_t userslock;

void *free_userperm(struct proxy_user *pu) {
	struct proxy_user *next_pu;

	while( pu ) {
		next_pu = pu->next;
		free( pu );
		pu = next_pu;
	}
	return 0;
}

void *add_userperm(char* username, char *userspec, struct proxy_user **pu) {

	int ccount = 0;
	struct proxy_user *user;
	char *s;

	/* malloc ourselves a server credentials structure */
	user = malloc(sizeof(struct proxy_user));
	if ( !user ) {
		fprintf(stderr, "Failed to allocate user credentials: %s\n", strerror(errno));
		exit(1);
	}
	memset(user, 0, sizeof (struct proxy_user) );

	s = userspec;
	strncpy(user->username, username, sizeof(user->username)-1 );
	do {
		if ( *s == ',' ) {
			ccount++;
			continue;
		}
		if( ccount > 0 )
			*s = tolower(*s);
		switch(ccount) {
			case 0:
			 strncat(user->secret, s, 1);
			 break;
			case 1:
			 strncat(user->channel, s, 1);
			 break;
			case 2:
			 strncat(user->ocontext, s, 1);
			 break;
			case 3:
			 strncat(user->icontext, s, 1);
			 break;
			case 4:
			 strncat(user->account, s, 1);
			 break;
			case 5:
			 strncat(user->server, s, 1);
			 break;
			case 6:
			 user->more_events[0] = 'y';	// Any non-null entry
			 break;
			case 7:
			 strncat(user->filters, s, 1);
			 break;
		}
	} while (*(s++));

	if( strcasestr(user->filters, FILT_TOK_CDRONLY) )
		user->filter_bits |= FILT_CDRONLY;
	if( strcasestr(user->filters, FILT_TOK_BRIONLY) )
		user->filter_bits |= FILT_BRIONLY;
	if( strcasestr(user->filters, FILT_TOK_XFRONLY) )
		user->filter_bits |= FILT_XFRONLY;
	if( strcasestr(user->filters, FILT_TOK_NOVAR) )
		user->filter_bits |= FILT_NOVAR;

	user->next = *pu;
	*pu = user;

	if (debug)
		debugmsg("perm: %s, %s, %d", username, userspec, user->filter_bits);

	return 0;
}

void *processperm(char *s, struct proxy_user **pu) {
	char name[80],value[256];
	int nvstate = 0;


	memset (name,0,sizeof name);
	memset (value,0,sizeof value);

	do {
		if ( *s == ' ' || *s == '\t')
			continue;
		if ( *s == ';' || *s == '#' || *s == '\r' || *s == '\n' )
			break;
		if ( *s == '=' ) {
			nvstate = 1;
			continue;
		}
		if (!nvstate)
			strncat(name, s, 1);
		else
			strncat(value, s, 1);
	} while (*(s++));

	add_userperm(name,value,pu);

	return 0;
}

int ReadPerms(char *user_file) {
	FILE *FP;
	char buf[1024];
	char cfn[80];
	struct proxy_user *pu;

	if(user_file == NULL){
		sprintf(cfn, "%s/%s", PDIR, PFILE);
		user_file = cfn;
	}

	pu=0;
	FP = fopen( user_file, "r" );

	if ( !FP )
	{
		fprintf(stderr, "Unable to open permissions file: %s/%s!\n", PDIR, PFILE);
		exit( 1 );
	}

	if (debug)
		debugmsg("config: parsing configuration file: %s", user_file);

	while ( fgets( buf, sizeof buf, FP ) ) {
		if (*buf == ';' || *buf == '\r' || *buf == '\n' || *buf == '#') continue;
		processperm(buf,&pu);
	}

	fclose(FP);

	pthread_mutex_lock(&userslock);
	free_userperm(pc.userlist);
	pc.userlist=pu;
	pthread_mutex_unlock(&userslock);

	return 0;
}

