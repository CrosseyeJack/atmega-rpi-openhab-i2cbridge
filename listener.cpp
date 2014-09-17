#include "listener.h"

#include <stdbool.h>
#include <mutex>
#include <condition_variable>
#include <wiringPi.h>
#include <wiringPiI2C.h>
#include <iostream>
#include <iomanip>      // std::setfill, std::setw

bool got_interrupt = false;

uint8_t ic2data[255];

#define INT_I2CBRIDGE 17		// Pin for the i2cbridge interrupt

// std::mutex m;
std::mutex mListener;
std::condition_variable cvListener;

void worker_thread_listener() {
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
//		for (int i = 0; i <= 255; i++) {
//			ic2data[i] = wiringPiI2CReadReg8(fd,i);
//		}
		
		
		for (int i = 0; i < 256; i+=16) {
			for (int i2 = 0; i2 < 16; i2++) {
				std::cout << std::hex << "0x" << std::uppercase << std::setw(2) << wiringPiI2CReadReg8(fd,i+i2) << std::nouppercase << std::dec << " ";
			}
			std::cout << std::endl;
			
		}
		wiringPiI2CWriteReg8(fd,0xFF,0xFF);
		// Tell the bridge to flush the data, release both the interrupt and radio
		
	}
}

void i2cbridge_interrupt(void) {
	std::cout << "interrupt triggered.." << std::endl;
	got_interrupt = true;
	cvListener.notify_one();
}