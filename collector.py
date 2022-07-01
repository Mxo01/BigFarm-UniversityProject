#! /usr/bin/python3

import sys, struct, socket, threading 

# ---Indirizzo e porta del socket---
HOST = "127.0.0.1"  # (localhost)
PORT = 57595 

# ---Variabile globale che contiene le coppie (nomefile, somma)---
pair_table = {} # tabella di coppie (dizionario)

def main(host=HOST, port=PORT):
	# ---Creo il server socket---
	with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
		s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
		s.bind((HOST, PORT)) # connetto il socket all' host e alla porta
		s.listen() # metto il socket in ascolto di richieste
		while True:
			# ---Mi metto in attesa di una connessione---
			# conn è l'oggetto che permette la comunicazione
			# addr è l'indirizzo da cui proviene la richiesta
			conn, addr = s.accept() 
			# ---Lavoro con la connessione appena ricevuta---
			tmp = recv_all(conn,4) # leggo i primi 4 byte che contengono il tipo di richiesta
			assert len(tmp)==4 # controllo di aver letto effettivamente 4 byte
			type = struct.unpack("!i", tmp)[0] # converto il bytearray in un intero
			if type == 0:
				# ---Richiesta da parte dei workers di memorizzare una data coppia (nomefile, somma)---
				# t è un thread che si occupa di gestire la connessione
				t = threading.Thread(target = gestisci_worker, args = (conn,addr))
				t.start() # faccio partire il thread
			elif type == 1:
				# ---Richiesta da parte del client di elencare le coppie con una data somma---
				# t è un thread che si occupa di gestire la connessione
				t = threading.Thread(target = gestisci_client1, args = (conn,addr))
				t.start() # faccio partire il thread
			elif type == 2:
				# ---Richiesta da parte del client di elencare tutte le coppie---
				# t è un thread che si occupa di gestire la connessione
				t = threading.Thread(target = gestisci_client2, args = (conn,addr))
				t.start() # faccio partire il thread
			else: 
				# ---Richiesta di chiudere il server---
				s.shutdown(socket.SHUT_RDWR)
				s.close()

# ---Funzione ausiliaria che permette di gestire la richiesta da parte dei worker---
def gestisci_worker(conn,addr):
	tmpL = recv_all(conn,4) # leggo la lunghezza del nome del file memorizzandola in un array di byte
	assert len(tmpL)==4 # controllo di aver letto effettivamente 4 byte
	l = struct.unpack("!i", tmpL)[0] # converto la lunghezza in un intero
	tmpN = recv_all(conn,l) # leggo il nome del file memorizzandolo in un array di byte
	assert len(tmpN)==l # controllo di aver letto effettivamente l byte
	nomefile = tmpN.decode() # converto il nome in una stringa
	tmpI = recv_all(conn,8) # leggo la somma memorizzandola in un array di byte
	assert len(tmpI)==8 # controllo di aver letto effettivamente 8 byte
	somma = struct.unpack("!q", tmpI)[0] # converto la somma in un long
	pair_table[nomefile] = somma # inserisco nel dizionario la coppia

# ---Funzione ausiliaria che permette di gestire la richiesta da parte del client di elencare
# le coppie con una data somma---
def gestisci_client1(conn,addr):
	tmpS = recv_all(conn,8) # leggo la somma dalla socket
	assert len(tmpS)==8 # controllo di aver letto effettivamente 8 byte
	sum = struct.unpack("!q", tmpS)[0] # converto la somma 
	d = {} # dizionario che conterrà le coppie con una data somma
	for k,v in pair_table.items():
		if sum == v:
			d[k] = v # inserisco nel dizionario tutte le coppie con una data somma
	pairs = len(d) # numero di coppie con una data somma
	conn.sendall(struct.pack("!i", pairs)) # mando il numero di coppie
	for k,v in sorted(d.items(), key=lambda x:x[1]): # Ordino il dizionario tramite il metodo sorted e il secondo parametro permette di ordinarlo per valori crescenti in quanto è una lambda espressione che per ogni elemento del primo parametro lo ordina in base ad x[1] che sarebbe il secondo elemento della coppia (nomefile, somma) all'interno del dizionario
		conn.sendall(struct.pack("!i", len(k))) # mando la lunghezza della chiave
		conn.sendall(k.encode()) # mando la chiave
		conn.sendall(struct.pack("!q", v)) # mando la somma

# ---Funzione ausiliaria che permette di gestire la richiesta da parte del client di elencare
# tutte le coppie---
def gestisci_client2(conn,addr):
	pairs = len(pair_table) # numero totale di coppie
	conn.sendall(struct.pack("!i", pairs)) # mando il numero di coppie
	# Per ogni coppia nel dizionario invio tale coppia al client
	for k,v in sorted(pair_table.items(), key=lambda x:x[1]): # Ordino il dizionario tramite il metodo sorted e il secondo parametro permette di ordinarlo per valori crescenti in quanto è una lambda espressione che per ogni elemento del primo parametro lo ordina in base ad x[1] che sarebbe il secondo elemento della coppia (nomefile, somma) all'interno del dizionario
		conn.sendall(struct.pack("!i", len(k))) # mando la lunghezza della chiave
		conn.sendall(k.encode()) # mando la chiave
		conn.sendall(struct.pack("!q", v)) # mando la somma

# riceve esattamente n byte e li restituisce in un array di byte
# il tipo restituto è "bytes": una sequenza immutabile di valori 0-255
# analoga alla readn che abbiamo visto nel C
def recv_all(conn,n):
  chunks = b''
  bytes_recd = 0
  while bytes_recd < n:
    chunk = conn.recv(min(n - bytes_recd, 1024))
    if len(chunk) == 0:
      raise RuntimeError("socket connection broken")
    chunks += chunk
    bytes_recd = bytes_recd + len(chunk)
  return chunks

# ---Gestione della chiamata al main in base ai parametri che vengono passati da linea di comando---
if len(sys.argv)==1:
  main() # se non scrivo nulla chiama main senza parametri
elif len(sys.argv)==2:
  main(sys.argv[1]) # se scrivo un parametro allora sara' l'host
elif len(sys.argv)==3: 
  main(sys.argv[1], int(sys.argv[2])) # se scrivo due parametri allora uso host e porta passati come parametro
else:
  print("Uso:\n\t %s [host] [port]" % sys.argv[0])