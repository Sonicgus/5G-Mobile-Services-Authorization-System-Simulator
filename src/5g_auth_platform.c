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
    pthread_mutex_t mutex_log;
    pthread_mutex_t mutex_shm;

    Mobile_user *users;
    Server *servers;

    // stats
    int video_data, music_data, social_data;
    int video_auth_reqs, music_auth_reqs, social_auth_reqs;

} Shared_Memory;

// Struct to store the configuration
typedef struct
{
    int MOBILE_USERS, QUEUE_POS, AUTH_SERVERS_MAX, AUTH_PROC_TIME, MAX_VIDEO_WAIT, MAX_OTHERS_WAIT;
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

pid_t *servers_pid;
Config config;
int shmid;
Shared_Memory *shm;
FILE *log_fp;
pthread_mutexattr_t mutex_attr;
pid_t system_manager_pid, authorization_request_manager_pid, monitor_engine_pid;

void debug(const char *string) {
    // print a debug message
#ifdef DEBUG
    printf("Debug: (%s)\n", string);
#endif
}

void print_log(const char *string) {
    pthread_mutex_lock(&shm->mutex_log);

    time_t t;
    struct tm *tm_ptr;
    // get the time
    t = time(NULL);
    tm_ptr = localtime(&t);

    if (tm_ptr == NULL) {
        perror("Error: getting local time struct");
        exit(1);
    }

    printf("%02d:%02d:%02d %s\n", tm_ptr->tm_hour, tm_ptr->tm_min, tm_ptr->tm_sec, string);

    // print in the log file and screen
    if (fprintf(log_fp, "%02d:%02d:%02d %s\n", tm_ptr->tm_hour, tm_ptr->tm_min, tm_ptr->tm_sec, string) < 0) {
        perror("Error writing to file");
        exit(1);
    }

    if (fflush(log_fp)) {
        perror("fflush");
        exit(1);
    }
    if (fflush(stdout)) {
        perror("fflush");
        exit(1);
    }

    pthread_mutex_unlock(&shm->mutex_log);
}

void read_config_file(const char *config_file) {
    FILE *config_fp;

    // open config file to read
    config_fp = fopen(config_file, "r");

    if (config_fp == NULL) {
        perror("Error: opening config file");
        exit(1);
    }

    // get config values
    if (fscanf(config_fp, "%d\n%d\n%d\n%d\n%d\n%d", &config.MOBILE_USERS, &config.QUEUE_POS, &config.AUTH_SERVERS_MAX, &config.AUTH_PROC_TIME, &config.MAX_VIDEO_WAIT, &config.MAX_OTHERS_WAIT) != 6) {
        fclose(config_fp);
        perror("Error: reading config file");
        exit(1);
    }

    if (config.MOBILE_USERS < 0) {
        print_log("Error: MOBILE_USERS must >=0");
        exit(1);
    }
    if (config.QUEUE_POS < 0) {
        print_log("Error: QUEUE_POS must >=0");
        exit(1);
    }
    if (config.AUTH_SERVERS_MAX < 1) {
        print_log("Error: AUTH_SERVERS_MAX must >=1");
        exit(1);
    }
    if (config.AUTH_PROC_TIME < 0) {
        print_log("Error: AUTH_PROC_TIME must >=0");
        exit(1);
    }
    if (config.MAX_VIDEO_WAIT < 1) {
        print_log("Error: MAX_VIDEO_WAIT must >=1");
        exit(1);
    }
    if (config.MAX_OTHERS_WAIT < 1) {
        print_log("Error: MAX_OTHERS_WAIT must >=1");
        exit(1);
    }
}

void start(const char *config_file) {
    // create shared memory
    shmid = shmget(IPC_PRIVATE, sizeof(Shared_Memory) + config.MOBILE_USERS * sizeof(Mobile_user) + sizeof(Server) * config.AUTH_SERVERS_MAX, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("Error: shmget error");
        exit(1);
    }

    // Attach shared memory thread to process
    shm = (Shared_Memory *)shmat(shmid, NULL, 0);
    if (shm == (Shared_Memory *)-1) {
        perror("Error: shmat error");
        exit(1);
    }

    // aFHDFKJASDHFASFHSKJHFAHSKJ
    shm->users = (Mobile_user *)((char *)(shm + 1));

    // open log to append
    log_fp = fopen(LOG_FILENAME, "a");

    if (log_fp == NULL) {
        perror("Error: opening log file");
        exit(1);
    }

    read_config_file(config_file);

    if (pthread_mutexattr_init(&mutex_attr)) {
        perror("Error: pthread_mutexattr_init");
        exit(1);
    }

    if (pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED)) {
        perror("Error: pthread_mutexattr_setpshared");
        exit(1);
    }

    if (pthread_mutex_init(&shm->mutex_log, &mutex_attr)) {
        perror("Error: creating a mutex");
        exit(1);
    }

    if (pthread_mutex_init(&shm->mutex_shm, &mutex_attr)) {
        perror("Error: creating a mutex");
        exit(1);
    }

    servers_pid = malloc(config.AUTH_SERVERS_MAX * sizeof(pid_t));

    if (servers_pid == NULL) {
        print_log("Error: alocating memory for workers_pid");
        exit(1);
    }

    print_log("5G_AUTH_PLATFORM SIMULATOR STARTING");
}

