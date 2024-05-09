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
    debug("Creating shared memory");
    shmid = shmget(IPC_PRIVATE, sizeof(Shared_Memory) + config.MOBILE_USERS * sizeof(Mobile_user) + sizeof(Server) * config.AUTH_SERVERS_MAX, IPC_CREAT | 0666);
    if (shmid == -1) {
        perror("Error: shmget error");
        exit(1);
    }

    debug("Attaching shared memory");
    shm = (Shared_Memory *)shmat(shmid, NULL, 0);
    if (shm == (Shared_Memory *)-1) {
        perror("Error: shmat error");
        return -1;
    }

    // initialize shared memory
    shm->users = (Mobile_user *)((char *)(shm + 1));
    shm->servers = (Server *)((char *)(shm->users) + config.MOBILE_USERS * sizeof(Mobile_user));

    shm->video_data = 0;
    shm->music_data = 0;
    shm->social_data = 0;
    shm->video_auth_reqs = 0;
    shm->music_auth_reqs = 0;
    shm->social_auth_reqs = 0;

    shm->num_servers = config.AUTH_SERVERS_MAX;

    for (int i = 0; i < config.MOBILE_USERS; i++) {
        shm->users[i].plafond = 0;
        shm->users[i].plafond_initial = 0;
        shm->users[i].id_mobile = 0;
        shm->users[i].checked = 0;
    }

    for (int i = 0; i < config.AUTH_SERVERS_MAX; i++) {
        shm->servers[i].state = 0;
    }

    // open log to append
    debug("Opening log file");
    log_fp = fopen(LOG_FILENAME, "a");

    if (log_fp == NULL) {
        perror("Error: opening log file");
        return -1;
    }

    debug("Reading config file");
    if (read_config_file(config_file) == -1)
        return -1;

    debug("initializing mutexes");
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
    // initialize the condition variable cond atributte
    if (pthread_condattr_init(&cond_attr)) {
        perror("Error: pthread_condattr_init");
        return -1;
    }
    if (pthread_condattr_setpshared(&cond_attr, PTHREAD_PROCESS_SHARED)) {
        perror("Error: pthread_condattr_setpshared");
        return -1;
    }

    // initialize the condition variable cond
    if (pthread_cond_init(&shm->cond_sender, &cond_attr)) {
        perror("Error: pthread_cond_init");
        return -1;
    }
    if (pthread_cond_init(&shm->cond_monitor_engine, &cond_attr)) {
        perror("Error: pthread_cond_init");
        return -1;
    }

    debug("allocating memory for servers_pid");
    servers_pid = malloc(config.AUTH_SERVERS_MAX * sizeof(pid_t));

    if (servers_pid == NULL) {
        print_log("Error: alocating memory for workers_pid");
        return -1;
    }

    // message queue initialization for the communication between the monitor engine and the authorization request manager

    debug("Creating message queue");
    key = ftok(".", 'A');

    if (key == -1) {
        perror("Error: ftok");
        return -1;
    }

    msgid = msgget(key, IPC_CREAT | 0666);

    if (msgid == -1) {
        perror("Error: msgget");
        return -1;
    }

    return 0;
}

void cleanup() {
    debug("Cleaning up");

    if (msgctl(msgid, IPC_RMID, NULL) == -1) {
        perror("msgctl");
    }

    debug("freeing servers_pid memory");
    if (servers_pid)
        free(servers_pid);

    debug("destroying condition variables");
    if (pthread_cond_destroy(&shm->cond_sender))
        perror("Error: destroy a condition variable cond_sender");

    if (pthread_cond_destroy(&shm->cond_monitor_engine))
        perror("Error: destroy a condition variable cond_monitor_engine");

    debug("destroying mutexes");
    if (pthread_mutex_destroy(&shm->mutex_shm))
        perror("Error: destroy a mutex mutex_shm");

    if (pthread_mutex_destroy(&shm->mutex_log))
        perror("Error: destroy a mutex mutex_log");

    debug("closing log file");
    if (log_fp)
        fclose(log_fp);

    debug("detaching shared memory");
    if (shmdt(shm))
        perror("Error: shmdt error");

    debug("removing shared memory");
    if (shmctl(shmid, IPC_RMID, NULL))
        perror("Error: shmget error");
}

