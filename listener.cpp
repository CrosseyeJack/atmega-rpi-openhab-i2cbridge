#include "listener.h"

bool got_interrupt = false;

char ic2data[255];
char payload_data[0xDF];

using namespace std;

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
#ifdef DEBUG_PRINT
		std::cout << "Entered thread loop, locking." << std::endl;
#endif

		// Lock the loop until we get the interrupt
		std::unique_lock<std::mutex> lkListener(mListener);
		cvListener.wait(lkListener, []{return got_interrupt;});

		// Lock released
#ifdef DEBUG_PRINT
		std::cout << "Exited lock." << std::endl;
#endif

		// reset the interrupt flag
		got_interrupt = false;

		// Header Check
		int headercheck[16] = {0xDE, 0xAD, 0xBE, 0xEF, 0x42, 0x48, 0x48, 0x47,
			0x54, 0x54, 0x47, 0x42, 0xDE, 0xAD, 0xBE, 0xEF};

		for (int i=0; i <=15; i++) {
			if (!(char)wiringPiI2CReadReg8(fd,i)==headercheck[i]) {
				// Header check. If head doesn't match what is expected.
#ifdef DEBUG_PRINT
				std::cout << "Error Reading Header..." << std::endl;
#endif
				wiringPiI2CWriteReg8(fd,0xFF,0xFF);	// Release the Radio
				continue;	// Jump back to the top of the thread loop
			}
		}

		// Get the payload size
		int payload_size = (char)wiringPiI2CReadReg8(fd,def_payload_size);

		// Do a simple sanity check on the payload size
		if (payload_size >= 0xDF) {
			// payload too large
#ifdef DEBUG_PRINT
			std::cout << "PayLoad Too Large: " << payload_size << std::endl;
			printf("\a");
#endif
			wiringPiI2CWriteReg8(fd,0xFF,0xFF);	// Release the Radio
			continue;	// Jump back to the top of the thread loop
		}

		// Read out the payload
		int read_attempt = 0;
		read_i2c:
		read_attempt++;
		// Wipe the payload_data
		for (int i=0; i<=0xDF;i++) payload_data[i] = 0xFF;

		bool dataOK = true;	// simple Data OK Flag
		// Read out the payload and put it in a char array
		for (int i = 0; i <= payload_size; i++) {
			payload_data[i] = (char)wiringPiI2CReadReg8(fd,i+0x20);
			if (payload_data[i]==0x00 && i < payload_size) 
					dataOK = false;
		}
		if (!dataOK && read_attempt <= 3) { // re-read the data buffer
			goto read_i2c;
		} else if (!dataOK && read_attempt >=3) { // The read failed 3 times
#ifdef DEBUG_PRINT
			std::cout << "Bad Read..." << std::endl;
			printf("\a");
#endif
			wiringPiI2CWriteReg8(fd,0xFF,0xFF);	// Release the Radio
			continue;	// Jump back to the top of the thread loop
		}

#ifdef DEBUG_PRINT
		// Dump the payload to console
		std::cout << "Payload Size: " << payload_size << " Payload: ";
		for (int i = 0; i <= payload_size; i++) {
			std::cout << payload_data[i];
		}
		std::cout << std::endl;
#endif


#ifdef DEBUG_PRINT
		// print out the data
		// Leaving this in for now but its will need to be removed for the daemon
		// infact why not just surround this with a ifdef?
		std::cout << "Data:" << std::endl;
		for (int i = 0; i < 0xDF; i+=16) {
			for (int i2 = 0; i2 < 16; i2++) {
				int hex = payload_data[i+i2];
				if (payload_data[i+i2]==0x00 && (i+i2)<payload_size) {
					dataOK = false;
				}
				std::cout << std::hex << "0x" << std::uppercase << std::setfill('0') << std::setw(2) << hex << std::nouppercase << std::dec << " ";
			}
			std::cout << std::endl;
		}
		if (!dataOK) {
			printf("\a");
			std::cout << "Data is bad... Re-read" << std::endl;
		}
#endif

		// extract the RSSI Data
		unsigned short rssi = (char)wiringPiI2CReadReg8(fd,def_rssi_low) 
			| ((char)wiringPiI2CReadReg8(fd,def_rssi_high)<<8);

		// extract LQI
		unsigned short lqi = (char)wiringPiI2CReadReg8(fd,def_lqi_low)
			| ((char)wiringPiI2CReadReg8(fd,def_lqi_high)<<8);

		// extract sender address
		unsigned short sender_address = (char)wiringPiI2CReadReg8(fd,def_sender_address_low) 
			| ((char)wiringPiI2CReadReg8(fd,def_sender_address_high)<<8);

		// extract board address (not sure how I will use this, but might aswell extract it)
		unsigned short board_address = (char)wiringPiI2CReadReg8(fd,def_board_address_low)
			| ((char)wiringPiI2CReadReg8(fd,def_board_address_high)<<8);

		// extract pan address
		unsigned short pan_address = (char)wiringPiI2CReadReg8(fd,def_pan_address_low)
			| ((char)wiringPiI2CReadReg8(fd,def_pan_address_high)<<8);

#ifdef DEBUG_PRINT
		std::cout<<"Sender: "<<std::hex<<sender_address<<std::dec<<" RSSI/LQI: "<<rssi<<"/"<<lqi<<" Payload size: "<<payload_size<<std::endl;