void finish() {
    print_log("5G_AUTH_PLATFORM SIMULATOR FINISHING");

    if (pthread_mutex_destroy(&shm->mutex_shm)) {
        perror("Error: destroy a mutex mutex_shm");
    }

    if (pthread_mutex_destroy(&shm->mutex_log)) {
        perror("Error: destroy a mutex mutex_log");
    }

    // close log file
    if (log_fp)
        fclose(log_fp);

    if (shmdt(shm))
        perror("Error: shmdt error");

    // remove the shared memory
    if (shmctl(shmid, IPC_RMID, NULL))
        perror("Error: shmget error");
}

void *receiver(void *arg) {
    print_log("THREAD RECEIVER CREATED");
    pthread_exit(NULL);
}

void *sender(void *arg) {
    print_log("THREAD SENDER CREATED");
    pthread_exit(NULL);
}

void authorization_engine(int server_id) {
    char aux[2048];

    if (sprintf(aux, "PROCESS AUTHORIZATION_ENGINE %d CREATED", server_id + 1) < 0) {
        perror("Error: sprintf failed");
        exit(1);
    }
    print_log(aux);

    if (sprintf(aux, "PROCESS AUTHORIZATION_ENGINE %d READY", server_id + 1) < 0) {
        perror("Error: sprintf failed");
        exit(1);
    }
    print_log(aux);
    exit(0);
}

void authorization_request_manager() {
    print_log("PROCESS AUTHORIZATION_REQUEST_MANAGER CREATED");

    for (int i = 0; i < config.AUTH_SERVERS_MAX; i++) {
        servers_pid[i] = fork();

        if (servers_pid[i] < 0) {
            perror("Error: creating process Authorization Engine\n");
            exit(1);
        } else if (servers_pid[i] == 0) {
            authorization_engine(i);
        }
    }

    Node *video_queue, *others_queue;
    int video_queue_size = config.QUEUE_POS;
    int others_queue_size = config.QUEUE_POS;

    pthread_mutex_t mutex_video_queue = PTHREAD_MUTEX_INITIALIZER,
                    mutex_others_queue = PTHREAD_MUTEX_INITIALIZER;

    pthread_t receiver_t, sender_t;

    // create the Receiver thread
    if (pthread_create(&receiver_t, NULL, receiver, NULL)) {
        perror("Error: Creating receiver thread");
        exit(1);
    }

    // create the Sender thread
    if (pthread_create(&sender_t, NULL, sender, NULL)) {
        perror("Error: Creating sender thread");
        exit(1);
    }

    // wait for the threads to finish
    if (pthread_join(sender_t, NULL)) {
        perror("Error: waiting for sender thread to finish");
        exit(1);
    }
    if (pthread_join(receiver_t, NULL)) {
        perror("Error: waiting for receiver thread to finish");
        exit(1);
    }

    // print_log("5G_AUTH_PLATFORM SIMULATOR WAITING FOR LAST TASKS TO FINISH");

    for (int i = 0; i < config.AUTH_SERVERS_MAX; i++) {
        if (wait(NULL) == -1) {
            perror("Error: waiting for a process to finish");
            exit(1);
        }
    }

    exit(0);
}

void monitor_engine() {
    print_log("PROCESS MONITOR_ENGINE CREATED");
    exit(0);
}

void system_manager() {
    print_log("PROCESS SYSTEM_MANAGER CREATED");

    system_manager_pid = getpid();

    // create the Authorization Request Manager process
    authorization_request_manager_pid = fork();

    if (authorization_request_manager_pid < 0) {
        perror("Error: creating process Authorization Request Manager\n");
        return;
    } else if (authorization_request_manager_pid == 0) {
        authorization_request_manager();
    }

    // create the Monitor Engine process
    monitor_engine_pid = fork();

    if (monitor_engine_pid < 0) {
        perror("Error: creating process Monitor Engine\n");
        return;
    } else if (monitor_engine_pid == 0) {
        monitor_engine();
    }

    for (int i = 0; i < 2; i++) {
        if (wait(NULL) == -1) {
            perror("Error: waiting for a process to finish");
            exit(1);
        }
    }
}

int main(int argc, char *argv[]) {
    // check if the number of parameters is correct
    if (argc != 2) {
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
