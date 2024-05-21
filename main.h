/* Cabeceras para manipulación de estructuras de datos y memoria */
#include <stdio.h>     // Entrada/salida estándar (fprintf y demás)
#include <stdlib.h>    // Buena y vieja biblioteca estándar
#include <unistd.h>    // API POSIX (close, open, etc)
#include <stdint.h>    // Definiciones de tipos portátiles (uint32_t, etc)
#include <string.h>    // memset y manipulación general de cadenas
#include <sys/types.h> // Temporizadores, mutexes y otras cosas útiles
#include <fcntl.h>     // Manipulación de descriptores de archivo (sockopts, etc)

/* Cabeceras de red */
#include <netdb.h>      // addrinfo y otras automatizaciones de red
#include <sys/socket.h> // SOCKETS, AMAMOS LOS SOCKETS, ¿QUIÉN NO AMA LOS SOCKETS?
#include <arpa/inet.h>  // inet_ntoa, inet_aton y otras

/* Cabeceras de multi-hilo */
#include <pthread.h> // Hilos y cosas relacionadas

/* Inicializa un socket TCP para la dirección IP de un par en el puerto 51511, establece la
   conexión TCP con el par y devuelve el ID del descriptor de archivo del socket.
   Devuelve -1 si no puede configurar la conexión. */
int init_peer_socket(char *ip);

/* Inicializa un socket TCP que se enlaza a la dirección local y devuelve su
   ID de descriptor de archivo. Este socket se utilizará para aceptar conexiones entrantes
   de otros pares. Devuelve -1 si falla. */
int init_incoming_socket();

/* Procesa un mensaje de PeerList recibido en el socket dado, verificando si hay
   algún par al que no estemos conectados actualmente, y conectándose a cualquier nuevo
   par potencial. */
void process_peerlist(int peersock, FILE *logfile);

/* Procesa una respuesta de archivo recibida en el socket dado. Primero, analizamos y
   almacenamos el contenido del archivo recibido de manera adecuada. Luego, verificamos si el
   nuevo archivo es más grande que el actualmente activo. Si es así, validamos este nuevo archivo.
   Si la verificación de validez es exitosa, reemplazamos el archivo actual por el nuevo y eliminamos
   el archivo antiguo. */
void process_archive(int peersock, FILE *logfile);

/* Publica un archivo recién creado iterando sobre la lista de pares y enviando
   el archivo activo actual a cada par. Esta función puede parecer extraña porque
   todos los datos a los que accede están contenidos en nuestras dos estructuras de datos globales,
   la estructura de lista de pares y la estructura de archivo activo. */
void publish_archive();

/* Implementa el trabajo realizado por los hilos lanzados para cada par, que periódicamente
   envían mensajes de solicitud de par ("0x1") al par conectado. Toma el socket
   asociado al par como entrada y simplemente entra en un bucle infinito, enviando
   mensajes de solicitud a intervalos regulares (5 segundos).
   Como un bono, dado que la especificación no menciona cuándo debemos enviar solicitudes de archivo,
   también las enviaremos periódicamente, en un intervalo más largo (cada 60 segundos). */
void *peer_requester_thread(void *sock);

/* Implementa el trabajo realizado por los hilos lanzados para cada par que reciben y
   procesan los datos enviados por el par conectado. Toma el socket asociado al
   par como entrada y usa recv() en el socket, esperando que lleguen mensajes, y
   procesa cada tipo de mensaje según corresponda.
   El socket está configurado con un tiempo de espera. Si una operación recv() se agota, asumimos
   que la conexión fue interrumpida, cerramos el socket, desconectamos al par y lo eliminamos
   de la lista de pares conectados. */
void *peer_receiver_thread(void *sock);

/* Esta función implementa todo el trabajo que debe realizar el hilo que trata con
   las conexiones entrantes de pares. Inicializa un socket pasivo, lo enlaza,
   luego escucha y espera conexiones entrantes, aceptándolas y
   lanzando hilos para intercambiar datos con cada par.
   El hilo que ejecuta esta función se llamará una vez al comienzo de la ejecución
   del programa y se ejecutará indefinidamente. */
void *incoming_peers_thread();
