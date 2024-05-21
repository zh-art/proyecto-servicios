#include "main.h"
#include "peerlist.h"
#include "archive.h"

/* El puerto siempre es 51511 */
#define TCP_PORT "51511"

/* Enum para tipos de mensajes, para hacer el código de tratamiento de mensajes más claro */
enum
{
	MSG_PEERREQ = 1,
	MSG_PEERLIST,
	MSG_ARCHREQ,
	MSG_ARCHRESP
};

/* La lista de pares conectados. Esto debe ser global para ser compartido entre todos
   los hilos (podríamos pasarla como parámetro, pero eso sería muy engorroso,
   así que simplificamos haciéndolo global)
   Debe ser seguro acceder a ella entre hilos porque main() la inicializa
   antes de lanzar cualquier hilo, y el acceso de los hilos está controlado a través
   de su variable mutex */
struct peer_list *peerlist;
pthread_mutex_t peerlist_mutex;

/* El archivo activo actual, que transmitiremos a cualquier par que envíe
   mensajes de solicitud de archivo. Debe ser global por las mismas razones que la lista de pares.
   Este será inicializado por el hilo principal tan pronto como comience la ejecución,
   y nos aseguramos de que contenga un archivo adecuado antes de transmitirlo.
   Para sincronizar archivos, utilizamos un rwlock en lugar de un mutex, porque solo
   1 hilo escribirá cambios en él (para agregar mensajes), mientras que otros hilos
   solo reemplazarán el archivo activo actual (lo que cuenta como escritura,
   pero no ocurrirá con frecuencia), o leerán valores como su tamaño o lo imprimirán. */
struct archive *active_arch;
pthread_rwlock_t archive_lock;

/* Dirección IP pública del dispositivo local, para evitar intentos de conexión a sí mismo */
uint32_t myaddr;

/* Inicializa un socket TCP para la dirección IP de un par en el puerto 51511, establece la
   conexión TCP con el par y devuelve el ID del descriptor de archivo del socket.
   Devuelve -1 si no puede configurar la conexión.
   Usamos select() y algo de magia no bloqueante para forzar un tiempo de espera de medio segundo en
   las conexiones, para evitar que los hilos se bloqueen durante largos períodos al
   intentar conectar con pares no receptivos. */
int init_peer_socket(char *ip)
{
	struct addrinfo hints, *peerinfo, *aux;
	int addrinfo_rv, sock = -1;

	/* Inicializa la estructura hints */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	/* Obtiene la lista de direcciones para el par dado */
	if ((addrinfo_rv = getaddrinfo(ip, TCP_PORT, &hints, &peerinfo)) != 0)
	{
		fprintf(stderr, "Error al recuperar la información de dirección del par!\n");
		fprintf(stderr, "Estado de Addrinfo: %s\n", gai_strerror(addrinfo_rv));
		return -1;
	}

	/* Itera sobre las direcciones, hasta encontrar una válida */
	for (aux = peerinfo; aux != NULL; aux = aux->ai_next)
	{
		if ((sock = socket(aux->ai_family, aux->ai_socktype, aux->ai_protocol)) == -1)
		{
			continue;
		}

		/* Establece el socket en modo no bloqueante, luego inicia el intento de conexión */
		fcntl(sock, F_SETFL, O_NONBLOCK);
		connect(sock, aux->ai_addr, aux->ai_addrlen);

		/* Crea un conjunto select con nuestro socket y configura un tiempo de espera de 500ms */
		fd_set fdset;
		struct timeval timeout;
		FD_ZERO(&fdset);
		FD_SET(sock, &fdset);
		timeout.tv_sec = 0;
		timeout.tv_usec = 500000;

		/* Ahora hacemos una encuesta a nuestro socket con select, con un tiempo de espera de 500ms */
		if (select(sock + 1, NULL, &fdset, NULL, &timeout) == 1)
		{
			int err;
			socklen_t len = sizeof(err);

			/* Obtiene el estado del socket */
			getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &len);

			/* ¡Éxito! Establece el socket en modo bloqueante nuevamente y rompe el bucle para retornar */
			if (err == 0)
			{
				int flags = fcntl(sock, F_GETFL);
				flags &= ~O_NONBLOCK;
				fcntl(sock, F_SETFL, flags);
				break;
			}

			/* Sin conexión después de 500ms, tiempo de espera! */
			close(sock);
		}

		/* Select falló, no estamos seguros de por qué sucede esto cuando ocurre... */
		else
		{
			close(sock);
		}
	}

	freeaddrinfo(peerinfo);

	/* Verifica si logramos conectarnos a alguna dirección */
	if (aux == NULL)
	{
		return -1;
	}

	return sock;
}