void *receiver(void *arg) {
    print_log("THREAD RECEIVER CREATED");

    // open the user pipe and back pipe
    int user_pipe_fd, back_pipe_fd;

    if ((user_pipe_fd = open(USER_PIPE, O_RDONLY | O_NONBLOCK)) < 0) {
        perror("Error opening USER_PIPE");
        pthread_exit(NULL);
    }

    if ((back_pipe_fd = open(BACK_PIPE, O_RDONLY | O_NONBLOCK)) < 0) {
        perror("Error opening BACK_PIPE");
        close(user_pipe_fd);
        pthread_exit(NULL);
    }

    while (1) {
        // now we want to read from the pipes, so we need to use select

        Task tarefa;
        bzero(&tarefa, sizeof(Task));

        fd_set readset;

        FD_ZERO(&readset);
        FD_SET(user_pipe_fd, &readset);
        FD_SET(back_pipe_fd, &readset);

        if (select(back_pipe_fd + 1, &readset, NULL, NULL, NULL) == -1) {
            perror("Erro em select");
        } else {
            if (FD_ISSET(back_pipe_fd, &readset)) {
                char buffer[2048];
                int n = read(back_pipe_fd, buffer, sizeof(buffer));

                if (n < 0) {
                    perror("Error reading from USER_PIPE");
                    break;
                }

                buffer[n] = '\0';

                char *token = strtok(buffer, "#");
                tarefa.id = atoi(token);

                token = strtok(NULL, "#");

                tarefa.type = 0;
                tarefa.arrival_time = time(NULL);

                if (strcmp(token, "data_stats") == 0) {
                    // print data stats
                    tarefa.data = 0;
                    // add task to the queue
                    pthread_mutex_lock(&mutex_others_queue);
                    if (others_queue_size == 0) {
                        print_log("Others queue full, Task Descartada");
                        pthread_mutex_unlock(&mutex_others_queue);
                        continue;
                    }

                    others_queue_size--;
                    tarefa.arrival_time = time(NULL);
                    // add task to the queue
                    add_task_to_queue(tarefa, &others_queue);
                    pthread_mutex_unlock(&mutex_others_queue);
                    pthread_cond_signal(&shm->cond_sender);
                } else if (strcmp(token, "reset") == 0) {
                    // reset stats
                    tarefa.data = 1;
                    // add task to the queue
                    pthread_mutex_lock(&mutex_others_queue);
                    if (others_queue_size == 0) {
                        print_log("Others queue full, Task Descartada");
                        pthread_mutex_unlock(&mutex_others_queue);
                        continue;
                    }

                    others_queue_size--;
                    tarefa.arrival_time = time(NULL);
                    // add task to the queue
                    add_task_to_queue(tarefa, &others_queue);
                    pthread_mutex_unlock(&mutex_others_queue);
                    pthread_cond_signal(&shm->cond_sender);
                } else {
                    print_log("Error: invalid command");
                }
            }

            // check if there is data to read in the user pipe
            if (FD_ISSET(user_pipe_fd, &readset)) {
                // read from the user pipe
                char buffer[2048];
                int n = read(user_pipe_fd, buffer, sizeof(buffer));

                if (n < 0) {
                    perror("Error reading from USER_PIPE");
                    break;
                }

                buffer[n] = '\0';

                // parse the command
                char *token = strtok(buffer, "#");

                tarefa.id = atoi(token);

                token = strtok(NULL, "#");

                if (strcmp(token, "MUSIC") == 0) {
                    // video data
                    token = strtok(NULL, "#");
                    tarefa.type = 1;
                    tarefa.data = atoi(token);
                } else if (strcmp(token, "SOCIAL") == 0) {
                    // music data
                    token = strtok(NULL, "#");
                    tarefa.type = 2;
                    tarefa.data = atoi(token);
                } else if (strcmp(token, "VIDEO") == 0) {
                    // social data
                    token = strtok(NULL, "#");
                    tarefa.type = 3;
                    tarefa.data = atoi(token);

                    // add task to the queue
                    pthread_mutex_lock(&mutex_video_queue);
                    if (video_queue_size == 0) {
                        print_log("Video queue full, Task Descartada");
                        pthread_mutex_unlock(&mutex_video_queue);
                        continue;
                    }

                    video_queue_size--;
                    tarefa.arrival_time = time(NULL);
                    add_task_to_queue(tarefa, &video_queue);
                    pthread_mutex_unlock(&mutex_video_queue);
                    pthread_cond_signal(&shm->cond_sender);
                    continue;
                } else {
                    // set plafond for a mobile user
                    tarefa.type = 4;
                    tarefa.data = atoi(token);
                }

                // add task to the queue
                pthread_mutex_lock(&mutex_others_queue);
                if (others_queue_size == 0) {
                    print_log("Others queue full, Task Descartada");
                    pthread_mutex_unlock(&mutex_others_queue);
                    continue;
                }

                others_queue_size--;
                tarefa.arrival_time = time(NULL);
                add_task_to_queue(tarefa, &others_queue);
                pthread_mutex_unlock(&mutex_others_queue);
                pthread_cond_signal(&shm->cond_sender);
            }
        }
    }
    debug("thread receiver closing");
    pthread_exit(NULL);
}

