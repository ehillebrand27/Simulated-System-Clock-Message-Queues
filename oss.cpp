
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <iostream>
#include <string>
#include <signal.h>
#include "message.h"

struct Clock
{
    int seconds = 0;
    int nanoseconds = 0;
};

const int BUFF_SZ = sizeof(Clock);

int shm_key;
int shm_id = -1;
Clock *sim_clock = nullptr;
int msg_id = -1;

void cleanup()
{
    if (msg_id != -1)
    {
        msgctl(msg_id, IPC_RMID, nullptr);
        msg_id = -1;
    }

    if (sim_clock != nullptr)
    {
        shmdt(sim_clock);
        sim_clock = nullptr;
    }

    if (shm_id != -1)
    {
        shmctl(shm_id, IPC_RMID, nullptr);
        shm_id = -1;
    }
}

int main(int argc, char *argv[])
{
    int c;

    // Default command-line values.
    int n = 5;
    float t = 3.0;
    int s = 7;
    float i = 0.1;

    while ((c = getopt(argc, argv, "hn:s:t:i:")) != -1)
    {
        switch (c)
        {
        case 'h':
            std::cout
                << "Usage: ./oss [-h] [-n proc] [-s simul] "
                << "[-t timelimitForChildren] "
                << "[-i intervalInSecondsToLaunchChildren]"
                << std::endl;
            return 0;

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
            return 1;
        }
    }

    if (n < 1 || s < 1 || t <= 0 || i < 0)
    {
        std::cerr << "Invalid argument values." << std::endl;
        return 1;
    }

    // Create and attach shared memory.
    shm_key = ftok("oss.cpp", 0);

    if (shm_key == -1)
    {
        perror("oss ftok shared memory");
        return 1;
    }

    shm_id = shmget(shm_key, BUFF_SZ, IPC_CREAT | 0666);

    if (shm_id == -1)
    {
        perror("oss shmget");
        return 1;
    }

    sim_clock = (Clock *)shmat(shm_id, nullptr, 0);

    if (sim_clock == (void *)-1)
    {
        perror("oss shmat");
        sim_clock = nullptr;
        cleanup();
        return 1;
    }

    // Temporary initial clock values for testing.
    sim_clock->seconds = 6;
    sim_clock->nanoseconds = 7;

    // Create the message queue.
    key_t msg_key = ftok("oss.cpp", 1);

    if (msg_key == -1)
    {
        perror("oss ftok message queue");
        cleanup();
        return 1;
    }

    msg_id = msgget(msg_key, IPC_CREAT | IPC_EXCL | 0666);

    if (msg_id == -1)
    {
        perror("oss msgget");
        cleanup();
        return 1;
    }

    // Launch one worker for this milestone.
    pid_t child_pid = fork();

    if (child_pid == -1)
    {
        perror("oss fork");
        cleanup();
        return 1;
    }

    if (child_pid == 0)
    {
        execl("./worker", "./worker", "5", "500000",
              (char *)nullptr);

        perror("oss execl");
        _exit(1);
    }

    // Repeatedly communicate with the worker until it expires.
    int status = 1;

    while (status == 1)
    {
        message msg;
        msg.mtype = 1;
        msg.number = 42;

        if (msgsnd(msg_id, &msg, sizeof(msg.number), 0) == -1)
        {
            perror("oss msgsnd");
            kill(child_pid, SIGTERM);
            waitpid(child_pid, nullptr, 0);
            cleanup();
            return 1;
        }

        std::cout << "OSS: Sent message to worker PID "
                  << child_pid << std::endl;

        message reply;

        if (msgrcv(msg_id, &reply, sizeof(reply.number), 2, 0) == -1)
        {
            perror("oss msgrcv");
            kill(child_pid, SIGTERM);
            waitpid(child_pid, nullptr, 0);
            cleanup();
            return 1;
        }

        status = reply.number;

        std::cout << "OSS: Worker PID " << child_pid
                  << " returned status " << status << std::endl;

        // Advance simulated time by 250 ms for this one worker.
        sim_clock->nanoseconds += 250000000;

        if (sim_clock->nanoseconds >= 1000000000)
        {
            sim_clock->seconds++;
            sim_clock->nanoseconds -= 1000000000;
        }

        std::cout << "OSS: Simulated time "
                  << sim_clock->seconds << " seconds, "
                  << sim_clock->nanoseconds << " nanoseconds"
                  << std::endl;
    }

    // Wait for the worker to exit.
    waitpid(child_pid, nullptr, 0);

    std::cout << "OSS: Worker has terminated." << std::endl;

    cleanup();

    return 0;
}