/* Inicializa un socket TCP que se enlaza a la dirección local y devuelve su
   ID de descriptor de archivo. Este socket se utilizará para aceptar conexiones entrantes
   de otros pares. Devuelve -1 si falla. */
int init_incoming_socket()
{
	int addrinfo_rv, sock = -1;
	int re = 1;
	struct addrinfo hints, *myinfo, *aux;

	/* Inicializa la estructura hints */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	/* Obtiene la lista de interfaces disponibles */
	if ((addrinfo_rv = getaddrinfo(NULL, TCP_PORT, &hints, &myinfo)) != 0)
	{
		fprintf(stderr, "Error al recuperar la lista de direcciones locales!\n");
		fprintf(stderr, "Estado de Addrinfo: %s\n", gai_strerror(addrinfo_rv));
		return -1;
	}

	/* Itera sobre las direcciones hasta encontrar una enlazable */
	for (aux = myinfo; aux != NULL; aux = aux->ai_next)
	{
		if ((sock = socket(aux->ai_family, aux->ai_socktype, aux->ai_protocol)) == -1)
		{
			fprintf(stderr, "Error al crear el socket para la dirección!\n");
			continue;
		}

		if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &re, sizeof(re)) == -1)
		{
			fprintf(stderr, "Error, no se pudo establecer el socket como reutilizable.\n");
			return -1;
		}

		if (bind(sock, aux->ai_addr, aux->ai_addrlen) == -1)
		{
			close(sock);
			fprintf(stderr, "Error, no se pudo enlazar el socket a la dirección.\n");
			continue;
		}

		break;
	}

	freeaddrinfo(myinfo);

	/* Verifica si logramos enlazarnos a alguna dirección */
	if (aux == NULL)
	{
		fprintf(stderr, "No se pudo encontrar una dirección válida para aceptar pares!\n");
		return -1;
	}

	return sock;
}

/* Procesa un mensaje de PeerList recibido en el socket dado, verificando si
   hay pares en la lista a los que no estemos conectados actualmente, y conectándose
   a cualquier posible nuevo par. */