void add_task_to_queue(Task tarefa, Node **queue) {
    Node *new_node = (Node *)malloc(sizeof(Node));
    if (new_node == NULL) {
        perror("Error: malloc");
        return;
    }

    new_node->task = tarefa;
    new_node->next = NULL;

    if (*queue == NULL) {
        *queue = new_node;
    } else {
        Node *current = *queue;
        while (current->next != NULL)
            current = current->next;

        current->next = new_node;
    }
}

int get_free_server() {
    for (int i = 0; i < config.AUTH_SERVERS_MAX; i++) {
        if (shm->servers[i].state == 1) {
            return i;
        }
    }

    return -1;
}

void *sender(void *arg) {
    char aux[512];
    print_log("THREAD SENDER CREATED");
    while (1) {
        // check if there is a task in the video queue to send
        Node *current;

        pthread_mutex_lock(&mutex_video_queue);
        while (1) {
            if (video_queue != NULL) {
                current = video_queue;
                video_queue = video_queue->next;
                video_queue_size++;
                pthread_mutex_unlock(&mutex_video_queue);
                break;
            }
            pthread_mutex_unlock(&mutex_video_queue);

            pthread_mutex_lock(&mutex_others_queue);
            if (others_queue != NULL) {
                current = others_queue;
                others_queue = others_queue->next;
                others_queue_size++;
                pthread_mutex_unlock(&mutex_others_queue);
                break;
            }
            pthread_mutex_unlock(&mutex_others_queue);

            pthread_cond_wait(&shm->cond_sender, &mutex_video_queue);
        }

        pthread_mutex_lock(&shm->mutex_shm);
        while (1) {  // check if there are servers available

            int i = get_free_server();

            if (i == -1) {
                pthread_mutex_unlock(&shm->mutex_shm);
                pthread_cond_wait(&shm->cond_sender, &shm->mutex_shm);
            } else {
                // send the task to the server
                write(shm->servers[i].fd[1], (void *)&current->task, sizeof(Task));
                if (sprintf(aux, "SENDER: %s AUTHORIZATION REQUEST (ID = %d) SENT FOR PROCESSING ON AUTH ENGINE %d", current->task.type == 1 ? "MUSIC" : current->task.type == 2 ? "SOCIAL"
                                                                                                                                                     : current->task.type == 3   ? "VIDEO"
                                                                                                                                                                                 : "OTHERS",
                            current->task.id, i + 1) < 0) {
                    perror("Error: sprintf failed");
                    exit(1);
                }
                print_log(aux);
                free(current);
                shm->servers[i].state = 2;
                pthread_mutex_unlock(&shm->mutex_shm);

                break;
            }
        }
    }

    debug("thread sender closing");
    pthread_exit(NULL);
}

