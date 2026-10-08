
#include <stdio.h>

#include <stdlib.h>

#include <unistd.h>

#include <sys/types.h>

#include <sys/ipc.h>

#include <sys/shm.h>

#include <iostream>

#include <sys/wait.h>

#include <time.h>

#include <signal.h>

#include <string.h>

#include <string>



//structure of the clock, will be in shared memory 
struct Clock
{

    int seconds = 0;
    int nanoseconds = 0;

};



//clock/shared memory intial values
const int BUFF_SZ = sizeof(Clock); //size of the shared memory segment
int shm_key; //shared memory key
int shm_id = -1; //shared memory id
Clock *sim_clock = nullptr; //pointer to the Clock structure in shared memory



int main(int argc, char *argv[])
{


    int c;

    //default values
    int n = 5;
    float t = 3.0;
    int s = 7;
    float i = 0.1;

    //parse command line arguments 
    while ((c = getopt(argc, argv, "hn:s:t:i:")) != -1)
    {

        switch (c)
        {
        case 'h':
            std::cout << "Usage: ./oss [-h] [-n proc] [-s simul] [-t timelimitForChildren] [-i intervalInSecondsToLaunchChildren]" << std::endl;
            exit(0);

        case 'n':
            n = std::stoi(optarg);
            break;

        case 't':
            t = std::stof(optarg);
            break;

        case 's':
            s = std::stoi(optarg);
            break;

        case 'i':
            i = std::stof(optarg);
            break;
            
        
        default:
            break;
        }

    }

    //basic argument checks
    if (n < 1 || s < 1 || t <= 0 || i < 0)
    {
        std::cerr << "Invalid argument values." << std::endl;
        exit(1);
    }




    //generate a key used to identify the shared memory segment
    shm_key = ftok("oss.cpp",0);

    if (shm_key == -1) 
    {
        fprintf(stderr,"Parent:... Error in ftok\n");
        exit(1);
    }


    //create or get the shared memory segment and store its ID
    shm_id = shmget( shm_key , sizeof(Clock) , IPC_CREAT | 0666  );

    if (shm_id == -1) 
    {

        fprintf(stderr,"Shared memory get failed\n");

        exit(1);

    }



    //attach the shared memory segment and get a pointer to it as a Clock
    sim_clock = (Clock *)shmat(shm_id, 0, 0);

    if (sim_clock == (void *)-1)
    {
        perror("oss shmat");
        shmctl(shm_id, IPC_RMID, NULL);
        exit(1);
    }

    
    //milestone 3 check- intialize the clock to 6, 7
    sim_clock->seconds = 6;
    sim_clock->nanoseconds = 7;

    //milestone 3- infinite loop to keep shared memory alive 
    //so worker can run in another terminal and see if it can access it
    bool run = true;
    while(run);




    return 0;
}