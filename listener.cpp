#include "listener.h"

#include <stdbool.h>
#include <mutex>
#include <condition_variable>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <iostream>
#include <iomanip>      // std::setfill, std::setw

bool got_interrupt = false;

char ic2data[255];

#define INT_I2CBRIDGE 17		// Pin for the i2cbridge interrupt
#define RST_I2CBRIDGE 27		// GPIO to reset the micro

#define DEBUG_PRINT

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

// std::mutex m;
std::mutex mListener;
std::condition_variable cvListener;

void worker_thread_listener() {
	// Reset the micro
#ifdef DEBUG_PRINT
	std::cout << "Reseting Micro..." << std::endl;
#endif
	digitalWrite(RST_I2CBRIDGE, LOW);	// Send the Reset pin of the micro low
	delay(1);							/* 1ms should be good enough
										 * the datasheet says reset should be a
										 * min of 2.5µs on reset (0.0025ms) */
	digitalWrite(RST_I2CBRIDGE,HIGH);	// Send the Reset pin of the micro high
	
	// Setup the interrupt for the i2cbridge
	wiringPiISR(INT_I2CBRIDGE, INT_EDGE_RISING, &i2cbridge_interrupt);
	
	int fd = wiringPiI2CSetup(0x42);
	if (fd == -1) {
		//something when wrong with opening the i2c bus
		std::cout << "Something went wrong: " << errno << std::endl;
		return;
	} else {
		std::cout << "Created i2c fd." << std::endl;
	}
	
	// Thread Loop
	while(1) {
		std::cout << "Entered thread loop, locking." << std::endl;
		// Lock the loop until we get the interrupt
		std::unique_lock<std::mutex> lkListener(mListener);
		cvListener.wait(lkListener, []{return got_interrupt;});
		std::cout << "Exited lock." << std::endl;
		// reset the interrupt flag
		got_interrupt = false;
		
		// Read the i2c data buffer
		// Really I could read the value of 0x1F add that value to 0x20 and read
		// from 0x00 to that value. And I will prob change this to do that later
		for (int i = 0x00; i <= 0xFF; i++) {
			ic2data[i] = (char)wiringPiI2CReadReg8(fd,i);
		}
		// Tell the bridge to flush the data, release both the interrupt and radio
		wiringPiI2CWriteReg8(fd,0xFF,0xFF);
		
		// print out the data
		// Leaving this in for now but its will need to be removed for the daemon
		// infact why not just surround this with a ifdef?
#ifdef DEBUG_PRINT
		std::cout << "Data:" << std::endl;
		for (int i = 0; i < 256; i+=16) {
			for (int i2 = 0; i2 < 16; i2++) {
				int hex = ic2data[i+i2];
				std::cout << std::hex << "0x" << std::uppercase << std::setfill('0') << std::setw(2) << hex << std::nouppercase << std::dec << " ";
			}
			std::cout << std::endl;
		}
#endif
		
		// Lets start formating this data
		
		// 0xDE 0xAD 0xBE 0xEF 0x42 0x48 0x48 0x47 0x54 0x54 0x47 0x42 0xDE 0xAD 0xBE 0xEF
		// The expected header from the i2c bridge micro
		int headercheck[16] = {0xDE, 0xAD, 0xBE, 0xEF, 0x42, 0x48, 0x48, 0x47,
			0x54, 0x54, 0x47, 0x42, 0xDE, 0xAD, 0xBE, 0xEF};
		bool headcheck = true;
		for (int i = 0; i < 16; i++) {
			// Copy out the header
			if (ic2data[i]!=headercheck[i]) headcheck = false;
		}
		if (!headcheck) {
#ifdef DEBUG_PRINT
			std::cout << "Header Check Failed" << std::endl;
#endif
			// Stop processing data - probally need to clean up the vars
			continue;
		}
#ifdef DEBUG_PRINT
		std::cout << "Header check passed" << std::endl;
#endif
		// Get the payload size
		// I still need to grab stuff like sender address/LQI/RSSI
		int payload_size = ic2data[0x1F];
		// Need to do some sanity checks on the payload size
		std::cout << "Payload Size: " << payload_size << std::endl;
		if (payload_size >= 0xDF) {
			// payload too large
#ifdef DEBUG_PRINT
			std::cout << "Too Large Payload" << std::endl;
#endif
			continue;
		}
		
		// extract the RSSI Data
		unsigned short rssi = ic2data[def_rssi_low] | (ic2data[def_rssi_high]<<8);
#ifdef DEBUG_PRINT
		std::cout << "rssi: " << rssi << std::endl;
#endif
		
	}
}

void i2cbridge_interrupt(void) {
	std::cout << "interrupt triggered.." << std::endl;
	got_interrupt = true;
	cvListener.notify_one();
}