void authorization_engine(int server_id) {
    char aux[512];

    if (sprintf(aux, "PROCESS AUTHORIZATION_ENGINE %d READY", server_id + 1) < 0) {
        perror("Error: sprintf failed");
        exit(1);
    }
    print_log(aux);

    // infinite loop to process requests
    // recive via unnamed pipe
    Task tarefa;

    while (1) {
        pthread_mutex_lock(&shm->mutex_shm);
        shm->servers[server_id].state = 1;
        pthread_mutex_unlock(&shm->mutex_shm);

        // signal the sender
        pthread_cond_signal(&shm->cond_sender);

        read(shm->servers[server_id].fd[0], (void *)&tarefa, sizeof(Task));

        usleep(config.AUTH_PROC_TIME * 1000);

        pthread_mutex_lock(&shm->mutex_shm);
        if (tarefa.type == 0) {
            if (tarefa.data == 0) {  // send via msg queue
                Message msg;
                msg.mtype = tarefa.id;
                sprintf(msg.message, "Service\tTotal Data\tAuth Reqs\nVideo\t%d\t%d\nMusic\t%d\t%d\nSocial\t%d\t%d\n", shm->video_data, shm->video_auth_reqs, shm->music_data, shm->music_auth_reqs, shm->social_data, shm->social_auth_reqs);

                if (msgsnd(msgid, &msg, sizeof(msg), 0) == -1) {
                    perror("msgsnd");
                }
                print_log(msg.message);

            } else {  // reset stats
                shm->video_data = 0;
                shm->music_data = 0;
                shm->social_data = 0;
                shm->video_auth_reqs = 0;
                shm->music_auth_reqs = 0;
                shm->social_auth_reqs = 0;
            }
        } else {
            if (tarefa.type == 4) {  // set plafond
                shm->users[tarefa.id].plafond = tarefa.data;
                shm->users[tarefa.id].plafond_initial = tarefa.data;
            } else {
                if (tarefa.type == 1) {  // music data
                    shm->music_data += tarefa.data;
                    shm->music_auth_reqs++;
                } else if (tarefa.type == 2) {  // social data
                    shm->social_data += tarefa.data;
                    shm->social_auth_reqs++;
                } else if (tarefa.type == 3) {  // video data
                    shm->video_data += tarefa.data;
                    shm->video_auth_reqs++;
                }
                if (tarefa.data > shm->users[tarefa.id].plafond)
                    shm->users[tarefa.id].plafond = 0;
                else
                    shm->users[tarefa.id].plafond -= tarefa.data;

                pthread_cond_signal(&shm->cond_monitor_engine);
            }
        }

        if (sprintf(aux, "AUTHORIZATION_ENGINE %d: %s AUTHORIZATION REQUEST (ID = %d) PROCESSING COMPLETED", server_id + 1, tarefa.type == 1 ? "MUSIC" : tarefa.type == 2 ? "SOCIAL"
                                                                                                                                                     : tarefa.type == 3   ? "VIDEO"
                                                                                                                                                                          : "OTHERS",
                    tarefa.id) < 0) {
            perror("Error: sprintf failed");
            exit(1);
        }
        print_log(aux);
        pthread_mutex_unlock(&shm->mutex_shm);
    }

    debug("authorization engine closing");
    exit(0);
}

