/*
 * gpspipe
 *
 * a simple program to connect to a gpsd daemon and dump the received data
 * to stdout
 *
 * This will dump the raw NMEA from gpsd to stdout
 *      gpspipe -r
 *
 * This will dump the GPSD sentences from gpsd to stdout
 *      gpspipe -w
 *
 * This will dump the GPSD and the NMEA sentences from gpsd to stdout
 *      gpspipe -wr
 *
 * bad code by: Gary E. Miller <gem@rellim.com>
 * all rights given to the gpsd project to release under whatever open source
 * license they use.  A thank would be nice if you use this code.
 *
 * just needs to be linked to netlib like this:
 *	gcc gpspipe.c ../netlib.c -o gpspipe
 *
 * TODO
 *      add "-p [device]" 
 *          This would force gpspipe to create a bidirectional pipe device
 *          for output.  Then programs that expect to connect to a raw GPS
 *          device could conenct to that.
 *
  $Id$
 */

#include <errno.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "gpsd.h"


static void usage() {
	fprintf(stderr, "Usage: gpspipe [OPTIONS] [server[:port]]\n\n"
	        "SVN ID: $Id$ \n"
		"-h show this help\n"
		"-r Dump raw NMEA\n"
	        "-w Dump gpsd native data\n"
	        "-t time stamp the data\n"
	        "-n [count] exit after count packets\n"
	        "-V print version and exit\n\n"
	        "You must specify one, or both, of -r/-w\n"
		);
}

int main( int argc, char **argv) {

	int s = 0;
        char buf[4096];
	char *cstr = NULL;
        ssize_t wrote = 0;
        bool dump_nmea = false;
        bool dump_gpsd = false;
        bool timestamp = false;
	bool new_line = true;
	long count = -1;
	int option;
        char *arg = NULL, *colon1, *colon2, *device = NULL; 
        char *port = DEFAULT_GPSD_PORT, *server = "127.0.0.1";
	//extern char *optarg;


	while ((option = getopt(argc, argv, "?hrwtVn:")) != -1) {
		switch (option) {
		case 'n':
			count = strtol(optarg, 0, 0);
			break;
		case 'r':
			dump_nmea = true;
			break;
		case 't':
			timestamp = true;
			break;
		case 'w':
			dump_gpsd = true;
			break;
		case 'V':
	                (void)fprintf(stderr, "%s: SVN ID: $Id$ \n", argv[0]);
			exit(0);
		case '?':
		case 'h':
		default:
			usage( );
			exit(1);
		}
	}
	if ( dump_nmea ) {
		if ( dump_gpsd ) {
			cstr = "rw\n";
		} else {
			cstr = "r\n";
		}
	} else if ( dump_gpsd ) {
		cstr = "w\n";
	} else {
		usage( );
		exit(1);
	}
	/* Grok the server, port, and device. */
	/*@ -branchstate @*/
	if (optind < argc) {
		arg = strdup(argv[optind]);
		/*@i@*/colon1 = strchr(arg, ':');
		server = arg;
		if (colon1 != NULL) {
			if (colon1 == arg) {
				server = NULL;
			} else {
				*colon1 = '\0';
			}
			port = colon1 + 1;
			colon2 = strchr(port, ':');
			if (colon2 != NULL) {
				if (colon2 == port) {
					port = NULL;
				} else {
					*colon2 = '\0';
				}
				device = colon2 + 1;
			}
		}
		colon1 = colon2 = NULL;
	}
	/*@ +branchstate @*/

	/*@ -nullpass @*/
	s = netlib_connectsock( server, port, "tcp");
	if ( s < 0 ) {
		fprintf( stderr, "%s: could not connect to gpsd %s:%s, %s(%d)\n"
			, argv[0] , server, port, strerror(errno), errno);
		exit (1);
	}
	/*@ +nullpass @*/

	wrote = write( s, cstr, strlen(cstr) );
	if ( (ssize_t)strlen(cstr) != wrote ) {
		fprintf( stderr, "%s: write error, %s(%d)\n", argv[0]
			, strerror(errno), errno);
		exit (1);
	}

	for(;;) {
		int i = 0;
		int readbytes = 0;

		readbytes = (int)read( s, buf, sizeof(buf));
		if ( readbytes > 0 ) {
		    for ( i = 0 ; i < readbytes ; i++ ) {
			char c = buf[i];
			if ( new_line && timestamp ) {
				time_t now = time(NULL);

				new_line = 0;
				if ( 0 > fprintf( stdout, "%.24s :",
					 ctime( &now )) ) {
					fprintf( stderr
						, "%s: Write Error, %s(%d)\n"
						, argv[0]
						, strerror(errno), errno);
					exit (1);
				}
			}
			if ( EOF == fputc( c, stdout) ) {
				fprintf( stderr, "%s: Write Error, %s(%d)\n"
					, argv[0]
					, strerror(errno), errno);
				exit (1);
			}
		
			if ( c == '\n' ) {
			    new_line = true;
			    /* flush after eveery good line */
			    if (  fflush( stdout ) ) {
				fprintf( stderr, "%s: fflush Error, %s(%d)\n"
					, argv[0]
					, strerror(errno), errno);
				exit (1);
			    }
			    if ( 0 <= count ) {
			        if ( 0 >= --count ) {
				    /* completed count */
			            exit(0);
			        }
			    }
			       
			}
		    }
		} else if ( readbytes < 0 ) {
			fprintf( stderr, "%s: Read Error %s(%d)\n", argv[0]
				, strerror(errno), errno);
		
			exit(1);
		}
	}
	//exit(0);
}