void process_peerlist(int peersock, FILE *logfile)
{
	uint32_t size;
	uint8_t buf[4];

	fprintf(logfile, "\n----------Procesando lista de pares!----------\n");

	/* Analiza los bytes de tamaño para calcular el número de IPs en la lista */
	recv(peersock, buf, 4, MSG_WAITALL);
	size = ((buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3]);
	fprintf(logfile, "%u clientes:\n", size);

	/* Itera a través de las direcciones, verificando si estamos conectados a ellas */
	uint32_t i;
	for (i = 0; i < size; i++)
	{
		uint32_t uip = 0;
		recv(peersock, buf, 4, MSG_WAITALL);
		uip = ((buf[3] << 24) | (buf[2] << 16) | (buf[1] << 8) | buf[0]);
		fprintf(logfile, "%d.%d.%d.%d\n", buf[0], buf[1], buf[2], buf[3]);

		/* No intentamos conectarnos a nosotros mismos :) */
		if (uip == myaddr)
		{
			continue;
		}

		/* Nos aseguramos de que seamos los únicos accediendo a la lista para evitar duplicados */
		pthread_mutex_lock(&peerlist_mutex);

		/* Si el par no está conectado, obtenemos su IP y creamos el socket */
		if (!is_connected(peerlist, uip))
		{
			char ip[17];
			snprintf(ip, 17, "%d.%d.%d.%d", buf[0], buf[1], buf[2], buf[3]);
			fprintf(stdout, "Intentando conectar con el nuevo par %s... \n", ip);
			int newpeersock = init_peer_socket(ip);

			/* No se pudo conectar después de 500ms, seguimos */
			if (newpeersock == -1)
			{
				fprintf(stderr, "No se pudo conectar con el par %s!\n", ip);
				pthread_mutex_unlock(&peerlist_mutex);
				continue;
			}

			/* Si la conexión fue exitosa, lanzamos hilos para tratar con el par */
			pthread_t peerReq, peerRecv;
			pthread_create(&peerReq, NULL, peer_requester_thread, &newpeersock);
			pthread_create(&peerRecv, NULL, peer_receiver_thread, &newpeersock);
		}

		pthread_mutex_unlock(&peerlist_mutex);
	}
	fprintf(logfile, "----------Lista de pares procesada!----------\n\n");
}

/* Procesa una respuesta de archivo recibida en el socket dado. Primero, analizamos y
   almacenamos el contenido del archivo recibido de manera adecuada. Luego, verificamos si el
   nuevo archivo es más grande que el actualmente activo. Si es así, validamos este
   nuevo archivo. Si la verificación de validez es exitosa, reemplazamos el
   archivo actual por el nuevo y eliminamos el archivo antiguo. */
void process_archive(int peersock, FILE *logfile)
{
	fprintf(logfile, "\n----------Procesando respuesta de archivo!---------\n");

	/* Obtiene el número de chats en el archivo */
	uint8_t buf[4];
	uint32_t usize = 0;
	recv(peersock, buf, 4, MSG_WAITALL);
	usize = ((buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3]);

	fprintf(logfile, "Número de chats: %u\n", usize);

	/* Asigna una estructura de archivo para almacenar el archivo recibido */
	struct archive *new_archive = init_archive();
	uint8_t *ptr, *aux;

	/* Inicializa el tamaño del archivo y asigna memoria para su contenido.
	   Al principio, necesitamos asignar memoria para el archivo más grande posible. */
	new_archive->size = usize;
	ptr = (uint8_t *)malloc(5 + (usize * 289));
	aux = ptr;

	/* Calcula el tipo de mensaje de archivo y tamaño en la nueva cadena */
	*aux++ = 4;
	memcpy(aux, buf, 4);
	aux += 4;

	/* Inicializa un contador para la longitud total del mensaje (en bytes) */
	uint32_t len = 5;

	/* Ahora iteramos sobre cada mensaje en el archivo */
	unsigned int i;
	uint8_t codes[32], msg[256], msglen;
	for (i = 0; i < usize; i++)
	{
		/* Lee el mensaje del socket */
		memset(msg, 0, 256);
		recv(peersock, &msglen, 1, MSG_WAITALL);
		recv(peersock, msg, msglen, MSG_WAITALL);
		recv(peersock, codes, 32, MSG_WAITALL);

		/* Lo almacena en nuestra cadena */
		memcpy(aux, &msglen, 1);
		aux++;
		memcpy(aux, msg, msglen);
		aux += msglen;
		memcpy(aux, codes, 32);
		aux += 32;

		/* Actualiza la longitud total (33 = 32 bytes de md5+código y 1 byte para el tamaño del mensaje) */
		len += (msglen + 33);
	}

	/* Ahora reasigna la cadena final con solo la cantidad de memoria necesaria */
	new_archive->str = realloc(ptr, len);
	new_archive->len = len;

	fprintf(logfile, "Contenido del archivo recibido:\n");
	print_archive(new_archive, logfile);

	/* Si el nuevo archivo es válido y más grande que el activo, lo sustituimos
	   (la evaluación de corto circuito ahorra tiempo aquí si el nuevo archivo ya es más pequeño) */
	pthread_rwlock_rdlock(&archive_lock);
	if (new_archive->size > active_arch->size && is_valid(new_archive))
	{
		pthread_rwlock_unlock(&archive_lock);
		pthread_rwlock_wrlock(&archive_lock);
		free(active_arch->str);
		free(active_arch);
		active_arch = new_archive;
		fprintf(stdout, "---------- Archivo activo reemplazado! ----------\n");
	}

	/* De lo contrario, el archivo activo se mantiene, por lo que eliminamos el nuevo */
	else
	{
		free(new_archive->str);
		free(new_archive);
	}
	pthread_rwlock_unlock(&archive_lock);
	fprintf(logfile, "----------Respuesta de archivo procesada!----------\n\n");
}

