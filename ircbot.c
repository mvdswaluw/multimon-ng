#include "libircclient/libircclient.h"
#include "libircclient/libirc_rfcnumeric.h"
#include "multimon.h"
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>

#define DEFAULT_CFG_FILE  	"/.pagerbot.cfg"
#define DEFAULT_IRC_SERVER  	"irc.freenode.org"
#define DEFAULT_IRC_PORT	"6667"
#define DEFAULT_IRC_CHANNEL  	"#multimon-ng"
#define DEFAULT_IRC_NICK	"pagerbot"
#define DEFAULT_IRC_USERNAME 	"pagerbot"
#define DEFAULT_IRC_REALNAME 	"pagerbot"

struct cfg {
	char cfg_file[1024];
	char server[255];
	char port[16];
	char channel[64];
	char nick[64];
	char username[16];
	char realname[16];
	char server_connect_msg[255];
	char server_connect_nick[16];
	char server_connect_delay[6];
	char channel_connect_msg[255];
	char channel_connect_nick[16];
	char channel_connect_delay[6];
} irc_cfg = {
	DEFAULT_CFG_FILE,
	DEFAULT_IRC_SERVER,
	DEFAULT_IRC_PORT,
	DEFAULT_IRC_CHANNEL,
	DEFAULT_IRC_NICK,
	DEFAULT_IRC_USERNAME,
	DEFAULT_IRC_REALNAME,
	"",
	"",
	"",
	"",
	"",
	""
};

#define NUM_CFG_OPTS 12

const char *cfg_options[] = {
	"server", "port", "channel", "nick", "username", "realname", 
	"server_connect_msg", "server_connect_nick", "server_connect_delay",
	"channel_connect_msg", "channel_connect_nick", "channel_connect_delay"
};

char *cfg_vars[] = {
	irc_cfg.server, irc_cfg.port, irc_cfg.channel, irc_cfg.nick, irc_cfg.username, irc_cfg.realname,
		irc_cfg.server_connect_msg, irc_cfg.server_connect_nick, irc_cfg.server_connect_delay,
		irc_cfg.channel_connect_msg, irc_cfg.channel_connect_nick, irc_cfg.channel_connect_delay
};

irc_session_t *session;

void irc_print_msg (int address, char *message)
{
	irc_cmd_msg (session, irc_cfg.channel, message);
}

//Sent on successful connection to server, useful for NickServ
static void send_server_connect_msg (void)
{
	//Check to see if we have commands to run
	if (strlen (irc_cfg.server_connect_msg) != 0)
	{
		//Make sure a nick was specified
		if (strlen (irc_cfg.server_connect_nick) == 0)
		{
			verbprintf (2, "IRC: server_connect_msg specified but not server_connect_nick\n");
			return;
		}
		if (atoi (irc_cfg.server_connect_delay) > 0)
		{
			verbprintf (2, "IRC: Waiting %i seconds before sending command\n", atoi(irc_cfg.server_connect_delay));
			sleep (atoi (irc_cfg.server_connect_delay));
		}
		verbprintf (2, "IRC: Sending server_connect_msg\n");
		irc_cmd_msg (session, irc_cfg.server_connect_nick, irc_cfg.server_connect_msg);
	}
}

//Sent on successful connection to channel, useful for ChanServ
static void send_channel_connect_msg (void)
{
	//Check to see if we have commands to run
	if (strlen (irc_cfg.channel_connect_msg) != 0)
	{
		//Make sure a nick was specified
		if (strlen (irc_cfg.channel_connect_nick) == 0)
		{
			verbprintf (2, "IRC: channel_connect_msg specified but not channel_connect_nick\n");
			return;
		}
		if (atoi (irc_cfg.channel_connect_delay) > 0)
		{
			verbprintf (2, "IRC: Waiting %i seconds before sending command\n", atoi(irc_cfg.channel_connect_delay));
			sleep (atoi (irc_cfg.channel_connect_delay));
		}
		verbprintf (2, "IRC: Sending channel_connect_msg\n");
		irc_cmd_msg (session, irc_cfg.channel_connect_nick, irc_cfg.channel_connect_msg);
	}

}

//Called when successfully connected to a server
void event_connect (irc_session_t *session, const char *event, const char *origin, const char **params, unsigned int count)
{
	verbprintf (2, "IRC: Successfully connected to server %s\n", irc_cfg.server);

	send_server_connect_msg ();
	
	verbprintf (2, "IRC: Attempting to join %s\n", irc_cfg.channel);

	if (irc_cmd_join (session, irc_cfg.channel, NULL))
	{
		verbprintf (0, "IRC: Error joining channel %s\n", irc_cfg.channel);
		return;
	}
	
	verbprintf (2, "IRC: Connected to %s\n", irc_cfg.channel);

	send_channel_connect_msg ();
}

void event_privmsg (irc_session_t *session, const char *event, const char *origin, const char **params, unsigned int count)
{
	printf ("'%s' said to me (%s): %s\n", origin ? origin : "someone", params[0], params[1] );
}