#endif
		
		wiringPiI2CWriteReg8(fd,0xFF,0xFF); // unlock radio
		continue;
		
		// Extract the payload
		char payload[payload_size+1];
		for (int i = 0; i <= payload_size; i++) {
			payload[i] = ic2data[i+0x20];
		}
		// Due to a bug in the bridge micro (which I need to fix) the first transmission returns 0's
		// check the first 3 byes for 0's and if they are there disguard the payload
		if (payload[0]==0&&payload[1]==0&&payload[2]==0) {
			// Invalid payload
#ifdef DEBUG_PRINT
			std::cout << "Invalid payload" << std::endl;
#endif
			continue;
		}
		// SO at this point I have all the data I need from the micro. I can now pass that on to OpenHab
		// Time to go and re-read the openHAB API
		// No I don't have all the data I need. I had all the data I needed for the TEST.
		
		// I need to split the payload data up.
		// For the sake for simplisty dump the payload into a string object
		std::string str_payload = std::string(payload);
		/* to count how often the terminator char occurs */
		int count = 0;
		size_t offset = 0;

		while((offset = str_payload.find(';',offset)) != string::npos) {
			count++;
			offset++;
		}
		
		// A quick sanity check on count
		// TODO need to preform more checks and purposely send the program invalid code to catch it out
		// Otherwise we will get segemntation faults later on in the code.
		if (count == 0) {
			// count didn't increase
			printf("\a");
#ifdef DEBUG_PRINT
			std::cout << "Error in payload" << std::endl;
#endif
			continue;
		}
		
#ifdef DEBUG_PRINT
		std::cout << "Payload: " << str_payload 
				<< " (';' appears in payload " << count << " times)" << std::endl;
#endif
		// Create string objects to hold the payload
		// TODO I should put in a limit on the number of strings that could be created
		string arr_payload[count];
		
		string split_string;
		stringstream stream(str_payload);
		count = 0;
		while( getline(stream, split_string, ';') )
		{
			arr_payload[count++] = split_string;
		}
		//count = sizeof(arr_payload)/sizeof(arr_payload[0])-1;
		count = count-1;
		for (int i=0; i <= count;i++) {
			size_t found;
			if ((found = arr_payload[i].find(":")) != string::npos) {
				string pin = arr_payload[i].substr(0,found);
				string data = arr_payload[i].substr(found+1, string::npos);
#ifdef DEBUG_PRINT
				cout << "arr_payload[" << i << "] left side = " << pin 
						<< ", right side = " << data << endl;
#endif
				// Lets do some sanity checks on the data before handing it off to OpenHAB
				// Check pin string's length
				if (pin.length()!=2 && pin.length()!=3) {
#ifdef DEBUG_PRINT
					std::cout << "Pin Data Length is invalid" << std::endl;
#endif
					continue;
				}

				// Check the first char for either A or D
				if(pin.at(0) != 'A' && pin.at(0) != 'D') {
#ifdef DEBUG_PRINT
					std::cout << "First char is bad" << std::endl;
#endif
					continue;
				}
				// Now we have the pin id and the data for that pin, next its time to pass it off to OpenHAB
				rest_api_post(sender_address, pin, data);
			}
		}
		continue;
	}
}

void i2cbridge_interrupt(void) {
#ifdef DEBUG_PRINT
	std::cout << "interrupt triggered.." << std::endl;
#endif
	got_interrupt = true;
	cvListener.notify_one();
}

// For the moment I have just shoved the rest api code down here.
// I am going to put it in a function simply because I may need to call it a number of times
// every time we get a data broadcast.

// OK so some reading, testing, dicking about, eating, etc i think for adding data to OpenHAB I'm
// going to use libcurl to send the rest api calls.
// Basically calling somthing like:-
// curl -H "Content-Type: text/plain" -d "<DATA>" http://openHABhost:8080/rest/items/<ITEM>
// set <DATA> on <ITEM> Seems easy enough
// for secuirty I can always bind the API address to localhost only.
// I can also use the basic auth built into OpenHAB
// For this test I will be "hardcoding" a lot of things but I will just need to filter this data out

// I need to handle payload. For now I am just going to strip the first 2 and the last chars from it
// I will need to break the payload into its multiple parts which are seperated by ;'s
int rest_api_post (short sender_address, string pin_id, string data) {
// Create the REST API String for the item
	std::string openhaburl = "http://openhab:8080/rest/items/";	// Base URL, should put this in a config file
	std::ostringstream s_item;	// String stream for putting the url together
	s_item << openhaburl << "fmk_" << std::hex << sender_address << "_" << pin_id;
	std::string itemurl = s_item.str();
#ifdef DEBUG_PRINT
	std::cout << "Item URL: " << itemurl << std::endl;
#endif

	CURL *curl;
	CURLcode curl_return;
	struct curl_slist *headers=NULL;
	curl = curl_easy_init();
	if (!curl) {
		//Unable to init curl - need to handle this
#ifdef DEBUG_PRINT
		std::cout<<"Failed to init curl"<<std::endl;
#endif
		return 0;
	}
	headers = curl_slist_append(headers, "Content-Type: text/plain");
	curl_easy_setopt(curl, CURLOPT_URL, itemurl.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
	curl_return = curl_easy_perform(curl); /* post away! */
	curl_slist_free_all(headers); /* free the header list */
	if (curl_return != CURLE_OK) {
#ifdef DEBUG_PRINT
//		std::cout<<"Something went wrong with curl... "<<curl_easy_strerror(curl_return)<<std::endl;
		std::cout<<"Something went wrong with curl... "<<std::endl;
#endif
		return 0;
	}
	curl_easy_cleanup(curl);
	curl_global_cleanup();
	return 1;
}
