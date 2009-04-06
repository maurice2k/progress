/**
 * progress (cat/zcat with progress)
 *
 * This tool reads in a file and outputs progress information on STDERR
 * while it dumps the file to STDOUT. Also works with gzipped files.
 *
 * Example: progress dump.sql.gz |mysql -D mydb -u myuser -p
 *
 * @author Moritz Mertinkat (mmertinkat@rapidsoft.de)
 * @copyright rapidsoft GmbH
 * @url http://tools.rapidsoft.de/progress/
 *
 * **********************************************************************
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#define VERSION "0.7 (2008-05-04)"
#define ZLIB
//#define DEBUG

#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>
#include <getopt.h>
#include <string.h>

#ifdef WIN32
#include <fcntl.h>
#endif

#ifdef ZLIB

    #include <zlib.h>

    // The following struct was copied from gzio.c ((C) 1995-2005 Jean-loup Gailly).
    //
    // I changed z_off_t type to uLong for "start", "in" and "out" fields, so that the
    // struct is (hopefully) binary compatible with the compiled libz.so on your system.
    //
    // In general zlib is NOT compiled with LFS on 32-bit systems (uLong = 32 bit).
    // As z_off_t is defined as off_t on most systems (see zconf.h) and LFS redefines
    // off_t being 64-bit, the following struct would have busted the frame size on
    // 32-bit systems.
    // uLong on the other hand is defined as long which is 32-bit on 32-bit systems and
    // 64-bit on 64-bit systems :)

    typedef struct gz_stream {
        z_stream stream;
        int      z_err;   /* error code for last stream operation */
        int      z_eof;   /* set if end of input file */
        FILE     *file;   /* .gz file */
        Byte     *inbuf;  /* input buffer */
        Byte     *outbuf; /* output buffer */
        uLong    crc;     /* crc32 of uncompressed data */
        char     *msg;    /* error message */
        char     *path;   /* path name for debugging only */
        int      transparent; /* 1 if input file is not a .gz file */
        char     mode;    /* 'w' or 'r' */
        uLong    start;   /* start of compressed data in file (header skipped) */
        uLong    in;      /* bytes into deflate or inflate */
        uLong    out;     /* bytes out of deflate or inflate */
        int      back;    /* one character push-back */
        int      last;    /* true if push-back is last character */
    } gz_stream;

#endif

#define BUFLEN 512 * 1024
#define BUFLEN_GZIP 128 * 1024

FILE *fh;
FILE *loadavg;
#ifdef ZLIB
gzFile gz;
#endif
char *filename;
off_t bytes_read = 0;
off_t bytes_total = 0;
unsigned int start = 0;

unsigned int gzip = 0;
unsigned int raw_output = 0;
double max_load = 0;
int wait_display = 0;

unsigned char buf[BUFLEN];
struct stat statinfo;
unsigned int elapsed = 0;
char elapsed_str[256];

/**
 * Converts seconds to H:m:i format
 *
 * @param char* str Buffer to which the formatted time will be written
 * @param int seconds Number of seconds
 * @param int accurate If accurate = 0 then format_time will display ~__h for seconds > 86400
 * @return void
 */
void format_time(char* str, int seconds, int accurate) {

    int hour, min, sec;

    if (seconds > 86400 && accurate == 0) {
        sprintf(str, "~%uh", (unsigned int)(seconds / 3600.0));
    } else {

        sec = seconds % 3600;
        hour = (seconds - sec) / 3600;
        min =  sec / 60;
        sec = sec % 60;

        if (hour == 0) {
            sprintf(str, "%02d:%02d", min, sec);
        } else {
            sprintf(str, "%02d:%02d:%02d", hour, min, sec);
        }

    }

}

/**
 * Displays progress information if there's a percental change
 * or every 15 seconds whichever occurs first.
 *
 * @param int force_show
 * @return void
 */
