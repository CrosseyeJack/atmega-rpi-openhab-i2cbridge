#include "listener.h"

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


using namespace std;

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
		// Get the payload size
		// I still need to grab stuff like sender address/LQI/RSSI
		int payload_size = ic2data[0x1F];
		// Need to do some sanity checks on the payload size
		if (payload_size >= 0xDF) {
			// payload too large
#ifdef DEBUG_PRINT
			std::cout << "Too Large Payload" << std::endl;
#endif
			continue;
		}
		
		// extract the RSSI Data
		unsigned short rssi = ic2data[def_rssi_low] | (ic2data[def_rssi_high]<<8);
		// extract LQI
		unsigned short lqi = ic2data[def_lqi_low] | (ic2data[def_lqi_high]<<8);
		// extract sender address
		unsigned short sender_address = ic2data[def_sender_address_low] | (ic2data[def_sender_address_high]<<8);
		// extract board address (not sure how I will use this, but might aswell extract it)
		unsigned short board_address = ic2data[def_board_address_low] | (ic2data[def_board_address_high]<<8);
		// extract pan address
		unsigned short pan_address = ic2data[def_pan_address_low] | (ic2data[def_pan_address_high]<<8);
#ifdef DEBUG_PRINT
		std::cout<<"Sender: "<<std::hex<<sender_address<<std::dec<<" RSSI/LQI: "<<rssi<<"/"<<lqi<<" Payload size: "<<payload_size<<std::endl;
#endif
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
		int count = -0;
		size_t offset = 0;

		while((offset = str_payload.find(';',offset)) != string::npos) {
			count++;
			offset++;
		}
		
		// A quick sanity check on count
		// TODO need to preform more checks and purposely send the program invalid code to catch it out
		// Otherwise we will get segemntation faults later on in the code.
		if (count == -1) {
			// count didn't increase
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
#ifdef DEBUG_PRINT
				cout << "arr_payload[" << count << "] left side  = " << arr_payload[i].substr(0,found) << endl;
				cout << "arr_payload[" << count << "] right side = " << arr_payload[i].substr(found+1, string::npos) << endl;
#endif
				// Now we have the pin id and the data for that pin, next its time to pass it off to OpenHAB
			}
		}
		
		
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
//		char *temp_test = (char*) malloc(payload_size-4);
//		strncpy(temp_test, payload+3, payload_size-4);
		
		
		// ATM I don't want to submit the WHOLE payload to OpenHAB
		// So I am cheating and just going to skip over this part with a continue;
		continue;	
		
		
	}
}

void i2cbridge_interrupt(void) {
	std::cout << "interrupt triggered.." << std::endl;
	got_interrupt = true;
	cvListener.notify_one();
}

// For the moment I have just shoved the rest api code down here.
// I am going to put it in a function simply because I may need to call it a number of times
// every time we get a data broadcast.
int rest_api_post (string sender_address, string pin_id, string data) {
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
		std::cout<<"Something went wrong with curl... "<<curl_easy_strerror(curl_return)<<std::endl;
#endif
		return 0;
	}
	curl_easy_cleanup(curl);
	curl_global_cleanup();
	return 1;
}