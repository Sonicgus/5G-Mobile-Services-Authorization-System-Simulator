# Gustavo Samuel de Alves e Bastos de André e Lima 2020217743
# João Maria Pereira Carvalho de Picado Santos 2022213725

CC = gcc
FLAGS = -pthread -Wall -g
SRC_DIR = src
TARGETS = 5g_auth_platform backoffice_user mobile_user

all: $(TARGETS)

$(TARGETS): %: $(SRC_DIR)/%.c
	$(CC) $(FLAGS) $< -o $@

clean:
	rm -f $(TARGETS)
