#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <unistd.h>

#define MY_MSG_SIZE 128
#define MAX_TWEETS 100

key_t shmkey;
int shmid;
int semid;

//--------------------- STRUKTURY ---------------------
typedef struct {
	char user[32];
	char msg[MY_MSG_SIZE];
	int likes;
	int active; // Sprawdzanie wolnych slotow
} tweet_t;

typedef struct {
	int n; // liczba slotow
	tweet_t tweets[MAX_TWEETS];
} shared_data_t;

shared_data_t *shared_data; // wskaznik na pamiec wspoldzielona

union semun {
	int val;
	struct semid_ds *buf;
	unsigned short *array;
};

// -----------------------FUNKCJA SHM -------------------------------------
int create_shm(key_t key, int n, shared_data_t **ptr) {
	int size = sizeof(shared_data_t);
	int id = shmget(key, size, IPC_CREAT | IPC_EXCL | 0666);
	struct shmid_ds buf;

	if(id == -1) { perror("[Error]: Tworzenie segmentu pamieci!"); exit(1); }

	shmctl(id, IPC_STAT, &buf);
	printf(" OK (id: %d rozmiar: %zub)\n", id, buf.shm_segsz);

	*ptr = (shared_data_t*) shmat(id, NULL, 0);
	if(*ptr == (shared_data_t*)-1) { perror("[Error]: Dolaczanie pamieci wspolnej!"); exit(1);}

	// ustawienie liczby slotow i zerowanie aktywnosci
	(*ptr)->n = n;
	for(int i = 0; i < n; i++) (*ptr)->tweets[i].active = 0;

	return id;
}

// -----------------------FUNKCJE SEMAFOROWE -------------------------------------
void init_semaphores(int n, key_t key) {
	semid = semget(key, n, IPC_CREAT | 0666);
	if(semid == -1) { perror("[Error]: Tworzenie semaforow"); exit(1); }

	unsigned short vals[n];
	for(int i = 0; i < n; i++) vals[i] = 1; // ustawianie na 1

	if(semctl(semid, 0, SETALL, vals) == -1) { perror("[Error]: Inicjalizacja semaforow"); exit(1); }
}

void sem_wait(int i) {
	struct sembuf op = {i, -1, 0};
	if(semop(semid, &op, 1) == -1) { perror("[Error]: sem_wait"); exit(1);}
}

void sem_signal(int i) {
	struct sembuf op = {i, +1, 0};
	if(semop(semid, &op, 1) == -1) { perror("[Error]: sem_signal"); exit(1); }
}

// -----------------------FUNKCJE SERWERA ---------------------------------------
void show_tweets(shared_data_t* shared_data) {
	int n = shared_data->n;

	printf("_______ Twitter 2.0: _______\n");
	for(int i = 0; i < n; i++) {
		sem_wait(i);
		if(shared_data->tweets[i].active) {
			printf("%d. %s: \"%s\" (likes: %d)\n", i + 1,
			       shared_data->tweets[i].user,
			       shared_data->tweets[i].msg,
			       shared_data->tweets[i].likes);
		}
		sem_signal(i);
	}
	printf("\n");
}

// ------------------------ Handler ------------------------
void handler(int signal) {
	printf("\n[Serwer]: dostalem SIGINT => koncze i sprzatam...");
	printf(" (odlaczenie shm: %s, usuniecie shm: %s, usuniecie sem: %s\n",
	       (shmdt(shared_data) == 0) ? " OK" : "[Error]: shmdt",
	       (shmctl(shmid, IPC_RMID, 0) == 0) ? "OK" : "[Error]: shmctl",
	       (semctl(semid, 0, IPC_RMID) == 0) ? "OK" : "[Error]: semctl");
	exit(0);
}

// -----------------------   MAIN   -------------------------------------
int main(int argc, char* argv[])
{
	if(argc != 3) {
		fprintf(stderr,"[Error]: Zla ilosc argumentow!\n Uzycie: %s <plik> <ilosc_wpisow>\n", argv[0]);
		return 1;
	}

	signal(SIGINT, handler);

	printf("[Serwer]: Twitter 2.0 (wersja A)\n");

	// Tworzenie klucza
	printf("[Serwer]: tworze klucz na podstawie pliku ./%s...", argv[1]);
	if((shmkey = ftok(argv[1], 1)) == -1) {
		perror("[Error]: Tworzenie klucza!");
		exit(1);
	}
	printf(" OK (klucz: %d)\n", shmkey);

	// Tworzenie segmentu pamieci
	int n = atoi(argv[2]);
	if(n > MAX_TWEETS) n = MAX_TWEETS;
	printf("[Serwer]: tworze segment pamieci wspolnej na %d wpisow po %db...", n, MY_MSG_SIZE);
	shmid = create_shm(shmkey, n, &shared_data);

	// Inicjalizacja semaforow
	init_semaphores(n, shmkey);

	printf("[Serwer]: dolaczam pamiec wspolna... OK (adres: %lX)\n", (long int)shared_data);
	printf("[Serwer]: nacisnij Ctrl^C by zakonczyc program\n");

	// Glowna petla serwera
	while(1) {
		printf("\nZaktualizowano!\n");
		show_tweets(shared_data);

		printf("\nAktualizuje wpisy (5s)\n");
		fflush(stdout);
		for(int i = 0; i < 5; i++)
		{
			printf(".");
			fflush(stdout);
			sleep(1);
		}
		// Powrot na poczatek linii i czyszczenie
		printf("\r");
		printf("                     \r"); // nadpisanie calej linii spacjami
		fflush(stdout);
	}

	return 0;
}

