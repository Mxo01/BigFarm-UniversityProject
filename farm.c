#include "xerrori.h"

// ---Indirizzo e porta del socket---
#define HOST "127.0.0.1" 
#define PORT 57595

// ---Variabile globale "esterna" che contiene il valore resituito dalla funzione getopt()---
extern char *optarg;

// ---Variabile globale che permette di interrompere l'esecuzione di un while nel main alla ricezione del segnale SIGINT---
volatile sig_atomic_t continua = 1;

// ---Dimensione standard del buffer produttore/consumatori---
int Buf_size = 8; 

// ---Gestore del segnale SIGINT---
void handler(int s) {
	if (s==SIGINT) continua = 0; // se ricevo il segnale SIGINT setto continua a 0
	fprintf(stderr,"\nSegnale SIGINT ricevuto\n");
}

// ---Struct che definisce di cosa ha bisogno ogni thread worker---
typedef struct {
	char **buffer; // buffer produttore/consumatori
	int *index; // indice del buffer
	pthread_mutex_t *mutex; // mutex per accedere in mutua esclusione al buffer
  sem_t *sem_free_slots; // slot liberi
  sem_t *sem_data_items;  // slot utilizzati
} workers;

// ---Corpo che esegue ogni thread---
void *tbody(void *arg) { 
	workers *w = (workers *) arg; // conversione di tipo del parametro formale
	ssize_t x; // variabile che memorizza il risultato della writen
	int i = 0; // è l'indice del do while
	long n, somma = 0; // n contiene l'i-esimo long del file corrente
	char *nomefile; // nome del file
	FILE *f; // dichiaro un file che proverò ad aprire dopo

	while (true) {
		// ---Ogni thread esegue lock e unlock così accedono al buffer uno alla volta---
		xsem_wait(w->sem_data_items,__LINE__,__FILE__);
		xpthread_mutex_lock(w->mutex, __LINE__, __FILE__); 
		nomefile = w->buffer[*(w->index) % Buf_size]; // leggo gli elementi dal buffer
		*(w->index) += 1; // incremento l'indice del buffer per leggere l'elemento successivo 	
		xpthread_mutex_unlock(w->mutex, __LINE__, __FILE__);
		xsem_post(w->sem_free_slots,__LINE__,__FILE__);

		if (strcmp("<end>", nomefile)==0) break; // esco dal while in quanto sono arrivato alla fine

		// ---Provo ad aprire il file in modalità rb ovvero lettura binaria---
		if ((f = fopen(nomefile, "rb"))==NULL) {
			fprintf(stderr, "Il file '%s' non esiste o non è presente nella directory\n", nomefile); // se il file non esiste stampo un errore
			continue; // controllo che l'elemento che sto aprendo sia effettivamente un file
		}
		do {
			size_t e = fread(&n, sizeof(n),  1, f); // leggo l'i-esimo long del file e lo memorizzo in n
			if (e!=1) break; // se arrivo alla fine mi fermo
			somma += i*n; // calcolo la somma per ogni file
			i++; // incremento i
		} while (true);

		if (fclose(f)!=0) perror("Errore chiusura file"); // chiudo il file
		
		// ---Creo una socket che manda la coppia (nomefile, somma) al server Collector---
		int fd_skt; // file descriptor del socket
		struct sockaddr_in serv_addr; // struct che permette di lavorare con indirizzi internet
	
		// ---Creo il socket---
		if ((fd_skt = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
			xtermina("Socket creation error", __LINE__, __FILE__);

		// ---Assegna l'indirizzo---
		serv_addr.sin_family = AF_INET; // uso come family Internet
		serv_addr.sin_port = htons(PORT); // determino la porta
		serv_addr.sin_addr.s_addr = inet_addr(HOST); // determino l'host

		// ---Apre la connessione---
		if (connect(fd_skt, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
			xtermina("Errore connessione", __LINE__, __FILE__);

		// ---Mando la coppia (nomefile, somma) attraverso la socket---
		int type = htonl(0); // tipo di richiesta dei worker
		x = writen(fd_skt, &type, sizeof(type)); // scrivo nella socket il tipo di richiesta indicata dall'intero "0" tramite writen
		if (x!=sizeof(int)) xtermina("Errore write (tipo)", __LINE__, __FILE__);
		
		int fileLen = htonl(strlen(nomefile)); // calcolo la lunghezza della stringa contenuta in nomefile
		x = writen(fd_skt, &fileLen, sizeof(fileLen)); // scrivo nella socket la lunghezza del nome del file tramite writen
		if (x!=sizeof(int)) xtermina("Errore write (lunghezza file)", __LINE__, __FILE__);
		x = writen(fd_skt, nomefile, ntohl(fileLen)); // scrivo nella socket il nome del file tramite writen
		if (x!=ntohl(fileLen)) xtermina("Errore write (file)", __LINE__, __FILE__);

		// ---Spezzo il long somma in due int dato che in C non esiste nessuna funzione che garantisce il passaggio di 8 byte---
		int tmp1, tmp2; // variabili temporanee per contenere le due parti di somma
		tmp2 = somma; // tmp2 contiene i 32 bit (4 byte) meno significativi di somma
		tmp1 = somma >> 32; // tmp1 contiene i 32 bit (4 byte) piu' significativi di somma
		int s1 = htonl(tmp1); // converto in big endian
		int s2 = htonl(tmp2); // converto in big endian
		
		x = writen(fd_skt, &s1, sizeof(s1)); // scrivo nella socket la prima parte (Msb) della somma tramite writen
		if (x!=sizeof(int)) xtermina("Errore write (somma1)", __LINE__, __FILE__);
		x = writen(fd_skt, &s2, sizeof(s2)); // scrivo nella socket la seconda parte (Lsb) somma tramite writen
		if (x!=sizeof(int)) xtermina("Errore write (somma2)" , __LINE__, __FILE__);

		if (close(fd_skt)<0) perror("Errore chiusura socket"); // chiudo il file descriptor del socket
	}
	pthread_exit(NULL); // Termina l'esecuzione del thread
}

int main(int argc, char *argv[]) {
	
	// ---Gestione del segnale SIGINT---
	struct sigaction sa; // struct che contiene il modo di gestione di un determinato segnale
	sigaction(SIGINT, NULL, &sa); // inizializzo sa al valore iniziale
	sa.sa_handler = &handler; // garantisce che il comportamento usato dalla struct sa per la gestione del segnale SIGINT sia quello definito nella funzione handler
	sigaction(SIGINT, &sa, NULL); // handler per Control-C 
	
  // ---Controlla numero argomenti---
  if (argc<2) {
    printf("Uso: %s file [file ...] \n",argv[0]);
    return 1;
  }

	// ---Prendo i parametri opzionali tramite getopt()---
	char *endptr;
	int opt, nthreads = 4, delay = 0, numopt = 0;
	while ((opt = getopt(argc, argv, "n:q:t:")) != -1) {
		switch(opt) {
			case 'n':
				nthreads = atoi(optarg); // numero di threads
				numopt++; // incremento il numero di parametri opzionali
				break;
			case 'q':
				Buf_size = atoi(optarg); // dimensione del buffer
				numopt++; // incremento il numero di parametri opzionali
				break;
			case 't':
				strtol(optarg, &endptr, 10); // converto il parametro opzionale 
				if (endptr == optarg) xtermina("Errore Delay, argomento passato non valido", __LINE__, __FILE__); // se il parametro opzionale non è valido termino il processo e stampo un errore 
				else {
					delay = atoi(optarg); // delay di scrittura nel buffer
					numopt++; // incremento il numero di parametri opzionali
				}
				break;
			default: xtermina("Flag passato non valido", __LINE__, __FILE__); // se il parametro che passo non esiste termino
			break;
		}
	}

	// ---Controllo che i valori siano validi---
	assert(nthreads>0);
	assert(Buf_size>0);
	assert(delay>=0);

	char *buffer[Buf_size]; // buffer produttore/consumatori
  int pindex = 0, cindex = 0; // indici produttore/consumatori
	pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER; // inizializzo il mutex al valore di default
  pthread_t t[nthreads]; // dichiaro gli n threads
  workers w[nthreads]; // dichiaro gli n worker
  sem_t sem_free_slots, sem_data_items; // semafori per gestire il buffer produttore/consumatori
	// ---Inizializzo i semafori---
  xsem_init(&sem_free_slots, 0, Buf_size, __LINE__, __FILE__); 
  xsem_init(&sem_data_items, 0, 0, __LINE__, __FILE__);

	for (int i=0; i<nthreads; i++) {
    // ---Faccio partire il thread i---
    w[i].buffer = buffer;
		w[i].index = &cindex;
		w[i].mutex = &mutex;
    w[i].sem_data_items = &sem_data_items;
    w[i].sem_free_slots = &sem_free_slots;
    xpthread_create(&t[i], NULL, &tbody, &w[i], __LINE__, __FILE__);
  }
	
	// ---Leggo i nomi dei file e li passo ai worker attraverso il buffer---
	int i=2*numopt+1; // dato che in argv prima vengono memorizzati i parametri opzionali, decido di leggere solo i file tramite questa formula che mi permette di saltare i numopt parametri opzionali (+1 perchè salto anche il nome del processo che chiamo "./farm" e 2* perchè per ogni parametro opzionale corrisponde un determinato valore)
	while (continua && i<argc) {
		xsem_wait(&sem_free_slots,__LINE__,__FILE__);
		buffer[pindex++ % Buf_size] = argv[i]; // inserisco nel buffer solo i file
		xsem_post(&sem_data_items,__LINE__,__FILE__);
		i++; // incremento l'indice
		usleep(delay*1000); // aspetto tra una scrittura e l'altra
	}
  
	// ---Inserisco nel buffer un carattere speciale che indica la terminazione---
	for (int i=0; i<Buf_size; i++) {
		xsem_wait(&sem_free_slots,__LINE__,__FILE__);
		buffer[pindex++ % Buf_size] = "<end>";
		xsem_post(&sem_data_items,__LINE__,__FILE__);
	}
	
	// ---Join dei thread e calcolo del risultato---
	for (int i=0; i<nthreads; i++) 
    xpthread_join(t[i], NULL, __LINE__, __FILE__);

	// ---Deallocazioni---
	xpthread_mutex_destroy(&mutex, __LINE__, __FILE__);
	if (sem_destroy(&sem_free_slots)!=0) perror("Errore sem_destroy");
	if (sem_destroy(&sem_data_items)!=0) perror("Errore sem_destroy");;

	return 0;
}