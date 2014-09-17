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

void i2cbridge_interrupt(void);
void worker_thread_listener();

#endif	/* LISTENER_H */