/* Publica un archivo recién creado iterando sobre la lista de pares y enviando
   el archivo activo actual a cada par. Esta función parece extraña, porque
   todos los datos a los que accede están contenidos en ambas de nuestras estructuras de datos globales,
   la estructura de lista de pares y la estructura de archivo activo. */
void publish_archive()
{
	struct node *aux;

	fprintf(stdout, "\n----------Publicando nuevo archivo!----------\n");

	aux = peerlist->head->next;

	/* Itera sobre la lista de pares y envía el archivo a cada par */
	while (aux != NULL)
	{
		fprintf(stdout, "Enviando al par en el socket %u\n", aux->sock);
		send(aux->sock, active_arch->str, active_arch->len, 0);
		aux = aux->next;
	}

	fprintf(stdout, "----------Publicación completada!---------\n\n");
}

/* Implementa el trabajo realizado por los hilos lanzados para cada par, que periódicamente
   envían mensajes de solicitud de par ("0x1") al par conectado. Toma el socket
   asociado al par como entrada y simplemente entra en un bucle infinito, enviando
   mensajes de solicitud en un intervalo dado (5 segundos).
   Como un bono, dado que la especificación no menciona cuándo debemos enviar
   solicitudes de archivo, también las enviaremos periódicamente, en un intervalo más largo
   (cada 60 segundos). */
void *peer_requester_thread(void *sock)
{
	int peersock = *((int *)sock);
	uint8_t msg[2];

	/* Abre el archivo de registro para el socket del hilo */
	char filename[7];
	memset(filename, 0, 7);
	snprintf(filename, 7, "%d.log", peersock);
	FILE *logfile = fopen(filename, "a");

	/* Tenemos dos bytes de mensaje, uno para solicitudes de pares y el otro para el archivo */
	msg[0] = MSG_PEERREQ;
	msg[1] = MSG_ARCHREQ;

	/* Envía solicitudes de pares cada 5 segundos, sale si hay un tubo roto */
	int count = 0;
	while (1)
	{
		if (send(peersock, msg, 1, 0) == -1)
		{
			fprintf(logfile, "Error al enviar solicitud de par, ¿tubo roto?\n");
			fprintf(logfile, "Terminando hilo de solicitudes.\n");
			pthread_exit(NULL);
		}
		count++;

		/* Envía solicitudes de archivo cada 60 segundos (5*12 = 60) */
		if (count == 12)
		{
			if (send(peersock, msg + 1, 1, 0) == -1)
			{
				fprintf(logfile, "Error al enviar solicitud de archivo, ¿tubo roto?\n");
				fprintf(logfile, "Terminando hilo de solicitudes.\n");
				pthread_exit(NULL);
			}
			count = 0;
		}
		sleep(5);
	}
}

/* Implementa el trabajo realizado por los hilos lanzados para cada par que reciben y
   procesan datos enviados por el par conectado. Toma el socket asociado al
   par como entrada y usa recv() en el socket, esperando que lleguen mensajes, y
   procesa cada tipo de mensaje según corresponda.
   El socket está configurado con un tiempo de espera. Si una operación recv() se agota, asumimos
   que la conexión fue interrumpida, y cerramos el socket, desconectamos al par
   y lo eliminamos de la lista de pares conectados. */
