/*
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Contributor(s): (c)2016 by teglo.info Berryfrog 
 *
 */
#include "RCSwitch.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <signal.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <mysql/mysql.h>
#include <iostream>
#include <libconfig.h>

MYSQL *mysql1;
RCSwitch mySwitch;

static int running = 0;
static int counter = 0;
static char *database_host = NULL;
static char *conf_file_name = NULL;
static char *pid_file_name = NULL;
static int pid_fd = -1;
static char *app_name = NULL;
static FILE *log_stream;
static char *MySQLHost;

//Config handling
config_t cfg, *cf;
const config_setting_t *retries;

int loop = 10;
const char *dbhost = NULL;
const char *dbname = NULL;
const char *dbuser = NULL;
const char *dbpass = NULL;
int count, n, enabled;

using namespace std;

/*
  CONNECT TO DATABASE ****************
*/
void mysql_connect (void) {

     //initialize MYSQL object for connections
     mysql1 = mysql_init(NULL);

     if(mysql1 == NULL) {
         fprintf(stderr, "ABB : %s\n", mysql_error(mysql1));
         return;
     }

	if (config_lookup_string(cf, "mysql.dbhost", &dbhost))
	if (config_lookup_string(cf, "mysql.dbname", &dbname))
	if (config_lookup_string(cf, "mysql.dbuser", &dbuser))
    if (config_lookup_string(cf, "mysql.dbpass", &dbpass))

     //Connect to the database
     if(mysql_real_connect(mysql1, dbhost, dbuser, dbpass, dbname, 0, NULL, 0) == NULL) {
      fprintf(stderr, "%s\n", mysql_error(mysql1));
     } else {
         printf("Database connection to %s successful.\r\n",dbhost);
     }
}

/*
  DISCONNECT FROM DATABASE ****************
*/
void mysql_disconnect (void) {
    mysql_close(mysql1);
    printf( "Disconnected from database.\r\n");
}

void mysql_write (int transmitter_id,int temp_dht_hic,int temp_dht_hif,int humidity_dht,int pressure_bmp,int altitude_bmp,int temp_bmp,int vcc_atmega,int vis_is,int uvindex_is) {
   
	float db_temp_dht_hic = (float) temp_dht_hic / 100;
	float db_temp_dht_hif = (float) temp_dht_hif / 100;
	float db_humidity_dht = (float) humidity_dht / 100;
	float db_pressure_bmp = (float) pressure_bmp / 10;
	float db_altitude_bmp = (float) altitude_bmp / 10;
	float db_temp_bmp     = (float) temp_bmp / 100;
	float db_vcc_atmega   = (float) vcc_atmega / 1000;
	float db_uvindex_is   = (float) uvindex_is / 1000;

	printf("Write datas into db for transmitter ID: %i\n",transmitter_id);
	printf("Temp DHT hiC: %.2f\n",db_temp_dht_hic);
	printf("Temp DHT hiF: %.2f\n",db_temp_dht_hif);
	printf("Humidity of DHT: %.2f\n",db_humidity_dht);
	printf("Pressure of BMP: %.1f\n",db_pressure_bmp);
	printf("Altitude of BMP: %.1f\n",db_altitude_bmp);
	printf("Temp of BMP: %.2f\n",db_temp_bmp);
	printf("VCC in ATMega: %.3f\n",db_vcc_atmega);
	printf("Vis of IS: %i\n",vis_is);
	printf("UV index: %.3f\n",db_uvindex_is);

    if(mysql1 != NULL) {
		
		//Retrieve all data from alarm_times
		char buf[512];
		sprintf(
			buf,
			"INSERT INTO measurements (transmitter_id,vcc_atmega,temp_dht_hic,temp_dht_hif,humidity_dht,pressure_bmp,altitude_bmp,temp_bmp,vis_is,uvindex_is) VALUES (%i,%.3f,%.2f,%.2f,%.2f,%.1f,%.1f,%.2f,%i,%.3f)",
					transmitter_id,
					db_vcc_atmega,
					db_temp_dht_hic,
					db_temp_dht_hif,
					db_humidity_dht,
					db_pressure_bmp,
					db_altitude_bmp,
					db_temp_bmp,
					vis_is,
					db_uvindex_is
		);

        if (mysql_query(mysql1, buf)) { 
             fprintf(stderr, "%s\n", mysql_error(mysql1));
             return;
        }
    }
}

