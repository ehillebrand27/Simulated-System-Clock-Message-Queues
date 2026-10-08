#include <stdio.h>

#include <stdlib.h>

#include <unistd.h>

#include <sys/types.h>

#include <sys/ipc.h>

#include <sys/shm.h>

#include <iostream>

#include <string>


//structure of the clock, will be in shared memory 
struct Clock
{

    int seconds = 0;
    int nanoseconds = 0;

};


const int BUFF_SZ = sizeof(Clock); //size of the shared memory segment
int shm_key; //shared memory key
int shm_id; //shared memory id
volatile Clock *sim_clock; //pointer to the Clock structure in shared memory (volatile so it is re-read every loop)


int main(int argc, char *argv[])
{

    //input check
    if (argc < 3)
    {
        std::cout << "Usage: ./worker <seconds> <nanoseconds>\n" << std::endl;
        exit(1);
    }



    shm_key = ftok("oss.cpp",0);
    if (shm_key == -1)
    {
        fprintf(stderr,"Child:... Error in ftok\n");
        exit(1);
    }

    shm_id = shmget(shm_key,BUFF_SZ,0666);
    if (shm_id == -1) 
    {
        perror("worker shmget");
        exit(1);
    }


    //attach the shared memory segment and get a pointer to it as a Clock
    sim_clock = (volatile Clock *)shmat(shm_id, 0, 0);

    if (sim_clock == (void *)-1)
    {
        perror("worker shmat");
        exit(1);
    }


    //milestone 3- check if one worker can can read the clock 
    std::cout << "seconds: " << sim_clock->seconds << " nanoseconds: " << sim_clock->nanoseconds << std::endl;


    return 0;
}