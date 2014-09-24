/* 
 * File:   listener.h
 * Author: srv
 *
 * Created on 17 September 2014, 01:30
 */

// This thread waits for an interrupt from the i2c bridge, reads it out,
// processes it then hands of that the data to openHAB

#ifndef LISTENER_H
#define	LISTENER_H

#include <stdbool.h>
#include <mutex>
#include <condition_variable>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <iostream>
#include <iomanip>      // std::setfill, std::setw
#include <curl/curl.h>
#include <string.h>
#include <string>
#include <sstream>

using namespace std;

void i2cbridge_interrupt(void);
void worker_thread_listener();
int rest_api_post(short sender_address, string pin_id, string data);

#endif	/* LISTENER_H */

