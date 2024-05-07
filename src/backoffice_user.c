// Gustavo Samuel de Alves e Bastos de André e Lima 2020217743
// João Maria Pereira Carvalho de Picado Santos 2022213725

#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/msg.h>
#include <unistd.h>

// #define DEBUG //remove this line to remove debug messages

#define BACK_PIPE "/tmp/BACK_PIPE"

// Struct to a message
typedef struct
{
    long mtype;
    char message[2048];
} Message;

int console_id;

void *print_data_stats(void *args) {
    // read from msg queue with mtype 1
    key_t key;
    int msgid;
    Message buf;

    key = ftok(".", 'A');
    if (key == -1) {
        perror("ftok");
        exit(1);
    }

    msgid = msgget(key, 0666);
    if (msgid == -1) {
        perror("msgget");
        exit(1);
    }

    while (1) {
        if (msgrcv(msgid, &buf, sizeof(Message), console_id, 0) == -1) {
            perror("msgrcv");
            exit(1);
        }

        printf("\n%s\n> %d#", buf.message, console_id);
        fflush(stdout);
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    console_id = 1;
    char input[50];

    int pipe_fd;

    if ((pipe_fd = open(BACK_PIPE, O_WRONLY | O_NONBLOCK)) < 0) {
        perror("Error opening pipe");
        exit(1);
    }

    pthread_t thread;
    pthread_create(&thread, NULL, print_data_stats, NULL);

    while (1) {
        printf("> %d#", console_id);

        if (fgets(input, sizeof(input), stdin) == NULL) {
            perror("Error: reading from stdin");
            exit(1);
        }

        input[strcspn(input, "\n")] = '\0';

        if (strcmp(input, "exit") == 0) {
            break;
        } else if (strcmp(input, "data_stats") == 0) {
            if (write(pipe_fd, "1#data_stats", strlen("1#data_stats")) < 0) {
                perror("Error: writing to pipe");
                close(pipe_fd);
                exit(1);
            }

        } else if (strcmp(input, "reset") == 0) {
            // send reset stats via named pipe
            if (write(pipe_fd, "1#reset", strlen("1#reset")) < 0) {
                perror("Error: writing to pipe");
                close(pipe_fd);
                exit(1);
            }

        } else if (strcmp(input, "") == 0)
            ;
        else {
            printf("Error: invalid command\n");
        }
    }

    pthread_join(thread, NULL);

    exit(0);
}
