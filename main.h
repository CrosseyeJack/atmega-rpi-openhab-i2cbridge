/* 
 * File:   main.h
 * Author: srv
 *
 * Created on 17 September 2014, 01:14
 */

#ifndef MAIN_H
#define	MAIN_H

void SignalHandler(int Reason); // Signal interupt handle
static void daemonise(void);	// daemonise the application

// Config Defines
#define CONFIG_FILE		"i2cbridge.ini"
#define CONFIG_MAIN		"main"
#define CONFIG_DAEMON	"daemonise"
#define CONFIG_BASEURL	"baseurl"

#include "../../simpleini/SimpleIni.h"

#endif	/* MAIN_H */

