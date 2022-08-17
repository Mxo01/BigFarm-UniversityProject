#include "xerrori.h"

// ---Indirizzo e porta del socket---
#define HOST "127.0.0.1" 
#define PORT 57595

int main(int argc, char *argv[]) {

	ssize_t r1; // variabile che contiene il tipo restituito dalla readn (type1)
	ssize_t r2; // variabile che contiene il tipo restituito dalla readn (type2)
	ssize_t w1; // variabile che contiene il tipo restituito dalla writen (type1)
	ssize_t w2; // variabile che contiene il tipo restituito dalla writen (type2)
	
	char *endptr; // variabile che serve alla funzione strtol() per memorizzare il primo carattere non valido

	// ---Caso in cui il Client viene invocato senza argomenti sulla linea di comando---
	if (argc == 1) {
		// ---Creo una socket che permette di comunicare con il server Collector---
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
		
		int type2 = htonl(2); // tipo speciale di richiesta
		w2 = writen(fd_skt, &type2, sizeof(type2)); // scrivo nella socket il tipo della richiesta di comunicazione tramite writen
		if (w2!=sizeof(int)) xtermina("Errore write (type2)", __LINE__, __FILE__);
		
		char *nomefile; // dichiaro la stringa contenente il nome del file
		int tmp, pairs; 
		r2 = readn(fd_skt, &tmp, 4); // leggo dalla socket il numero di coppie
		if (r2!=sizeof(int)) xtermina("Errore read (type2)", __LINE__, __FILE__);
		pairs = ntohl(tmp); // ricavo il numero di coppie convertendo quello che ho letto
		if (pairs==0) printf("Nessun file\n");
		for (int i=0; i<pairs; i++) {
			int tmpL, len, tmpI1, s1, tmpI2, s2, tmpF, lenF; // variabili temporanee
			r2 = readn(fd_skt, &tmpI1, 4); // leggo dalla socket i primi 4 byte della somma
			if (r2!=sizeof(int)) xtermina("Errore read (type2)", __LINE__, __FILE__);
			s1 = ntohl(tmpI1); // ricavo i primi 4 byte della somma convertendo quello che ho letto
			r2 = readn(fd_skt, &tmpI2, 4); // leggo dalla socket gli ultimi 4 byte della somma
			if (r2!=sizeof(int)) xtermina("Errore read (type2)", __LINE__, __FILE__);
			s2 = ntohl(tmpI2); // ricavo gli ultimi 4 byte della somma convertendo quello che ho letto
			long somma = ((long)s1 << 32) + ((long)s2 & 0x00000000ffffffff); // unisco i due interi in un unico long da 8 byte, estendendo s1 e shiftandolo di 32 bit a sinistra e sommandolo a s2 esteso e messo in and con la maschera 0x00000000ffffffff che ad ogni 0 corrispondono quattro bit a zero e ad ogni f corrispondono quattro bit a uno, con un totale di 64 bit (ovvero 8 byte)
			r2 = readn(fd_skt, &tmpF, 4); // leggo dalla socket il numero di file relativi alla somma
			if (r2!=sizeof(int)) xtermina("Errore read (type2)", __LINE__, __FILE__);
			lenF = ntohl(tmpF); // ricavo il numero di file relativi alla somma
			for (int i=0; i<lenF; i++) {
				r2 = readn(fd_skt, &tmpL, 4); // leggo dalla socket la lunghezza del nome del file
				if (r2!=sizeof(int)) xtermina("Errore read (type2)", __LINE__, __FILE__);
				len = ntohl(tmpL); // ricavo la lunghezza del nome del file convertendo quello che ho letto
				nomefile = malloc((len+1)*sizeof(char)); // alloco la dimensione della stringa contenente il nome del file
				nomefile[len] = 0; // aggiungo in ultima posizione lo zero '\0'
				r2 = readn(fd_skt, nomefile, len); // leggo dalla socket il nome del file
				if (r2!=len) xtermina("Errore read (type2)", __LINE__, __FILE__);
				printf("%ld %s \n", somma, nomefile); //stampo la coppia (somma, nomefile)
				free(nomefile); // dealloco la memoria allocata con la malloc del nome del file
			}
		}
		if (close(fd_skt)<0) perror("Errore chiusura socket"); // chiudo il file descriptor del socket
	}	
	
	// ---Per ogni long letto da linea di comando chiedo al server le coppie con una data somma---
	for (int i=1; i<argc; i++) {
		// ---Creo una socket che permette di comunicare con il server Collector---
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
	
		int type1 = htonl(1); // tipo normale di richiesta 
		w1 = writen(fd_skt, &type1, sizeof(type1)); // scrivo nella socket il tipo della richiesta di comunicazione tramite writen
		if (w1!=sizeof(int)) xtermina("Errore write (type1)", __LINE__, __FILE__);

		long sum = strtol(argv[i], &endptr, 10); // converto la stringa che leggo da linea di comando (argv[i]) in un long

		// ---Controllo che strtol() sia andata a buon fine---
		if (endptr == argv[i]) // se strtol() non legge long inserisce in endptr il carettere non valido
      fprintf(stderr, "Argomenti passati al client errati, '%s' non è una somma valida\n", argv[i]);

		// ---Spezzo il long somma in due int dato che in C non esiste nessuna funzione che garantisce il passaggio di 8 byte---
		int tmp1, tmp2; // variabili temporanee per contenere le due parti di somma
		tmp2 = sum; // tmp2 contiene i 32 bit (4 byte) meno significativi di somma
		tmp1 = sum >> 32; // tmp1 contiene i 32 bit (4 byte) piu' significativi di somma
		int s1 = htonl(tmp1); // converto in big endian
		int s2 = htonl(tmp2); // converto in big endian
		
		w1 = writen(fd_skt, &s1, sizeof(s1)); // scrivo nella socket la prima parte (Msb) della somma tramite writen
		if (w1!=sizeof(int)) xtermina("Errore write (somma1)", __LINE__, __FILE__);
		w1 = writen(fd_skt, &s2, sizeof(s2)); // scrivo nella socket la seconda parte (Lsb) somma tramite writen
		if (w1!=sizeof(int)) xtermina("Errore write (somma2)" , __LINE__, __FILE__);
		
		int tmp, pairs;
		r1 = readn(fd_skt, &tmp, 4); // leggo dalla socket il numero di coppie
		if (r1!=sizeof(int)) xtermina("Errore read (type1)", __LINE__, __FILE__);
		pairs = ntohl(tmp); // ricavo il numero di coppie convertendo quello che ho letto
		if (pairs==0) printf("Nessun file\n");
		for (int i=0; i<pairs; i++) {
			int tmpL, len;
			r1 = readn(fd_skt, &tmpL, 4); // leggo dalla socket la lunghezza del nome del file
			if (r1!=sizeof(int)) xtermina("Errore read (type1)", __LINE__, __FILE__);
			len = ntohl(tmpL); // ricavo la lunghezza del nome del file convertendo quello che ho letto
			char *nomefile = malloc((len+1)*sizeof(char)); // alloco la dimensione della stringa contenente il nome del file
			nomefile[len] = 0; // aggiungo in ultima posizione lo zero '\0'
			r1 = readn(fd_skt, nomefile, len); // leggo dalla socket il nome del file
			if (r1!=len) xtermina("Errore read (type1)", __LINE__, __FILE__);
			printf("%ld %s \n", sum, nomefile); //stampo la coppia (somma, nomefile)
			free(nomefile); // dealloco la memoria allocata con la malloc del nome del file
		}
		if (close(fd_skt)<0) perror("Errore chiusura socket"); // chiudo il file descriptor del socket
	}

	return 0;
}