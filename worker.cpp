
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <iostream>
#include <string>
#include "message.h"

struct Clock
{
    int seconds = 0;
    int nanoseconds = 0;
};

const int BUFF_SZ = sizeof(Clock);

int shm_key;
int shm_id = -1;
volatile Clock *sim_clock = nullptr;

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        std::cerr << "Usage: ./worker <seconds> <nanoseconds>"
                  << std::endl;
        return 1;
    }

    // Parse the worker's lifetime.
    int lifetime_seconds;
    int lifetime_nanoseconds;

    try
    {
        lifetime_seconds = std::stoi(argv[1]);
        lifetime_nanoseconds = std::stoi(argv[2]);
    }
    catch (const std::exception &e)
    {
        std::cerr << "Invalid lifetime arguments." << std::endl;
        return 1;
    }

    if (lifetime_seconds < 0 ||
        lifetime_nanoseconds < 0 ||
        lifetime_nanoseconds >= 1000000000)
    {
        std::cerr << "Invalid lifetime values." << std::endl;
        return 1;
    }

    // Attach to the shared simulated clock.
    shm_key = ftok("oss.cpp", 0);

    if (shm_key == -1)
    {
        perror("worker ftok shared memory");
        return 1;
    }

    shm_id = shmget(shm_key, BUFF_SZ, 0666);

    if (shm_id == -1)
    {
        perror("worker shmget");
        return 1;
    }

    sim_clock = (volatile Clock *)shmat(shm_id, nullptr, 0);

    if (sim_clock == (void *)-1)
    {
        perror("worker shmat");
        sim_clock = nullptr;
        return 1;
    }

    // Record start time.
    int start_seconds = sim_clock->seconds;
    int start_nanoseconds = sim_clock->nanoseconds;

    // Calculate termination time.
    int term_time_seconds = start_seconds + lifetime_seconds;
    int term_time_nanoseconds =
        start_nanoseconds + lifetime_nanoseconds;

    if (term_time_nanoseconds >= 1000000000)
    {
        term_time_seconds++;
        term_time_nanoseconds -= 1000000000;
    }

    std::cout << "WORKER PID:" << getpid()
              << " PPID:" << getppid() << std::endl;

    std::cout << "SysClockS: " << start_seconds
              << " SysclockNano: " << start_nanoseconds
              << " TermTimeS: " << term_time_seconds
              << " TermTimeNano: " << term_time_nanoseconds
              << std::endl;

    std::cout << "--Just Starting" << std::endl;

    // Connect to the existing message queue.
    key_t msg_key = ftok("oss.cpp", 1);

    if (msg_key == -1)
    {
        perror("worker ftok message queue");
        shmdt((const void *)sim_clock);
        return 1;
    }

    int msg_id = msgget(msg_key, 0666);

    if (msg_id == -1)
    {
        perror("worker msgget");
        shmdt((const void *)sim_clock);
        return 1;
    }

    // Wait for oss to request a lifetime check.
    while (true)
    {
        message msg;

        if (msgrcv(msg_id, &msg, sizeof(msg.number), 1, 0) == -1)
        {
            perror("worker msgrcv");
            shmdt((const void *)sim_clock);
            return 1;
        }

        // Read the current simulated time after receiving the message.
        int current_seconds = sim_clock->seconds;
        int current_nanoseconds = sim_clock->nanoseconds;

        bool expired =
            (current_seconds > term_time_seconds) ||
            (current_seconds == term_time_seconds &&
             current_nanoseconds >= term_time_nanoseconds);

        // Status 1 means still running; 0 means terminate.
        message reply;
        reply.mtype = 2;
        reply.number = expired ? 0 : 1;

        if (msgsnd(msg_id, &reply, sizeof(reply.number), 0) == -1)
        {
            perror("worker msgsnd");
            shmdt((const void *)sim_clock);
            return 1;
        }

        std::cout << "WORKER: Current time "
                  << current_seconds << " seconds, "
                  << current_nanoseconds << " nanoseconds; status = "
                  << reply.number << std::endl;

        if (expired)
        {
            break;
        }
    }

    shmdt((const void *)sim_clock);

    return 0;
}
