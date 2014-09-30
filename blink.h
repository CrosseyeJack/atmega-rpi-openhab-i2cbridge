/* 
 * File:   blink.h
 * Author: srv
 *
 * Created on 17 September 2014, 02:28
 */

#ifndef BLINK_H
#define	BLINK_H

/* 
 * File:   blink.cpp
 * Author: srv
 * 
 * Created on 17 September 2014, 01:33
 */

#define LED_FLASH_LOOP  2000		// Time between the start of each LED flash
#define LED_FLASH_TIME  1000		// Time the LED is on for each flash
#define LED_RUNNING_PIN	4			// OUTPUT - 7

#include <wiringPi.h>
#include <unistd.h>

// Simple thread to flash an LED for a visual confirmation that the application is running.
void worker_thread_blink() {
#ifdef DEBUG_PRINT
	std::cout << "Blinking..." << std::endl;
#endif
	for (;;) {
		digitalWrite(LED_RUNNING_PIN, HIGH);
		usleep(LED_FLASH_TIME * 1000);
		digitalWrite(LED_RUNNING_PIN, LOW);
		usleep((LED_FLASH_LOOP - LED_FLASH_TIME) * 1000);
	}
}


#endif	/* BLINK_H */

