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
    long int mtype;
    char message[2048];
} Message;

pid_t pid;
int pipe_fd,

    n_pedidos,
    intervalo_video,
    intervalo_musica,
    intervalo_social,
    dados;

pthread_mutex_t n_pedidos_mutex = PTHREAD_MUTEX_INITIALIZER;

void *reserve_video_data(void *args) {
    char buffer[100];
    while (n_pedidos != 0) {
        pthread_mutex_lock(&n_pedidos_mutex);
        n_pedidos--;
        pthread_mutex_unlock(&n_pedidos_mutex);

        sleep(intervalo_video);

        sprintf(buffer, "%d#VIDEO#%d\n", pid, dados);
        write(pipe_fd, buffer, strlen(buffer));
        printf("Mobile user %d sent a video request\n", pid);
        if (fflush(stdout) != 0)
            perror("Error flushing stdout");
    }

    pthread_exit(NULL);
}

void *reserve_music_data(void *args) {
    while (n_pedidos != 0) {
        pthread_mutex_lock(&n_pedidos_mutex);
        n_pedidos--;
        pthread_mutex_unlock(&n_pedidos_mutex);

        sleep(intervalo_musica);

        char buffer[100];
        sprintf(buffer, "%d#MUSIC#%d\n", pid, dados);
        write(pipe_fd, buffer, strlen(buffer));
        printf("Mobile user %d sent a music request\n", pid);
        if (fflush(stdout) != 0)
            perror("Error flushing stdout");
    }

    pthread_exit(NULL);
}

void *reserve_social_data(void *args) {
    while (n_pedidos) {
        pthread_mutex_lock(&n_pedidos_mutex);
        n_pedidos--;
        pthread_mutex_unlock(&n_pedidos_mutex);

        sleep(intervalo_social);

        char buffer[100];
        sprintf(buffer, "%d#SOCIAL#%d\n", pid, dados);
        write(pipe_fd, buffer, strlen(buffer));
        printf("Mobile user %d sent a social request\n", pid);
        if (fflush(stdout) != 0)
            perror("Error flushing stdout");
    }

    pthread_exit(NULL);
}

void *print_alerts(void *args) {
    // read from msg queue with mtype 1
    key_t key;
    int msgid;

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

    char aux[512];

    sprintf(aux, "ALERT 100%% (USER %d) TRIGGERED", pid);

    Message buf;

    while (1) {
        if (msgrcv(msgid, &buf, sizeof(Message), pid, 0) == -1) {
            perror("msgrcv");
            exit(1);
        }
        printf("\n%s\n", buf.message);
        if (fflush(stdout) != 0)
            perror("Error flushing stdout");

        if (strcmp(buf.message, aux) == 0) {
            printf("Mobile user %d reached 100%% of data usage\n, Exiting...", pid);
            if (fflush(stdout) != 0)
                perror("Error flushing stdout");
            exit(0);
        }
    }
    pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
    // check if the number of parameters is correct
    if (argc != 7) {
        printf("Error: Wrong parameters, use ./mobile_user {plafond inicial} {número de pedidos de autorização} {intervalo VIDEO} {intervalo MUSIC} {intervalo SOCIAL} {dados a reservar}\n");
        if (fflush(stdout) != 0)
            perror("Error flushing stdout");
        exit(1);
    }

    // check if the parameters are numbers
    for (int i = 1; i < 7; i++)
        for (int j = 0; j < strlen(argv[i]); j++)
            if (!isdigit(argv[i][j])) {
                printf("Error: invalid paramter %d, must be a number\n", i);
                if (fflush(stdout) != 0)
                    perror("Error flushing stdout");
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

    // create the 4 threads
    pthread_t video_thread, music_thread, social_thread, alerts_thread;
    if (pthread_create(&alerts_thread, NULL, print_alerts, NULL))
        perror("Error creating alerts thread");
    if (pthread_create(&video_thread, NULL, reserve_video_data, NULL))
        perror("Error creating video thread");
    if (pthread_create(&music_thread, NULL, reserve_music_data, NULL))
        perror("Error creating music thread");
    if (pthread_create(&social_thread, NULL, reserve_social_data, NULL))
        perror("Error creating social thread");
    if (pthread_join(social_thread, NULL))
        perror("Error joining social thread");
    if (pthread_join(music_thread, NULL))
        perror("Error joining music thread");
    if (pthread_join(video_thread, NULL))
        perror("Error joining video thread");

    if (pthread_join(alerts_thread, NULL))
        perror("Error joining alerts thread");

    exit(0);
}