void *peer_receiver_thread(void *sock)
{
	int peersock = *((int *)sock);

	/* Abre el archivo de registro para el socket del hilo */
	char filename[7];
	memset(filename, 0, 7);
	snprintf(filename, 7, "%d.log", peersock);
	FILE *logfile = fopen(filename, "a");

	/* Obtiene la información del nombre+ip del par */
	struct sockaddr_storage peeraddr;
	socklen_t peersize = sizeof(peeraddr);
	getpeername(peersock, (struct sockaddr *)&peeraddr, &peersize);
	struct sockaddr_in *peeraddr_in = (struct sockaddr_in *)&peeraddr;
	uint32_t upeerip = peeraddr_in->sin_addr.s_addr;
	char *cpeerip = inet_ntoa(peeraddr_in->sin_addr);

	/* Añade al par a la lista de pares conectados */
	pthread_mutex_lock(&peerlist_mutex);
	add_peer(peerlist, upeerip, peersock);
	fprintf(stdout, "Conectado exitosamente con el par %s\n", cpeerip);
	pthread_mutex_unlock(&peerlist_mutex);

	/* Configura el socket para que se agote en operaciones de recepción después de 60 segundos */
	struct timeval tout;
	tout.tv_sec = 60;
	tout.tv_usec = 0;
	setsockopt(peersock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tout, sizeof(tout));

	/* Bucle esperando mensajes */
	while (1)
	{
		/* Obtiene el primer byte para determinar el tipo de mensaje */
		uint8_t type;
		if (recv(peersock, &type, 1, MSG_WAITALL) <= 0)
		{
			/* La conexión se cerró o el socket se agotó */
			fprintf(stderr, "Tiempo de espera agotado esperando al par %s.\n", cpeerip);
			fprintf(stderr, "Probablemente el par se desconectó. Cerrando conexión...\n");
			close(peersock);
			pthread_mutex_lock(&peerlist_mutex);
			remove_peer(peerlist, upeerip);
			pthread_mutex_unlock(&peerlist_mutex);
			pthread_exit(NULL);
		}

		/* Procesa cada tipo de mensaje según corresponda */
		switch (type)
		{
		case MSG_PEERREQ:
		{
			fprintf(logfile, "Recibida solicitud de par, enviando lista!\n");
			send(peersock, peerlist->str, (5 + (4 * peerlist->size)), 0);
			break;
		}

		case MSG_PEERLIST:
		{
			process_peerlist(peersock, logfile);
			break;
		}

		case MSG_ARCHREQ:
		{
			fprintf(logfile, "Recibida solicitud de archivo!\n");
			if (!active_arch->size)
			{
				fprintf(logfile, "El archivo actual está vacío, ignorando la solicitud!\n");
				break;
			}
			fprintf(logfile, "Enviando archivo!\n");
			send(peersock, active_arch->str, active_arch->len, 0);
			break;
		}

		case MSG_ARCHRESP:
		{
			process_archive(peersock, logfile);
			break;
		}

		default:
		{
			fprintf(logfile, "Tipo de mensaje desconocido, ignorando... (byte = %d)\n", type);
			break;
		}
		}
	}
}

/* Esta función implementa todo el trabajo que debe realizar el hilo que
   trata con las conexiones entrantes de pares. Inicializa un socket pasivo, lo enlaza,
   luego escucha y espera conexiones entrantes, aceptándolas y
   lanzando hilos para intercambiar datos con cada par.
   El hilo que ejecuta esta función se llamará una vez al comienzo de
   la ejecución del programa y se ejecutará indefinidamente. */
