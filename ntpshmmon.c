/* ntpshmmon.c -- monitor the inner end of an ntpshmwrite.connection
 *
 * This file is Copyright (c) 2010 by the GPSD project
 * BSD terms apply: see the file COPYING in the distribution root for details.
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <limits.h>
#include <unistd.h>

#include "gps.h"	/* for safe_atof() */
#include "gpsd_config.h"
#include "ntpshm.h"
#include "revision.h"
#include "timespec.h"

#define NTPSEGMENTS	256	/* NTPx for x any byte */

static struct shmTime *segments[NTPSEGMENTS + 1];

int main(int argc, char **argv)
{
    int option;
    int	i;
    bool killall = false;
    bool verbose = false;
    int nsamples = INT_MAX;
    time_t timeout = (time_t)INT_MAX, starttime = time(NULL);
    /* a copy of all old segments */
    struct shm_stat_t	shm_stat_old[NTPSEGMENTS + 1];;

    memset( shm_stat_old, 0 ,sizeof( shm_stat_old));

#define USAGE	"usage: ntpshmmon [-s] [-n max] [-t timeout] [-v] [-h] [-V]\n"
    while ((option = getopt(argc, argv, "hn:st:vV")) != -1) {
	switch (option) {
	case 'n':
	    nsamples = atoi(optarg);
	    break;
	case 's':
	    killall = true;
	    break;
	case 't':
	    timeout = (time_t)atoi(optarg);
	    break;
	case 'v':
	    verbose = true;
	    break;
	case 'V':
	    (void)fprintf(stderr, "%s: version %s (revision %s)\n",
			  argv[0], VERSION, REVISION);
	    exit(EXIT_SUCCESS);
	case 'h':
	default:
	    fprintf(stderr, USAGE);
	    break;
	}
    }

    /* grab all segments, keep the non-null ones */
    for (i = 0; i < NTPSEGMENTS; i++) {
	segments[i] = shm_get(i, false, true);
	if (verbose && segments[i] != NULL)
	    fprintf(stderr, "unit %d opened\n", i);
    }

    if (killall) {
	struct shmTime **pp;

	for (pp = segments; pp < segments + NTPSEGMENTS; pp++)
	    if (*pp != NULL)
		(void)shmdt((void *)(*pp));
	exit(EXIT_SUCCESS);
    }

    (void)printf("ntpshmmon version 1\n");
    (void)printf("#      Name   Seen@                Clock                Real               L Prec\n");

    do {
	/* the current segment */
	struct shm_stat_t	shm_stat;

	for (i = 0; i < NTPSEGMENTS; i++) {
	    long long diff;  /* 32 bit long is too short for a timespec */
	    enum segstat_t status = ntp_read(segments[i], &shm_stat, false);
	    if (verbose)
		fprintf(stderr, "unit %d status %d\n", i, status);
	    switch(status) {
	    case OK:
		/* ntpd can slew the clock at 120% real time
                 * so do not lock out slightly short cycles
		 * use 50% of cycle time as lock out limit.
                 * ignore that system time may jump. */
		diff = timespec_diff_ns(shm_stat.tvr, shm_stat_old[i].tvr);
		if ( 0 == diff) {
		    /* no change in tvr */
		    break;
		}
		diff = timespec_diff_ns(shm_stat.tvt, shm_stat_old[i].tvt);
		if ( 0 == diff) {
		    /* no change in tvt */
		    break;
		}
		printf("%s %ld.%09ld %ld.%09ld %ld.%09ld %d %3d\n",
		       ntp_name(i),
		       (long)shm_stat.tvc.tv_sec, shm_stat.tvc.tv_nsec,
		       (long)shm_stat.tvr.tv_sec, shm_stat.tvr.tv_nsec,
		       (long)shm_stat.tvt.tv_sec, shm_stat.tvt.tv_nsec,
		       shm_stat.leap, shm_stat.precision);
		--nsamples;
		/* save the new time stamp */
		shm_stat_old[i] = shm_stat; /* structure copy */

		break;
	    case NO_SEGMENT:
		break;
	    case NOT_READY:
		/* do nothing, data not ready, wait another cycle */
		break;
	    case BAD_MODE:
		fprintf(stderr, "ntpshmmon: unknown mode %d on segment %s\n",
			shm_stat.status, ntp_name(i));
		break;
	    case CLASH:
		/* do nothing, data is corrupt, wait another cycle */
		break;
	    default:
		fprintf(stderr, "ntpshmmon: unknown status %d on segment %s\n",
			status, ntp_name(i));
		break;
	    }
	}
	/* all segments now checked */

	/*
	 * Even on a 1 Hz PPS, a sleep(1) may end up
         * being sleep(1.1) and missing a beat.  Since
	 * we're ignoring duplicates via timestamp, polling
	 * at fast intervals should not be a problem
	 *
	 * PPS is not always one pulse per second.
	 * the Garmin GPS 18x-5Hz outputs 5 pulses per second.
         * That is a 200 millSec cycle, minimum 20 milliSec duration
         * we will wait 1 milliSec out of caution
         *
         * and, of course, usleep() may sleep a lot longer than we ask...
	 */
	(void)usleep((useconds_t)1000);
    } while
	    (nsamples != 0 && time(NULL) - starttime < timeout);

    exit(EXIT_SUCCESS);
}

/* end */