void event_numeric (irc_session_t *session, unsigned int event, const char *origin, const char **params, unsigned int count)
{
	//FIXME

	switch (event)
	{
		case LIBIRC_RFC_RPL_NAMREPLY:
		      verbprintf (2, "IRC: User list: %s\n", params[3]);
		      break;
		case LIBIRC_RFC_RPL_WELCOME:
		      verbprintf (2, "IRC: %s\n", params[1]);
		      break;
		case LIBIRC_RFC_RPL_YOURHOST:
		      verbprintf (2, "IRC: %s\n", params[1]);
		case LIBIRC_RFC_RPL_ENDOFNAMES:
		      verbprintf (2, "IRC: End of user list\n");
		      break;
		case LIBIRC_RFC_RPL_MOTD:
		      verbprintf (2, "%s\n", params[1]);
		      break;
		default:
		      verbprintf (2, "IRC: event_numeric %u\n", event);
		      break;
	}
}

void event_channel (irc_session_t *session, const char *event, const char *origin, const char **params, unsigned int count)
{
}

static int cfg_load (void)
{
	FILE *in_file;
	char cfg_buff[64];
	int i;
	

	strcpy (cfg_buff, getenv("HOME"));
	strcat (cfg_buff, irc_cfg.cfg_file);
	strcpy (irc_cfg.cfg_file, cfg_buff);
	
	verbprintf (2, "IRC: Attempting to load config file %s\n", irc_cfg.cfg_file);

	in_file = fopen (irc_cfg.cfg_file, "r");

	if (in_file == NULL)
		return 1;

	//Copy a line from cfg file into cfg_buff
	while (fgets (cfg_buff, 64, in_file) != NULL)
	{
		//Cycle through the different options
		for (i = 0; i < NUM_CFG_OPTS; i++)
		{
			//Look for a match and make sure there's a = after it
			if (strncmp (cfg_buff, cfg_options[i], strlen(cfg_options[i])) == 0 &&
					strncmp (cfg_buff + strlen (cfg_options[i]), "=", 1) == 0)
			{
				//Copy to variable
				strcpy (cfg_vars[i], cfg_buff + strlen (cfg_options[i]) + 1);
				//Strip newline
				strtok (cfg_vars[i], "\n");
				//Check if empty var
				if (strcmp (cfg_vars[i], "\n") == 0)
				{
					verbprintf (0, "IRC: Empty cfg option %s\n", cfg_options[i]);
					fclose (in_file);
					return 1;
				}
			}
		}
	}
	fclose (in_file);
	return 0;

}

void *irc_monitor (void *threadid)
{
	if (irc_run (session))
	{
		verbprintf (0, "IRC: ERROR %s\n", irc_strerror(irc_errno (session)));
	}
	return 0;
}

int ircbot_init (void)
{
	irc_callbacks_t callbacks;
	pthread_t irc_thread[1];

	if (cfg_load ())
		verbprintf (2, "IRC: No configuration file found, using defaults\n");

	verbprintf (2, "Configuration options:\n");
	verbprintf (2, "server = %s\n", irc_cfg.server);
	verbprintf (2, "port = %s\n", irc_cfg.port);
	verbprintf (2, "channel = %s\n", irc_cfg.channel);
	verbprintf (2, "nick = %s\n", irc_cfg.nick);
	verbprintf (2, "username = %s\n", irc_cfg.username);
	verbprintf (2, "realname = %s\n", irc_cfg.realname);
	verbprintf (2, "server_connect_msg = %s\n", irc_cfg.server_connect_msg);
	verbprintf (2, "server_connect_nick = %s\n", irc_cfg.server_connect_nick);
	verbprintf (2, "server_connect_delay = %s\n", irc_cfg.server_connect_delay);
	verbprintf (2, "channel_connect_msg = %s\n", irc_cfg.channel_connect_msg);
	verbprintf (2, "channel_connect_nick = %s\n", irc_cfg.channel_connect_nick);
	verbprintf (2, "channel_connect_delay = %s\n\n", irc_cfg.channel_connect_delay);
	verbprintf (2, "IRC: Bot initilising\n");

	memset (&callbacks, 0, sizeof(callbacks));

	callbacks.event_connect = event_connect;
	callbacks.event_numeric = event_numeric;
	callbacks.event_privmsg = event_privmsg;
	callbacks.event_channel = event_channel;
	
	session = irc_create_session(&callbacks);

	if (!session)
	{
		verbprintf (0, "IRC: Error setting up session\n");
		return 1;
	}
	
	irc_option_set(session, LIBIRC_OPTION_STRIPNICKS);

	verbprintf (2, "IRC: Attempting to connect to server %s:%i channel %s with nick %s\n", 
			irc_cfg.server, atoi(irc_cfg.port), irc_cfg.channel, irc_cfg.nick);

	if (irc_connect (session, irc_cfg.server, atoi(irc_cfg.port), 0, irc_cfg.nick, irc_cfg.username, irc_cfg.realname ))
	{
		verbprintf (0, "IRC: ERROR %s\n", irc_strerror(irc_errno (session)));
	}
	//Enter main loop
	pthread_create (&irc_thread[0], NULL, irc_monitor, (void  *)0);
	
	return 0;
}

void ircbot_shutdown (void)
{
	irc_disconnect (session);
}
