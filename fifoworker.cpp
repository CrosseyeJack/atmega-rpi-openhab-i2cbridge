#include "fifoworker.h"
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#define MAX 100
#define FIFO_NAME "FIFO_PIPE"

void worker_thread_fifo() {
	std::cout << "Fifo Thread Started" << std::endl;
	while(1){
		int fd;
		char buf[MAX];
		int res;

		if (access(FIFO_NAME, F_OK) == -1) {
			res = mkfifo(FIFO_NAME, 0777);
			if (res != 0) {
				std::cout << "Could not create fifo" << std::endl;
				exit(EXIT_FAILURE);
			}
			std::cout<<"Created FIFO"<<std::endl;
		}

		//OPEN PIPE WITH READ ONLY
		std::cout << "Opening PIPE" <<std::endl;
		if ((fd = open (FIFO_NAME, O_RDONLY))<0)
		{
			std::cout << "Could not open named pipe for reading." << std::endl;
			exit(-1);
		}
		std::cout << "Data in pipe. Reading." << std::endl;

		//READ FROM PIPE
		if (read( fd,buf,MAX) < 0 )
		{
			std::cout << "Error reading pipe." << std::endl;
			exit(-1);
		}
		std::cout << "Pipe Data: " << buf << std::endl;

		if (close(fd)<0)
		{
			std::cout << "Error closing FIFO" << std::endl;
			exit(-1);
		}

		if (unlink(FIFO_NAME)<0)
		{
			std::cout << "Error deleting pipe file." << std::endl;
			exit(-1);
		}
		
		// Clear Buffer
		std::cout << "Clearing Buffer..." << std::endl;
		for (int i=0; i <= MAX; i++) {
			buf[i] = 0x00;
		}
	}
}