void show_progress(int force_show) {

    char eta_str[256];
    char rate_str[128];
    char str[256];
    float percent = 0.0;
    static unsigned int old_percent = 0;
    unsigned int seconds_left = 0;
    static unsigned int last_rate_calculated = 0;
    static unsigned int last_displayed = 0;
    static double rate = 0.0;

    #ifdef DEBUG
    fprintf(stderr, "===> %Lu / %Lu\n", bytes_read, bytes_total);
    #endif

    if ((off_t)bytes_total > 0) {

        memset(eta_str, 0, sizeof(eta_str));
        memset(rate_str, 0, sizeof(rate_str));
        memset(str, 0, sizeof(str));

        if ((off_t)bytes_read <= (off_t)bytes_total) {
            // calculate percentage
            percent = (double)bytes_read / bytes_total * 100;
        }

        if (force_show == 1 && gzip == 1 && bytes_read + 8 == bytes_total) {
            // hack for gzipped files (8 byte struct at the end of file)
            percent = 100.0;
        }

        elapsed = time(NULL) - start;

        if (elapsed >= 2 || force_show == 1) {

            if (elapsed - last_rate_calculated >= 2) {
                // calculate rate every two seconds
                rate = (double)((off_t)bytes_read) / elapsed;
                last_rate_calculated = elapsed;
            }

            if (rate > 0) {

                double tmp = 0;

                seconds_left = (unsigned int)(((off_t)bytes_total - (off_t)bytes_read) / rate);

                strcat(eta_str, " -- ETA ");
                format_time(str, seconds_left, 0);
                strcat(eta_str, str);

                if (rate >= 1024 * 1024) {
                    tmp = rate / (1024 * 1024);
                    sprintf(rate_str, " (%.1f MB/s)", tmp);
                } else if (rate >= 1024) {
                    tmp = rate / 1024;
                    sprintf(rate_str, " (%.1f kB/s)", tmp);
                } else {
                    tmp = rate;
                    sprintf(rate_str, " (%.1f B/s)", tmp);
                }

            }

        }

        if ((unsigned int)percent != old_percent || (elapsed - last_displayed) >= 15) {

            format_time(elapsed_str, elapsed, 1);

            if ((off_t)bytes_read <= (off_t)bytes_total) {
                fprintf(stderr, "progress: %s -- %s -- %5.1f%%%s%s\n", elapsed_str, filename, percent, rate_str, eta_str);
            } else {
                fprintf(stderr, "progress: %s -- %s -- n/a\n", elapsed_str, filename);
            }

#ifndef WIN32
            if (max_load > 0) {

                char* pos;

                while (1) {

                    loadavg = fopen("/proc/loadavg", "r");

                    if (!loadavg) {
                        fprintf(stderr, "progress: Could not get cpu load information; max-load option skipped\n");
                        max_load = 0;
                        break;
                    }

                    fgets(str, 255, loadavg);
                    fclose(loadavg);

                    pos = strchr(str, 0x20);
                    if (pos != NULL) {
                        *pos = 0;
                    }

                    if (atof(str) >= max_load * 0.95) {

                        format_time(elapsed_str, elapsed, 1);
                        fprintf(stderr, "progress: %s -- %s -- cpu load exceeded (%.2f), sleeping for 5 seconds\n", elapsed_str, filename, atof(str));

                        sleep(5);

                        elapsed = time(NULL) - start;

                    } else {
                        break;
                    }

                }

            }
#endif

            last_displayed = elapsed;
            old_percent = (unsigned int)percent;

        }

    }

}

/**
 * Displays usage information
 *
 * @return void
 */
void print_usage() {

    fprintf(stderr, "progress %s\n", VERSION);
    fprintf(stderr, " Reads in a file and outputs progress information on STDERR\n");
    fprintf(stderr, " while it dumps the file to STDOUT. Also works with gzipped files.\n");
    fprintf(stderr, "Usage: progress [option] file\n");
    fprintf(stderr, " -r --raw        output given file as plain text (no gzip decompression)\n");
#ifndef WIN32
    fprintf(stderr, " -m --max-load   set cpu load limit (progress will sleep if exceeded)\n");
#endif
    fprintf(stderr, " -w --wait       don't display anything before transfer has been started\n");
    fprintf(stderr, " -h --help       give this help\n");
    fprintf(stderr, "Report bugs to <mmertinkat@rapidsoft.de>.\n");
    exit(1);

}

