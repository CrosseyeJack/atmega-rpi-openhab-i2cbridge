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

// Just C+P'ed this from the bridge micro code and slapped def_ at the start
#define def_pan_address_low     0x10
#define def_pan_address_high    0x11
#define def_board_address_low   0x12
#define def_board_address_high  0x13
#define def_sender_address_low  0x14
#define def_sender_address_high 0x15
#define def_lqi_low             0x16
#define def_lqi_high            0x17
#define def_rssi_low            0x18
#define def_rssi_high           0x19

#define INT_I2CBRIDGE 17		// Pin for the i2cbridge interrupt
#define RST_I2CBRIDGE 27		// GPIO to reset the micro

// Flag to output debug messages
#define DEBUG_PRINT

using namespace std;

void i2cbridge_interrupt(void);
void worker_thread_listener();
int rest_api_post(short sender_address, string pin_id, string data);

#endif	/* LISTENER_H */

