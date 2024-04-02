// Gustavo Samuel de Alves e Bastos de André e Lima 2020217743
// João Maria Pereira Carvalho de Picado Santos 2022213725

#include "5g_auth_platform.h"

void debug(const char *string) {
    // print a debug message
#ifdef DEBUG
    printf("Debug: (%s)\n", string);
#endif
}

void print_log(const char *string) {
    pthread_mutex_lock(&shm->mutex_log);

    // get the time
    time_t t = time(NULL);
    struct tm *tm_ptr = localtime(&t);

    if (tm_ptr == NULL)
        perror("Error: getting local time struct");

    // print in the log file and screen
    printf("%02d:%02d:%02d %s\n", tm_ptr->tm_hour, tm_ptr->tm_min, tm_ptr->tm_sec, string);
    if (fprintf(log_fp, "%02d:%02d:%02d %s\n", tm_ptr->tm_hour, tm_ptr->tm_min, tm_ptr->tm_sec, string) < 0)
        perror("Error: writing to file");

    if (fflush(log_fp))
        perror("Error: fflush log_fp");

    if (fflush(stdout))
        perror("Error: fflush stdout");

    pthread_mutex_unlock(&shm->mutex_log);
}

int read_config_file(const char *config_file) {
    // open config file to read
    FILE *config_fp = fopen(config_file, "r");

    if (config_fp == NULL) {
        perror("Error: opening config file");
        return -1;
    }

    // get config values
    if (fscanf(config_fp, "%d\n%d\n%d\n%d\n%d\n%d", &config.MOBILE_USERS, &config.QUEUE_POS, &config.AUTH_SERVERS_MAX, &config.AUTH_PROC_TIME, &config.MAX_VIDEO_WAIT, &config.MAX_OTHERS_WAIT) != 6) {
        fclose(config_fp);
        perror("Error: reading config file");
        return -1;
    }

    fclose(config_fp);

    if (config.MOBILE_USERS < 0) {
        print_log("Error: MOBILE_USERS must >=0");
        return -1;
    }
    if (config.QUEUE_POS < 0) {
        print_log("Error: QUEUE_POS must >=0");
        return -1;
    }
    if (config.AUTH_SERVERS_MAX < 1) {
        print_log("Error: AUTH_SERVERS_MAX must >=1");
        return -1;
    }
    if (config.AUTH_PROC_TIME < 0) {
        print_log("Error: AUTH_PROC_TIME must >=0");
        return -1;
    }
    if (config.MAX_VIDEO_WAIT < 1) {
        print_log("Error: MAX_VIDEO_WAIT must >=1");
        return -1;
    }
    if (config.MAX_OTHERS_WAIT < 1) {
        print_log("Error: MAX_OTHERS_WAIT must >=1");
        return -1;
    }

    return 0;
}

int init(const char *config_file) {
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
        return -1;
    }

    shm->users = (Mobile_user *)((char *)(shm + 1));
    shm->servers = (Server *)((char *)(shm->users) + config.MOBILE_USERS * sizeof(Mobile_user));

    // open log to append
    log_fp = fopen(LOG_FILENAME, "a");

    if (log_fp == NULL) {
        perror("Error: opening log file");
        return -1;
    }

    if (read_config_file(config_file) == -1)
        return -1;

    if (pthread_mutexattr_init(&mutex_attr)) {
        perror("Error: pthread_mutexattr_init");
        return -1;
    }

    if (pthread_mutexattr_setpshared(&mutex_attr, PTHREAD_PROCESS_SHARED)) {
        perror("Error: pthread_mutexattr_setpshared");
        return -1;
    }

    if (pthread_mutex_init(&shm->mutex_log, &mutex_attr)) {
        perror("Error: creating a mutex");
        return -1;
    }

    if (pthread_mutex_init(&shm->mutex_shm, &mutex_attr)) {
        perror("Error: creating a mutex");
        return -1;
    }

    servers_pid = malloc(config.AUTH_SERVERS_MAX * sizeof(pid_t));

    if (servers_pid == NULL) {
        print_log("Error: alocating memory for workers_pid");
        return -1;
    }

    return 0;
}

void cleanup() {
    if (servers_pid)
        free(servers_pid);

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
    char aux[512];

    if (sprintf(aux, "PROCESS AUTHORIZATION_ENGINE %d CREATED", server_id) < 0) {
        perror("Error: sprintf failed");
        exit(1);
    }
    print_log(aux);

    if (sprintf(aux, "PROCESS AUTHORIZATION_ENGINE %d READY", server_id) < 0) {
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
            for (int j = 0; j < i; j++)
                if (wait(NULL) == -1)
                    perror("Error: waiting for a process to finish");
            exit(1);
        } else if (servers_pid[i] == 0) {
            authorization_engine(i + 1);
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
    } else {
        // create the Sender thread
        if (pthread_create(&sender_t, NULL, sender, NULL))
            perror("Error: Creating sender thread");

        else if (pthread_join(sender_t, NULL))
            perror("Error: waiting for sender thread to finish");

        if (pthread_join(receiver_t, NULL))
            perror("Error: waiting for receiver thread to finish");
    }

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
    print_log("5G_AUTH_PLATFORM SIMULATOR STARTING");
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
    } else if (monitor_engine_pid == 0) {
        monitor_engine();
    } else {
        if (wait(NULL) == -1)
            perror("Error: waiting for a process to finish");
    }

    if (wait(NULL) == -1)
        perror("Error: waiting for a process to finish");

    print_log("5G_AUTH_PLATFORM SIMULATOR CLOSING");
}

int main(int argc, char *argv[]) {
    // check if the number of parameters is correct
    if (argc != 2) {
        printf("Error: Wrong parameters, use ./5g_auth_platform {config-file}\n");
        exit(1);
    }

    // start the simulation
    if (init(argv[1]) != -1)
        system_manager();

    // free resources
    cleanup();

    exit(0);
}
