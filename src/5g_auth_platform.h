// Gustavo Samuel de Alves e Bastos de André e Lima 2020217743
// João Maria Pereira Carvalho de Picado Santos 2022213725

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>
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
    int plafond_initial;
    int id_mobile;
} Mobile_user;

typedef struct
{
    int state;  // 0 - desligado, 1 - a espera de uma tarefa, 2 - a executar uma tarefa
    int fd[2];
} Server;

// Struct to store the shared memory
typedef struct
{
    pthread_mutex_t mutex_shm, mutex_log;
    pthread_cond_t cond_sender;

    // stats
    int video_data,
        music_data,
        social_data;

    int video_auth_reqs,
        music_auth_reqs,
        social_auth_reqs;

    int num_servers;

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
    int type;  // 0 - comando / 1 - dados de musica / 2 - dados de social / 3 - dados de video / 4 - set plafond
    int id;
    int data;  // if type == 0, data = 0 -> data_stats / data = 1 -> reset else if type == 1 or 2 or 3 or 4, data = dados
    long arrival_time;
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

key_t key;
int msgid;

Node *video_queue, *others_queue;
int video_queue_size;
int others_queue_size;

pthread_mutex_t mutex_video_queue = PTHREAD_MUTEX_INITIALIZER,
                mutex_others_queue = PTHREAD_MUTEX_INITIALIZER;

pthread_t receiver_t, sender_t;

void add_task_to_video_queue(Task tarefa);
void add_task_to_others_queue(Task tarefa);

// Struct to a message
typedef struct
{
    long mtype;
    char message[2048];
} Message;

pthread_cond_t cond_receiver = PTHREAD_COND_INITIALIZER;

pthread_condattr_t cond_attr;