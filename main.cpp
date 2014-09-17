/* 
 * File:   main.cpp
 * Author: Daniel Williams
 *
 * Created on 16 September 2014, 16:14
 */

/*
 * Plan of action
 * OK so the plan is this:-
 * - Create a deamon application that waits for an interrupt from the i2c bridge
 * upon interrupt read the i2c data, process it and pass it onto openhab via it's
 * REST API - rinse and repeat
 * 
 * - Create a FIFO and listen for comms, When OpenHAB (or any other app) needs to
 * comminucate with the i2c bridge it will write to a FIFO which this app will read
 * process and send to the i2c bridge.
 * 
 * for quickness I am going am probbally going tot be using wiringPi for interrupt,
 * i2c, forking, etc
 * 
 * I could create a binding in OPENHAB to do this but I'm being lazy :-p
 */

#define DEBUG_PRINT

#include <cstdlib>
#include <wiringPi.h>	// wiringPi Libary (Used for Simple Threading/gpio access)
#include <wiringPiI2C.h>
#include <string.h>
#include <string>
#include <iostream>
#include <sys/stat.h>

#include <sys/signal.h>
#include <exception>

// main header file
#include "main.h"
#include "blink.h"
#include "listener.h"

// Threading Rebuild Includes
#include <thread>
#include <mutex>
#include <condition_variable>


using namespace std;

/*
 * Program entry point
 */
int main(int argc, char** argv) {
	
	// Set up the signal interrupts
	signal(SIGINT, SignalHandler); 
	signal(SIGTERM, SignalHandler);
	
	// Set up wiringPI
	wiringPiSetupSys(); 
	
	// Thread the LED blinker
	std::thread worker_led(worker_thread_blink);

	// Thread the "Listen to bridge" element
	
	// Forever loop
	while(1);
	return 0;
}


// Keyboard Interrupt Handler. To stop the main loop and allow the application to close gracefully.
void SignalHandler(int Reason) {
	//running = false;
#ifdef DEBUG_PRINT
	std::cout << "\r\nQuitting: " << Reason << std::endl;
#endif
	// TODO Shutdown the LED Flasher thread
	digitalWrite(LED_RUNNING_PIN, LOW);
	exit(EXIT_SUCCESS);
}

// Daemonise the application
static void daemonise(void) {
	pid_t pid, sid;

	/* already a daemon */
	if (getppid() == 1) return;

	/* Fork off the parent process */
	pid = fork();
	if (pid < 0) {
		exit(EXIT_FAILURE);
	}
	/* If we got a good PID, then we can exit the parent process. */
	if (pid > 0) {
		std::cout << "Forked into the background. (" << pid << ")" << std::endl;
		exit(EXIT_SUCCESS);
	}

	/* At this point we are executing as the child process */

	/* Change the file mode mask */
	umask(0);

	/* Create a new SID for the child process */
	sid = setsid();
	if (sid < 0) {
		exit(EXIT_FAILURE);
	}

	/* Change the current working directory.  This prevents the current
	   directory from being locked; hence not being able to remove it. */
	if ((chdir("/")) < 0) {
		exit(EXIT_FAILURE);
	}
	
	/* Close out the standard file descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);
	std::cout << "Daemonized the app" << std::endl;
}