/**
 * \Read configuration from config file
 */
int read_conf_file(int reload) {


	FILE *conf_file = NULL;
	int ret = -1;

	if (conf_file_name == NULL) return 0;

	cf = &cfg;
 	config_init(cf);

    if (!config_read_file(cf, conf_file_name)) {
        fprintf(stderr, "%s:%d - %s\n",
            config_error_file(cf),
            config_error_line(cf),
            config_error_text(cf));
        config_destroy(cf);
        return(EXIT_FAILURE);
    }

	return ret;
}

/**
 * \brief This function tries to test config file
 */
int test_conf_file(char *_conf_file_name) {
	FILE *conf_file = NULL;
	int ret = -1;

	conf_file = fopen(_conf_file_name, "r");

	if(conf_file == NULL) {
		fprintf(stderr, "Can't read config file %s\n",
			_conf_file_name);
		return EXIT_FAILURE;
	}

	if(ret <= 0) {
		fprintf(stderr, "Wrong config file %s\n",
			_conf_file_name);
	}

	fclose(conf_file);

	if(ret > 0)
		return EXIT_SUCCESS;
	else
		return EXIT_FAILURE;
}


/**
 * \brief Callback function for handling signals.
 * \param	sig	identifier of signal
 */
void handle_signal(int sig) {
	if(sig == SIGINT) {
		fprintf(log_stream, "Debug: stopping daemon ...\n");
		/* Unlock and close lockfile */
		if(pid_fd != -1) {
			lockf(pid_fd, F_ULOCK, 0);
			close(pid_fd);
		}
		/* Try to delete lockfile */
		if(pid_file_name != NULL) {
			unlink(pid_file_name);
		}
		running = 0;
		/* Reset signal handling to default behavior */
		signal(SIGINT, SIG_DFL);
	} else if(sig == SIGHUP) {
		fprintf(log_stream, "Debug: reloading daemon config file ...\n");
		read_conf_file(1);
	} else if(sig == SIGCHLD) {
		fprintf(log_stream, "Debug: received SIGCHLD signal\n");
	}
}

/**
 * \brief This function will daemonize this app
 */
static void daemonize() {
	pid_t pid = 0;
	int fd;

	/* Fork off the parent process */
	pid = fork();

	/* An error occurred */
	if(pid < 0) {
		exit(EXIT_FAILURE);
	}

	/* Success: Let the parent terminate */
	if(pid > 0) {
		exit(EXIT_SUCCESS);
	}

	/* On success: The child process becomes session leader */
	if(setsid() < 0) {
		exit(EXIT_FAILURE);
	}

	/* Ignore signal sent from child to parent process */
	signal(SIGCHLD, SIG_IGN);

	/* Fork off for the second time*/
	pid = fork();

	/* An error occurred */
	if(pid < 0) {
		exit(EXIT_FAILURE);
	}

	/* Success: Let the parent terminate */
	if(pid > 0) {
		exit(EXIT_SUCCESS);
	}

	/* Set new file permissions */
	umask(0);

	/* Change the working directory to the root directory */
	/* or another appropriated directory */
	chdir("/");

	/* Close all open file descriptors */
	for(fd = sysconf(_SC_OPEN_MAX); fd > 0; fd--)
	{
		close(fd);
	}

	/* Reopen stdin (fd = 0), stdout (fd = 1), stderr (fd = 2) */
	stdin = fopen("/dev/null", "r");
	stdout = fopen("/dev/null", "w+");
	stderr = fopen("/dev/null", "w+");

	/* Try to write PID of daemon to lockfile */
	if(pid_file_name != NULL)
	{
		char str[256];
		pid_fd = open(pid_file_name, O_RDWR|O_CREAT, 0640);
		if(pid_fd < 0)
		{
			/* Can't open lockfile */
			exit(EXIT_FAILURE);
		}
		if(lockf(pid_fd, F_TLOCK, 0) < 0)
		{
			/* Can't lock file */
			exit(EXIT_FAILURE);
		}
		/* Get current PID */
		sprintf(str, "%d\n", getpid());
		/* Write PID to lockfile */
		write(pid_fd, str, strlen(str));
	}
}

