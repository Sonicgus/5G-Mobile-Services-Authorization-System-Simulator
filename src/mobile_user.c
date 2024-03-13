// Gustavo Samuel de Alves e Bastos de André e Lima 2020217743
// João Maria Pereira Carvalho de Picado Santos 2022213725

#include <stdio.h>
#include <stdlib.h>

// #define DEBUG //remove this line to remove debug messages
#define LOG_FILENAME "log.txt"

FILE *log_fp;

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

int main(int argc, char *argv[])
{
    // check if the number of parameters is correct
    if (argc != 7)
    {
        printf("Error: Wrong parameters, use ./mobile_user {plafond inicial} {número de pedidos de autorização} {intervalo VIDEO} {intervalo MUSIC} {intervalo SOCIAL} {dados a reservar}\n");
        exit(1);
    }

    start(argv[1]);

    finish();

    exit(0);
}
