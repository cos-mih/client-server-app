APLICATIE CLIENT-SERVER PESTE TCP SI UDP PENTRU GESTIONAREA MESAJELOR

S-a implementat o aplicatie simpla tip client-server pentru gestionarea unor mesaje, peste protocoalele TCP
si UDP.

Structura temei este urmatoarea:

list.c, list.h -> implementarea unei liste generice simplu inlantuite pentru a permite stocarea unui
                  numar variabil de clienti si mesaje in cadrul aplicatiei;

common.c, common.h -> implementarea structurilor care reprezinta mesajele recunoscute in retea si a
                      functiilor de trimitere si primire de mesaje peste protocolul TCP:
                      send_all() si recv_all() se folosesc de functiile de baza send() si recv() pentru
                      a unifica si separa octetii trimisi/primiti in mesaje interpretabile;

subscriber.c -> implementarea unui client TCP care poate trimite catre server request-uri de logare si
                abonare, respectiv dezabonare de la un anumit topic de mesaje, si interpreta mesajele
                primite pe respectivele topic-uri de la server;
        -rulare: ./suscriber <id> <ip server> <port server>

server.c -> implementarea unui server care primeste mesaje printr-un socket UDP de la clienti UDP (mesaje
            cu topic, data_type si continut, conform cerintei), requesturi de la clientii TCP, si redirectioneaza
            mesajele de la clientii UDP catre clientii TCP corespunzatori;
        - rulare: ./server <port>

Este implementata si functionalitatea de store-and-forward: daca un client TCP se aboneaza la un topic cu
optiunea store-and-forward activata (sf = 1), in cazul in care se deconecteaza si se conecteaza apoi din nou,
va primi mesajele primite de server pe acel topic in timpul in care a fost deconectat.


Structurile mesajelor peste TCP este dupa cum urmeaza:

-> mesajele trimise de catre client la server (request-uri) sunt formate dintr-un header
   de meta informatii despre request (tipul request-ului si lungimea datelor efective),
   reprezentat de structura *request_header*, urmat de un packet de date specific fiecarui
   tip de request (*connect_packet* - pentru trimiterea catre server a id-ului unui client
   nou conectat, *subscribe_packet* - pentru abonarea la un anumit topic, *unsubscribe_packet*
   - pentru dezabonarea de la un topic); pe conexiunea TCP se va trimite intregul header,
   urmat de header->len octeti (cei relevanti, neincluzand octeti filler in stringuri) din
   pachetul corespunzator.

-> mesajele trimise de catre server la clienti (cele care contin informatia mesajelor primite
   de la clientii UDP) sunt formate tot dintr-un header de meta informatii despre mesaj,
   reprezentat de structura *content_header* (care contine lungimea stringului ce reprezinta
   topicul mesajului, numarul de octeti care reprezinta datele efective, tipul datelor
   transmise, precum si IP-ul si portul clientului UDP de la care provine mesajul), urmat de
   un payload ce contine titlul topicului in care se incadreaza mesajul, urmat de datele
   din mesaj; pe conexiunea TCP se va trimite intreg headerul, urmat de header->topic_len
   octeti care reprezinta stringul ce identifica topicul, si header->data_len octeti care
   reprezinta datele din mesaj (astfel se trimit doar octeti cu informatie, fara fillere).

Mai multe detalii despre toate campurile structurilor mentionate se gasesc in comentariile
din common.h.

"Baza de date" a serverului consta in doua liste, una de subscriberi si una de topicuri.
In lista de subscriberi sunt retinute datele despre clientii TCP, in structuri de date de
tip *subscriber*, care identifica un client print socketul la care acesta este conectat,
id, IP, port, starea de conectivitate si eventualele mesaje primite cat a fost deconectat.

Lista de topicuri contine elemente de tip *topic*, care reprezinta o anumita categorie de
mesaje, identificate prin titlu si o lista de abonari. O abonare consta intr-o pereche
subscriber - sf, unde subscriber este un pointer la clientul abonat, iar sf este optiunea
de store-and-forward asociata abonarii.

Detalii ale logicii aplicatiei se gasesc in comentariile din cod.