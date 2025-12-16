#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>

#define MY_MSG_SIZE 128

key_t key;
int shmid;
int semid;

//------------- STRUKTURY I UNIA ---------------------
typedef struct {
	char user[32];
	char msg[MY_MSG_SIZE];
	int likes;
	int active;
} tweet_t;

typedef struct {
	int n; // liczba slot�w
	tweet_t tweets[100];
} shared_data_t;

union semun {
	int val;
	struct semid_ds *buf;
	unsigned short *array;
};

//------------- FUNKCJE SEMAFOROWE ---------------------
void sem_wait(int i) {
	struct sembuf op = {i, -1, 0};
	if(semop(semid, &op, 1) == -1) {
		perror("[Error]: sem_wait");
		exit(1);
	}
}

void sem_signal(int i) {
	struct sembuf op = {i, +1, 0};
	if(semop(semid, &op, 1) == -1) {
		perror("[Error]: sem_signal");
		exit(1);
	}
}

int main(int argc, char* argv[])
{
	if(argc < 3) {
		printf("[Error]: Zla liczba argumentow!\n");
		printf("Uzycie(dodanie tweeta): ./%s <plik> N <nazwa_uzytkownika>\n", argv[0]);
		printf("Uzycie(polubienie tweeta): ./%s <plik> P\n", argv[0]);
		exit(1);
	}

	char mode = argv[2][0]; // N lub P

	printf("Twitter 2.0 wita! (wersja A)\n");

	key = ftok(argv[1], 1);
	if(key == -1){ perror("[Error]: Tworzenie klucza!"); exit(1);}

	// Pobranie pamiaci wspoldzielonej
	shmid = shmget(key, 0, 0);
	if(shmid == -1) { perror("[Error]: Pamiec wspoldzielona!"); exit(1);}

	shared_data_t *shared_data = (shared_data_t*) shmat(shmid, NULL, 0);
	if(shared_data == (void*) -1) { perror("[Error]: shmat"); exit(1);}

	// Pobranie semaforow
	semid = semget(key, 0, 0);
	if(semid == -1) { perror("[Error]: semget"); exit(1);}

	int n = shared_data->n; // liczba slotow z pamieci wspoldzielonej

	// Liczenie wolnych slotow
	int free_slots = 0;
	for(int i=0; i<n; i++)
		if(shared_data->tweets[i].active == 0)
			free_slots++;

	printf("Wolnych %d wpisow (na %d)\n", free_slots, n);

	// Tryb "N" - dodanie wpisu
	if(mode == 'N') {
		if(argc < 4) { perror("[Error]: Brak nazwy uzytkownika!"); exit(1);}

		char username[32];
		strncpy(username, argv[3], 31);
		username[31] = '\0';

		char text[MY_MSG_SIZE];
		printf("Podaj swoj wpis:\n");
		fgets(text, MY_MSG_SIZE, stdin);
		text[strcspn(text, "\n")] = 0; // usuwa znak nowej linii

		int inserted = -1;
		for(int i = 0; i < n; i++) {
			sem_wait(i);
			if(shared_data->tweets[i].active == 0) {
				strncpy(shared_data->tweets[i].user, username, 31);
				shared_data->tweets[i].user[31] = '\0';

				strncpy(shared_data->tweets[i].msg, text, MY_MSG_SIZE - 1);
				shared_data->tweets[i].msg[MY_MSG_SIZE-1] = '\0';

				shared_data->tweets[i].likes = 0;
				shared_data->tweets[i].active = 1;
				inserted = i;
			}
			sem_signal(i);
			if(inserted != -1) break;
		}

		if(inserted == -1) {
			printf("Brak miejsca na wpis!\n");
		} else {
			printf("Dodano wpis!\n");
			printf("Dziekuje za skorzystanie z aplikacji Twitter 2.0\n");
		}
		return 0;
	}

	// Tryb "P" - wypisanie + polubienie
	if(mode == 'P') {
		printf("Wpisy w serwisie:\n");
		for(int i = 0; i < n; i++) {
			sem_wait(i);
			if(shared_data->tweets[i].active) {
				printf("[%d] %s: \"%s\" (likes: %d)\n",
					i + 1,
					shared_data->tweets[i].user,
					shared_data->tweets[i].msg,
					shared_data->tweets[i].likes);
			}
			sem_signal(i);
		}

		int which;
		printf("Ktory wpis chcesz polubic: ");
		scanf("%d", &which);
		which--; // aby zachowac poprawnosc idx

		if(which < 0 || which >= n || shared_data->tweets[which].active == 0) {
			printf("Wpis nie istnieje.\n");
		} else {
			sem_wait(which);
			shared_data->tweets[which].likes++;
			sem_signal(which);
			printf("Polubiono wpis nr %d.\n", which + 1);
			printf("Dziekuje za skorzystanie z aplikacji Twitter 2.0\n");
		}
		return 0;
	}

	perror("[Error]: Nieznana opcja. Uzyj N lub P\n");
	return 1;
}

