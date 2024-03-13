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

// Struct to store the configuration
typedef struct
{
    int QUEUE_POS, AUTH_SERVERS_MAX, AUTH_PROC_TIME, MAX_VIDEO_WAIT, MAX_OTHERS_WAIT;
} Config;

Config config;

void print_log(const char *string)
{
    time_t t;
    struct tm *tm_ptr;
    // get the time
    t = time(NULL);
    tm_ptr = localtime(&t);

    if (tm_ptr == NULL)
    {
        perror("Error: getting local time struct");
        exit(1);
    }

    printf("%02d:%02d:%02d %s\n", tm_ptr->tm_hour, tm_ptr->tm_min, tm_ptr->tm_sec, string);

    // print in the log file and screen
    if (fprintf(log_fp, "%02d:%02d:%02d %s\n", tm_ptr->tm_hour, tm_ptr->tm_min, tm_ptr->tm_sec, string) < 0)
    {
        perror("Error writing to file");
        exit(1);
    }

    if (fflush(log_fp))
    {
        perror("fflush");
        exit(1);
    }
    if (fflush(stdout))
    {
        perror("fflush");
        exit(1);
    }
}

void read_config_file(const char *config_file)
{
    FILE *config_fp;

    // open config file to read
    config_fp = fopen(config_file, "r");

    if (config_fp == NULL)
    {
        perror("Error: opening config file");
        exit(1);
    }

    // get config values
    if (fscanf(config_fp, "%d\n%d\n%d\n%d\n%d", &config.QUEUE_POS, &config.AUTH_SERVERS_MAX, &config.AUTH_PROC_TIME, &config.MAX_VIDEO_WAIT, &config.MAX_OTHERS_WAIT) != 5)
    {
        fclose(config_fp);
        perror("Error: reading config file");
        exit(1);
    }

    if (config.QUEUE_POS < 0)
    {
        print_log("Error: QUEUE_POS must >=0");
        exit(1);
    }
    if (config.AUTH_SERVERS_MAX < 1)
    {
        print_log("Error: AUTH_SERVERS_MAX must >=1");
        exit(1);
    }
    if (config.AUTH_PROC_TIME < 0)
    {
        print_log("Error: AUTH_PROC_TIME must >=0");
        exit(1);
    }
    if (config.MAX_VIDEO_WAIT < 1)
    {
        print_log("Error: MAX_VIDEO_WAIT must >=1");
        exit(1);
    }
    if (config.MAX_OTHERS_WAIT < 1)
    {
        print_log("Error: MAX_OTHERS_WAIT must >=1");
        exit(1);
    }
}

void start(const char *config_file)
{
    // open log to append
    log_fp = fopen(LOG_FILENAME, "a");

    if (log_fp == NULL)
    {
        perror("Error: opening log file");
        exit(1);
    }

    read_config_file(config_file);
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