void *incoming_peers_thread()
{
	int mysock, peersock;
	struct sockaddr_storage peeraddr;
	socklen_t peersize;

	/* Inicializa el socket de escucha */
	mysock = init_incoming_socket();

	/* Intenta escuchar en el socket creado */
	if (listen(mysock, 10) == -1)
	{
		fprintf(stderr, "No se pudo escuchar en el socket de pares entrantes!\n");
		pthread_exit(NULL);
	}

	fprintf(stdout, "[El hilo de pares entrantes está esperando conexiones]\n");

	/* Mientras (esperemos) para siempre, acepta conexiones entrantes de pares */
	char pigs_can_fly = 0;
	while (!pigs_can_fly)
	{
		peersize = sizeof(peeraddr);
		if ((peersock = accept(mysock, (struct sockaddr *)&peeraddr, &peersize)) == -1)
		{
			fprintf(stderr, "Error, no se pudo aceptar la conexión del par!\n");
			continue;
		}

		/* Lanza hilos de solicitud y recepción para el par entrante */
		fprintf(stdout, "Conexión de par entrante aceptada!\n");
		pthread_t peerReq, peerRecv;
		pthread_create(&peerReq, NULL, peer_requester_thread, &peersock);
		pthread_create(&peerRecv, NULL, peer_receiver_thread, &peersock);
	}

	pthread_exit(NULL);
}

/* Inicio de la ejecución del programa */
int main(int argc, char *argv[])
{
	/* Argumentos insuficientes, necesitamos un par inicial para conectarnos y la
	   dirección IP pública del dispositivo local */
	if (argc != 3)
	{
		fprintf(stderr, "Uso: ./blockchain <ip/hostname> <IP pública>\n");
		return 0;
	}

	/* Obtiene la representación int de la IP pública y la almacena, para evitar la autoconexión */
	struct in_addr testing;
	inet_aton(argv[2], &testing);
	myaddr = testing.s_addr;

	/* Inicializa nuestra estructura de lista de pares y su variable mutex */
	peerlist = init_list();
	pthread_mutex_init(&peerlist_mutex, NULL);

	/* Y el archivo activo, que inicialmente está vacío */
	active_arch = init_archive();
	pthread_rwlock_init(&archive_lock, NULL);

	/* Lo primero que hacemos es iniciar un hilo para aceptar conexiones entrantes */
	pthread_t incoming_thread;
	pthread_create(&incoming_thread, NULL, incoming_peers_thread, NULL);

	/* Ahora inicializa un socket para el primer par y lanza hilos para hablar con ellos */
	int sock = init_peer_socket(argv[1]);
	if (sock == -1)
	{
		fprintf(stderr, "No se pudo conectar con el par inicial!\n");
	}

	else
	{
		pthread_t reqthread, recvthread;
		pthread_create(&reqthread, NULL, peer_requester_thread, &sock);
		pthread_create(&recvthread, NULL, peer_receiver_thread, &sock);
	}

	/* Solicita al usuario mensajes para agregar al archivo */
	while (1)
	{
		uint8_t msg[256];

		memset(msg, 0, 256);
		fprintf(stdout, "Ingrese un mensaje de chat para enviar (máx. 255 caracteres):\n");
		fgets((char *)msg, 256, stdin);

		/* Vamos a escribir en el archivo, así que lo bloqueamos para escritura */
		pthread_rwlock_wrlock(&archive_lock);

		if (strcmp((char *)msg, "exit\n") == 0)
		{
			exit(0);
		}

		/* No se pudo agregar el mensaje, probablemente contenido ilegal */
		if (!add_message(active_arch, msg))
		{
			fprintf(stderr, "Mensaje inválido! Inténtalo de nuevo :)\n");
			pthread_rwlock_unlock(&archive_lock);
			continue;
		}

		/* Mensaje agregado al archivo, imprime el nuevo archivo, publícalo y desbloquéalo */
		fprintf(stdout, "Mensaje agregado al archivo con éxito!\n");
		fprintf(stdout, "Nuevo archivo activo:\n");
		print_archive(active_arch, stdout);

		publish_archive();
		pthread_rwlock_unlock(&archive_lock);
	}
}