/**
 * \brief Print help for this application
 */
void print_help(void) {

	printf("\n Usage: %s [OPTIONS]\n\n", app_name);
	printf("  Options:\n");
	printf("   -h --help                 Print this help\n");
	printf("   -c --conf_file filename   Read configuration from the file\n");
	printf("   -l --log_file  filename   Write logs to the file\n");
	printf("   -d --daemon               Daemonize this application\n");
	printf("   -p --pid_file  filename   PID file used by daemonized app\n");
	printf("\n");
}

/* Main function */
int main(int argc, char *argv[]) {

	static struct option long_options[] = {
		{"conf_file", required_argument, 0, 'c'},
		{"test_conf", required_argument, 0, 't'},
		{"log_file", required_argument, 0, 'l'},
		{"help", no_argument, 0, 'h'},
		{"daemon", no_argument, 0, 'd'},
		{"pid_file", required_argument, 0, 'p'},
		{NULL, 0, 0, 0}
	};

	//Init some variables
	int value, option_index = 0, ret;
	char *log_file_name = NULL;
	int start_daemonized = 0;
	//Default PIN for 433MHz receiver data
	int rx_PIN = 2;

	app_name = argv[0];

	/* Try to process all command line arguments */
	while( (value = getopt_long(argc, argv, "c:l:t:p:dh", long_options, &option_index)) != -1) {
		switch(value) {
			case 'c':
				conf_file_name = strdup(optarg);
				break;
			case 'l':
				log_file_name = strdup(optarg);
				break;
			case 'p':
				pid_file_name = strdup(optarg);
				break;
			case 't':
				return test_conf_file(optarg);
			case 'd':
				start_daemonized = 1;
				break;
			case 'h':
				print_help();
				return EXIT_SUCCESS;
			case '?':
				print_help();
				return EXIT_FAILURE;
			default:
				break;
		}
	}

	/* When daemonizing is requested at command line. */
	if(start_daemonized == 1) {
		/* It is also possible to use glibc function deamon()
		 * at this point, but it is useful to customize your daemon. */
		daemonize();
	}

	/* Open system log and write message to it */
	openlog(argv[0], LOG_PID|LOG_CONS, LOG_DAEMON);
	syslog(LOG_INFO, "Started %s", app_name);

	/* Daemon will handle two signals */
	signal(SIGINT, handle_signal);
	signal(SIGHUP, handle_signal);

	/* Try to open log file to this daemon */
	if(log_file_name != NULL) {
		log_stream = fopen(log_file_name, "a+");
		if (log_stream == NULL)
		{
			syslog(LOG_ERR, "Can not open log file: %s, error: %s",
				log_file_name, strerror(errno));
			log_stream = stdout;
		}
	} else {
		log_stream = stdout;
	}

	/* Read configuration from config file */
	read_conf_file(0);

	if (config_lookup_int(cf, "loop", &loop))
	if (config_lookup_int(cf, "rx_PIN", &rx_PIN))

	//Init the wiringPi interface
	if(wiringPiSetup() == -1) {
       printf("wiringPiSetup failed, exiting...");
       return 0;
     }

     int pulseLength = 0;
     if (argv[1] != NULL) pulseLength = atoi(argv[1]);

     mySwitch = RCSwitch();
	 if (pulseLength != 0) mySwitch.setPulseLength(pulseLength);
     mySwitch.enableReceive(rx_PIN);  // Receiver on interrupt 0 => that is pin #2

	/* This global variable can be changed in function handling signal */
	running = 1;

	//Define output value for database
	int receiver_id;
	int sensor_id;
	int measured_value;
	int temp_dht_hic = 0;
	int check11 = 0;
	int temp_dht_hif = 0;
	int check12 = 0;
	int humidity_dht = 0;
	int check13 = 0;
	int pressure_bmp = 0;
	int check14 = 0;
	int altitude_bmp = 0;
	int check15 = 0;
	int temp_bmp = 0;
	int check16 = 0;
	int vcc_atmega = 0;
	int check17 = 0;
	int vis_is = 0;
	int check18 = 0;
	int uvindex_is = 0;
	int check19 = 0;
	int check98 = 0;
	int check99 = 0;
	int check10 = 0;
	/* Never ending loop of server */
	while(running == 1) {

		if (mySwitch.available()) {
    
        	int value = mySwitch.getReceivedValue();

        	if (value == 0) {
          		printf("Unknown encoding\n");
        	} else {

				int transmitter_id = value/10000000;
				int sensor_id = (value/100000)%100;
				int measured_value = value%10000;

				//START of sending loop
				if ((value/10)%100 == 98 && transmitter_id + (value/10)%100 != check98) {
					check98 = transmitter_id + (value/10)%100;
					check99 = 0;
					continue;
				}

				if (check10 == 0 && (value/10)%100 != 98
				  || check10 == 0 && (value/10)%100 != 99) {

					//Do not try to get transmitter ID in start or and of transmition loop
					check10 = transmitter_id;
				}

				if (sensor_id == 11 && check11 != transmitter_id + sensor_id) {
					temp_dht_hic = measured_value;
					check11 = transmitter_id + sensor_id;
					continue;
				}

				if (sensor_id == 12 && check12 != transmitter_id + sensor_id) {
					temp_dht_hif = measured_value;
					check12 = transmitter_id + sensor_id;
					continue;
				} 

				if (sensor_id == 13 && check13 != transmitter_id + sensor_id) {
					humidity_dht = measured_value;
					check13 = transmitter_id + sensor_id;
					continue;
				} 

				if (sensor_id == 14 && check14 != transmitter_id + sensor_id) {
					pressure_bmp = measured_value;
					check14 = transmitter_id + sensor_id;
					continue;
				} 

				if (sensor_id == 15 && check15 != transmitter_id + sensor_id) {
					altitude_bmp = measured_value;
					check15 = transmitter_id + sensor_id;
					continue;
				} 

				if (sensor_id == 16 && check16 != transmitter_id + sensor_id) {
					temp_bmp = measured_value;
					check16 = transmitter_id + sensor_id;
					continue;
				} 

				if (sensor_id == 17 && check17 != transmitter_id + sensor_id) {
					vcc_atmega = measured_value;
					check17 = transmitter_id + sensor_id;
					continue;
				}

				if (sensor_id == 18 && check18 != transmitter_id + sensor_id) {
					vis_is = measured_value;
					check18 = transmitter_id + sensor_id;
					continue;
				}

				if (sensor_id == 19 && check19 != transmitter_id + sensor_id) {
					uvindex_is = measured_value;
					check19 = transmitter_id + sensor_id;
					continue;
				}

				//END of sending loop
				if ((value/10)%100 == 99 && transmitter_id + (value/10)%100 != check99) {

					//Writes collected datas in database
					mysql_connect();					
						mysql_write(check10,temp_dht_hic,temp_dht_hif,humidity_dht,pressure_bmp,altitude_bmp,temp_bmp,vcc_atmega,vis_is,uvindex_is);						
					mysql_disconnect();

					//Reset checks for next loop
					check10 = 0;
					check11 = 0;
					check12 = 0;
					check13 = 0;
					check14 = 0;
					check15 = 0;
					check16 = 0;
					check17 = 0;
					check18 = 0;
					check19 = 0;
					check98 = 0;
					check99 = transmitter_id + (value/10)%100;
				} else {
					continue;
				}
			}   
        }
    
        mySwitch.resetAvailable();

		/**
		 * Debug print
		 */
		if(ret < 0) {
			syslog(LOG_ERR, "Can not write to log stream: %s, error: %s",
				(log_stream == stdout) ? "stdout" : log_file_name, strerror(errno));
			break;
		}
		ret = fflush(log_stream);
		if(ret != 0) {
			syslog(LOG_ERR, "Can not fflush() log stream: %s, error: %s",
				(log_stream == stdout) ? "stdout" : log_file_name, strerror(errno));
			break;
		}

		//sleep(loop);
	}

	/* Close log file, when it is used. */
	if (log_stream != stdout)
	{
		fclose(log_stream);
	}

	/* Write system log and close it. */
	syslog(LOG_INFO, "Stopped %s", app_name);
	closelog();

	/* Free allocated memory */
	if(conf_file_name != NULL) free(conf_file_name);
	if(log_file_name != NULL) free(log_file_name);
	if(pid_file_name != NULL) free(pid_file_name);

	return EXIT_SUCCESS;
}
