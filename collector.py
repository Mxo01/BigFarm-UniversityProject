#! /usr/bin/python3

import sys, struct, socket, threading 

# ---Indirizzo e porta del socket---
HOST = "127.0.0.1"  # (localhost)
PORT = 57595 

# ---Variabile globale che contiene le coppie (somma, [nomefie1, nomefile2, ...])---
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
	# ---Se nel dizionario ho già inserito un file per una determinata somma e non ho già inserito il quel file---
	if somma in pair_table and not(nomefile in pair_table[somma]): # evito di avere duplicati
		pair_table[somma].append(nomefile) # inserisco nel dizionario la coppia
	else:
		pair_table[somma] = [nomefile] # inserisco per la prima volta una somma associata ad una chiave

# ---Funzione ausiliaria che permette di gestire la richiesta da parte del client di elencare
# le coppie con una data somma---
def gestisci_client1(conn,addr):
	tmpS = recv_all(conn,8) # leggo la somma dalla socket
	assert len(tmpS)==8 # controllo di aver letto effettivamente 8 byte
	sum = struct.unpack("!q", tmpS)[0] # converto la somma 
	# ---Se la somma è contenuta nel dizionario mando le coppie---
	if sum in pair_table:
		pairs = len(pair_table[sum]) # numero di coppie con una data somma
		conn.sendall(struct.pack("!i", pairs)) # mando il numero di coppie
		for k in pair_table[sum]: # per ogni nomefile all'interno della lista relativa ad una determinata somma
			conn.sendall(struct.pack("!i", len(k))) # mando la lunghezza del nome del file
			conn.sendall(k.encode()) # mando il nome del file
			# conn.sendall(struct.pack("!q", sum)) # mando la somma
	# ---Se la somma non è contenuta nel dizionario mando 0 come numero di coppie ovvero "Nessun file"---
	else:
		pairs = 0
		conn.sendall(struct.pack("!i", pairs)) # mando il numero di coppie

# ---Funzione ausiliaria che permette di gestire la richiesta da parte del client di elencare
# tutte le coppie---
def gestisci_client2(conn,addr):
	# ---Calcolo il numero totale di coppie---
	keys = list(pair_table) # array di somme 
	keys.sort() # ordino l'array
	conn.sendall(struct.pack("!i", len(keys))) # mando il numero di somme
	for s in keys:
		conn.sendall(struct.pack("!q", s)) # mando la somma al client
		files = pair_table[s] # array di file realtivi alla somma s
		conn.sendall(struct.pack("!i", len(files))) # mando il numero di file relativi alla somma s
		for f in files:
			conn.sendall(struct.pack("!i", len(f))) # mando la lunghezza del nome del file
			conn.sendall(f.encode()) # mando il nome del file

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