void authorization_request_manager() {
    print_log("PROCESS AUTHORIZATION_REQUEST_MANAGER CREATED");

    for (int i = 0; i < config.AUTH_SERVERS_MAX; i++) {
        // create unnamed pipes
        if (pipe(shm->servers[i].fd) == -1) {
            perror("Error: creating pipe");
            exit(1);
        }

        servers_pid[i] = fork();
        if (servers_pid[i] < 0) {
            perror("Error: creating process Authorization Engine\n");
            for (int j = 0; j < i; j++)
                if (wait(NULL) == -1)
                    perror("Error: waiting for a process to finish");
            exit(1);
        } else if (servers_pid[i] == 0) {
            authorization_engine(i);
        }
    }

    video_queue_size = config.QUEUE_POS;
    others_queue_size = config.QUEUE_POS;

    // create named pipes
    if ((mkfifo(USER_PIPE, O_CREAT | O_EXCL | 0666) < 0) && (errno != EEXIST)) {
        perror("Cannot create USER_PIPE: ");
        exit(1);
    }

    if ((mkfifo(BACK_PIPE, O_CREAT | O_EXCL | 0666) < 0) && (errno != EEXIST)) {
        perror("Cannot create BACK_PIPE: ");
        exit(1);
    }

    debug("Creating receiver and sender threads");
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

    debug("Waiting for Authorization Engine processes to finish");
    for (int i = 0; i < config.AUTH_SERVERS_MAX; i++) {
        if (wait(NULL) == -1) {
            perror("Error: waiting for a process to finish");
            exit(1);
        }
    }

    if (unlink(USER_PIPE)) {
        perror("Error: unlinking USER_PIPE");
    }

    if (unlink(BACK_PIPE)) {
        perror("Error: unlinking BACK_PIPE");
    }

    exit(0);
}

void *trintasecs() {
    while (1) {
        sleep(30);
        pthread_mutex_lock(&shm->mutex_shm);
        // send stats
        Message msg;
        msg.mtype = 1;
        sprintf(msg.message, "Service\tTotal Data\tAuth Reqs\nVideo\t%d\t%d\nMusic\t%d\t%d\nSocial\t%d\t%d\n", shm->video_data, shm->video_auth_reqs, shm->music_data, shm->music_auth_reqs, shm->social_data, shm->social_auth_reqs);
        print_log(msg.message);
        if (msgsnd(msgid, &msg, sizeof(Message), 0) == -1) {
            perror("msgsnd");
        }
        pthread_mutex_unlock(&shm->mutex_shm);
    }

    pthread_exit(NULL);
}

void monitor_engine() {
    print_log("PROCESS MONITOR_ENGINE CREATED");

    // create thread to  each 30 seconds send data_stats to backoffice
    pthread_t thread;
    if (pthread_create(&thread, NULL, trintasecs, NULL)) {
        perror("Error: creating trintasecs thread");
    }

    Message msg;
    while (1) {
        pthread_cond_wait(&shm->cond_monitor_engine, &shm->mutex_shm);
        for (int i = 0; i < config.MOBILE_USERS; i++) {
            if (shm->users[i].plafond_initial == 0) continue;

            if (shm->users[i].checked < 3 && shm->users[i].plafond == 0) {
                sprintf(msg.message, "ALERT 100%% (USER %d) TRIGGERED", shm->users[i].id_mobile);
                shm->users[i].checked = 3;
            } else if (shm->users[i].checked < 2 && shm->users[i].plafond <= 0.1 * shm->users[i].plafond_initial) {
                sprintf(msg.message, "ALERT 90%% (USER %d) TRIGGERED", shm->users[i].id_mobile);
                shm->users[i].checked = 2;
            } else if (shm->users[i].checked < 1 && shm->users[i].plafond <= 0.2 * shm->users[i].plafond_initial) {
                sprintf(msg.message, "ALERT 80%% (USER %d) TRIGGERED", shm->users[i].id_mobile);
                shm->users[i].checked = 1;
            } else
                continue;

            msg.mtype = shm->users[i].id_mobile;
            if (msgsnd(msgid, &msg, sizeof(Message), 0) == -1) {
                perror("msgsnd");
            }
            print_log(msg.message);
        }
        pthread_mutex_unlock(&shm->mutex_shm);
    }

    debug("monitor engine closing");
    exit(0);
}

void system_manager() {
    print_log("5G_AUTH_PLATFORM SIMULATOR STARTING");
    print_log("PROCESS SYSTEM_MANAGER CREATED");

    system_manager_pid = getpid();

    debug("Creating Authorization Request Manager and Monitor Engine processes");
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
        debug("Waiting for Authorization Request Manager and Monitor Engine processes to finish");
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
