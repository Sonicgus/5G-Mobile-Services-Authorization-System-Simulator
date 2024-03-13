// Gustavo Samuel de Alves e Bastos de André e Lima 2020217743
// João Maria Pereira Carvalho de Picado Santos 2022213725

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

// #define DEBUG //remove this line to remove debug messages
#define LOG_FILENAME "log.txt"

FILE *log_fp;
pid_t system_manager_pid, authorization_request_manager_pid, monitor_engine_pid;

void start(const char *config_file)
{
    // open log to append
    log_fp = fopen(LOG_FILENAME, "a");

    if (log_fp == NULL)
    {
        perror("Error: opening log file");
        exit(1);
    }
}

void finish()
{
    // close log file
    if (log_fp)
        fclose(log_fp);
}

void *receiver(void *arg)
{
    pthread_exit(NULL);
}

void *sender(void *arg)
{
    pthread_exit(NULL);
}

void authorization_request_manager()
{
    pthread_t receiver_t, sender_t;

    // create the Receiver thread
    if (pthread_create(&receiver_t, NULL, receiver, NULL))
    {
        perror("Error: Creating receiver thread");
        exit(1);
    }

    // create the Sender thread
    if (pthread_create(&sender_t, NULL, sender, NULL))
    {
        perror("Error: Creating sender thread");
        exit(1);
    }

    // wait for the threads to finish
    if (pthread_join(sender_t, NULL))
    {
        perror("Error: waiting for sender thread to finish");
        exit(1);
    }
    if (pthread_join(receiver_t, NULL))
    {
        perror("Error: waiting for receiver thread to finish");
        exit(1);
    }

    exit(0);
}

void monitor_engine()
{
    exit(0);
}

void system_manager()
{
    system_manager_pid = getpid();

    // create the Authorization Request Manager process
    authorization_request_manager_pid = fork();

    if (authorization_request_manager_pid < 0)
    {
        perror("Error: creating process Authorization Request Manager\n");
        return;
    }
    else if (authorization_request_manager_pid == 0)
    {
        authorization_request_manager();
    }

    // create the Monitor Engine process
    monitor_engine_pid = fork();

    if (monitor_engine_pid < 0)
    {
        perror("Error: creating process Monitor Engine\n");
        return;
    }
    else if (monitor_engine_pid == 0)
    {
        monitor_engine();
    }
}

int main(int argc, char *argv[])
{
    // check if the number of parameters is correct
    if (argc != 2)
    {
        printf("Error: Wrong parameters, use ./5g_auth_platform {config-file}\n");
        exit(1);
    }

    // start the simulation
    start(argv[1]);

    system_manager();

    // finish the simulation
    finish();

    exit(0);
}