int main (int argc, char **argv) {

    int ch = 0;

    static struct option longopts[] = {
        { "raw",        no_argument,            NULL,     'r' },
        { "max-load",   required_argument,      NULL,     'm' },
        { "wait",       no_argument,            NULL,     'w' },
        { "help",       no_argument,            NULL,     'h' },
        { NULL,         0,                      NULL,     0   }
    };

    while ((ch = getopt_long(argc, argv, "rm:wh", longopts, NULL)) != -1) {

        switch (ch) {

            case 'r':
                raw_output = 1;
                break;

            case 'm':
                max_load = atof(optarg);
                break;

            case 'w':
                wait_display = 1;
                break;

            case 'h':
                print_usage();
                break;

            default:
                print_usage();

        }

    }

    argc -= optind;
    argv += optind;

    if (argc >= 1) {
        filename = argv[0];
    } else {
        fprintf(stderr, "progress: Please supply a filename; for help type \"progress --help\"\n");
        exit(3);
    }

    fh = fopen(filename, "rb");

    if (!fh) {
        fprintf(stderr, "progress: Unable to open file: %s\n", filename);
        exit(4);
    }

    if (fstat(fileno(fh), &statinfo) == -1) {
        fclose(fh);
        fprintf(stderr, "progress: Unable to retrieve file size for file: %s\n", filename);
        exit(5);
    }

    if ((statinfo.st_mode & S_IFMT) != S_IFREG) {
        fclose(fh);
        fprintf(stderr, "progress: Given file is not a regular one: %s\n", filename);
        exit(6);
    }


    bytes_total = (off_t)statinfo.st_size;

    memset(buf, 0, sizeof(buf));

    if (!raw_output) {

        fread(buf, 1, 2, fh);

        if (buf[0] == 0x1f && buf[1] == 0x8b) {
            gzip = 1;
        }

    }

#ifdef WIN32
    setmode(fileno(stdin), O_BINARY);
    setmode(fileno(stdout), O_BINARY);
#endif

    if (!wait_display) {
        format_time(elapsed_str, 0, 1);
        fprintf(stderr, "progress: %s -- %s -- starting (%Lu bytes)...\n", elapsed_str, filename, (long long)bytes_total);
    }

    start = time(NULL);

    fseek(fh, 0, SEEK_SET);

    if (gzip == 0) {

        int res = 0;

        while (!feof(fh)) {

            int length = fread(buf, 1, sizeof(buf), fh);

            if (length == 0) {
                break;
            }

            if (length < 0) {
                fclose(fh);
                fprintf(stderr, "progress: Error reading from file\n");
                exit(7);
            }

            bytes_read += length;

            res = fwrite(buf, 1, length, stdout);
            if (res != length) {
                fclose(fh);
                fprintf(stderr, "progress: Error writing to stdout\n");
                exit(8);
            }

            show_progress(0);

        }

        fclose(fh);

    } else {

#ifdef ZLIB

        int res = 0;

        if (bytes_total >= 0xFFFFFFFF && sizeof(uLong) <= 4) {
            fclose(fh);
            fprintf(stderr, "progress: zlib does not support gz files >4GB\n");
            exit(3);
        }

        gz_stream *s;

        gz = gzdopen(fileno(fh), "rb");

        if (!gz) {
            fclose(fh);
            fprintf(stderr, "progress: Unable to open gz file: %s\n", filename);
            exit(3);
        }

        s = (gz_stream *)gz;

        while (!gzeof(gz)) {

            int length = gzread(gz, buf, BUFLEN_GZIP);

            if (length == 0) {
                break;
            }

            if (length < 0) {
                gzclose(gz);
                fclose(fh);
                fprintf(stderr, "progress: Error decompressing gz file: %s\n", filename);
                exit(7);
            }

            bytes_read = (uLong)s->in + 8 + (uLong)s->start;

            res = fwrite(buf, 1, length, stdout);
            if (res != length) {
                gzclose(gz);
                fclose(fh);
                fprintf(stderr, "progress: Error writing to stdout\n");
                exit(8);
            }

            show_progress(0);

        }

        gzclose(gz);
        fclose(fh);

        show_progress(1);

#else
        fprintf(stderr, "progress: File seems to be gzip compressed, but progress was not compiled with zlib support\n");
        exit(9);
#endif

    }


    elapsed = time(NULL) - start;
    format_time(elapsed_str, elapsed, 1);
    fprintf(stderr, "progress: %s -- %s -- finished.\n", elapsed_str, filename);

    return 0;

}
