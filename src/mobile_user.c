// Gustavo Samuel de Alves e Bastos de André e Lima 2020217743
// João Maria Pereira Carvalho de Picado Santos 2022213725

#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <unistd.h>

// #define DEBUG //remove this line to remove debug messages

#define USER_PIPE "/tmp/USER_PIPE"

// Struct to a message
typedef struct
{
    long mtype;
    char message[2048];
} Message;

pid_t pid;
int pipe_fd,

    n_pedidos,
    intervalo_video,
    intervalo_musica,
    intervalo_social,
    dados;

void *reserve_video_data(void *args) {
    for (int i = 0; i < n_pedidos; i++) {
        sleep(intervalo_video);
        char buffer[100];
        sprintf(buffer, "%d#VIDEO#%d\n", pid, dados);
        write(pipe_fd, buffer, strlen(buffer));
        printf("Mobile user %d sent a video request\n", pid);
    }

    pthread_exit(NULL);
}

void *reserve_music_data(void *args) {
    for (int i = 0; i < n_pedidos; i++) {
        sleep(intervalo_musica);
        char buffer[100];
        sprintf(buffer, "%d#MUSIC#%d\n", pid, dados);
        write(pipe_fd, buffer, strlen(buffer));
        printf("Mobile user %d sent a music request\n", pid);
    }

    pthread_exit(NULL);
}

void *reserve_social_data(void *args) {
    for (int i = 0; i < n_pedidos; i++) {
        sleep(intervalo_social);
        char buffer[100];
        sprintf(buffer, "%d#SOCIAL#%d\n", pid, dados);
        write(pipe_fd, buffer, strlen(buffer));
        printf("Mobile user %d sent a social request\n", pid);
    }

    pthread_exit(NULL);
}

void *print_alerts(void *args) {
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
        if (msgrcv(msgid, &buf, sizeof(Message), pid, 0) == -1) {
            perror("msgrcv");
            exit(1);
        }
        printf("\n%s\n", buf.message);

        char aux[512];

        sprintf(aux, "ALERT: Plafond de dados atingiu 100%% para o Mobile User %d", pid);

        if (strcmp(buf.message, aux) == 0) {
            printf("Mobile user %d reached 100%% of data usage\n, Exiting...", pid);
            exit(0);
        }

        fflush(stdout);
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    // check if the number of parameters is correct
    if (argc != 7) {
        printf("Error: Wrong parameters, use ./mobile_user {plafond inicial} {número de pedidos de autorização} {intervalo VIDEO} {intervalo MUSIC} {intervalo SOCIAL} {dados a reservar}\n");
        exit(1);
    }

    // check if the parameters are numbers
    for (int i = 1; i < 7; i++)
        for (int j = 0; j < strlen(argv[i]); j++)
            if (!isdigit(argv[i][j])) {
                printf("Error: invalid paramter %d, must be a number\n", i);
                exit(1);
            }

    int plafond = atoi(argv[1]);
    n_pedidos = atoi(argv[2]),
    intervalo_video = atoi(argv[3]),
    intervalo_musica = atoi(argv[4]),
    intervalo_social = atoi(argv[5]),
    dados = atoi(argv[6]);

    pid = getpid();
    printf("Mobile user pid: %d\n", pid);

    if (plafond < 0) {
        printf("Error: Plafond inicial must be a positive number\n");
        exit(1);
    }
    if (n_pedidos < 0) {
        printf("Error: Número de pedidos de autorização must be a positive number\n");
        exit(1);
    }
    if (intervalo_video < 0) {
        printf("Error: Intervalo VIDEO must be a positive number\n");
        exit(1);
    }
    if (intervalo_musica < 0) {
        printf("Error: Intervalo MUSIC must be a positive number\n");
        exit(1);
    }
    if (intervalo_social < 0) {
        printf("Error: Intervalo SOCIAL must be a positive number\n");
        exit(1);
    }
    if (dados < 0) {
        printf("Error: Dados a reservar must be a positive number\n");
        exit(1);
    }

    if ((pipe_fd = open(USER_PIPE, O_WRONLY | O_NONBLOCK)) < 0) {
        perror("Error opening pipe");
        exit(1);
    }

    // send the initial plafond
    char buffer[100];
    sprintf(buffer, "%d#%d\n", pid, plafond);
    if (write(pipe_fd, buffer, strlen(buffer)) < 0) {
        perror("Error writing to pipe");
        exit(1);
    }

    // create the 3 threads
    pthread_t video_thread, music_thread, social_thread;
    if (pthread_create(&video_thread, NULL, reserve_video_data, NULL))
        perror("Error creating video thread");
    if (pthread_create(&music_thread, NULL, reserve_music_data, NULL))
        perror("Error creating music thread");
    if (pthread_create(&social_thread, NULL, reserve_social_data, NULL))
        perror("Error creating social thread");

    if (pthread_join(video_thread, NULL))
        perror("Error joining video thread");
    if (pthread_join(music_thread, NULL))
        perror("Error joining music thread");
    if (pthread_join(social_thread, NULL))
        perror("Error joining social thread");

    exit(0);
}
