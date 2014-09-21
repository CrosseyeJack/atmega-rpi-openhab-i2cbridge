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

// std::mutex m;
std::mutex mListener;
std::condition_variable cvListener;

void worker_thread_listener() {
	// Reset the micro
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
		std::cout << "Data:" << std::endl;
		for (int i = 0x00; i <= 0xFF; i++) {
			ic2data[i] = (char)wiringPiI2CReadReg8(fd,i);
		}
		// Tell the bridge to flush the data, release both the interrupt and radio
		wiringPiI2CWriteReg8(fd,0xFF,0xFF);
		
		// print out the data
		for (int i = 0; i < 256; i+=16) {
			for (int i2 = 0; i2 < 16; i2++) {
				int hex = ic2data[i+i2];
				std::cout << std::hex << "0x" << std::uppercase << std::setfill('0') << std::setw(2) << hex << std::nouppercase << std::dec << " ";
			}
			std::cout << std::endl;
		}
		
	}
}

void i2cbridge_interrupt(void) {
	std::cout << "interrupt triggered.." << std::endl;
	got_interrupt = true;
	cvListener.notify_one();
}