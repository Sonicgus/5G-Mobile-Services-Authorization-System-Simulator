// Gustavo Samuel de Alves e Bastos de André e Lima 2020217743
// João Maria Pereira Carvalho de Picado Santos 2022213725

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

// #define DEBUG //remove this line to remove debug messages
#define LOG_FILENAME "log.txt"
#define USER_PIPE "/tmp/USER_PIPE"
#define BACK_PIPE "/tmp/BACK_PIPE"

typedef struct
{
    int plafond;
    int id_mobile;
} Mobile_user;

typedef struct
{
    int state;  // 0 - desligado, 1 - a espera de uma tarefa, 2 - a executar uma tarefa
} Server;

// Struct to store the shared memory
typedef struct
{
    pthread_mutex_t mutex_shm, mutex_log;

    // stats
    int video_data,
        music_data,
        social_data;

    int video_auth_reqs,
        music_auth_reqs,
        social_auth_reqs;

    Mobile_user *users;
    Server *servers;

} Shared_Memory;

// Struct to store the configuration
typedef struct
{
    int MOBILE_USERS,
        QUEUE_POS,
        AUTH_SERVERS_MAX,
        AUTH_PROC_TIME,
        MAX_VIDEO_WAIT,
        MAX_OTHERS_WAIT;
} Config;

typedef struct
{
    int type;  //-1 - vazia / 0 - comando / 1 - dados de other / 2 - dados de video
    char string[2048];
} Task;

typedef struct node {
    Task task;
    struct node *next;
} Node;

Config config;
int shmid;
Shared_Memory *shm;
FILE *log_fp;
pthread_mutexattr_t mutex_attr;
pid_t system_manager_pid, authorization_request_manager_pid, monitor_engine_pid;
pid_t *servers_pid;
