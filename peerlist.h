#include <stdio.h>  // para imprimir información de depuración
#include <stdlib.h> // mallocs, frees y demás
#include <stdint.h> // tipos de tamaño portátil (uint8_t, uint32_t, etc.)

/* Estructura que representa un nodo en una lista de pares conectados. Almacenamos las IPs como
   enteros sin signo de 4 bytes para una comparación más rápida. Esto es seguro porque todas las IPs
   están garantizadas como IPv4. También almacenamos el socket asociado con ese par,
   para que podamos transmitir mensajes iterando a través de la lista */
struct node
{
  uint32_t ip;
  uint32_t sock;
  struct node *next;
};

/* Estructura que representa toda una lista de pares, con punteros a los primeros y
   últimos nodos, tamaño (en número de nodos) y representación en cadena, para una construcción
   de mensajes más rápida */
struct peer_list
{
  struct node *head, *last;
  uint32_t size;
  uint8_t *str;
};

/* Recomputa la representación en cadena de la lista, para actualizar los pares conectados después
   de la eliminación o adición de un par */
void list_to_str(struct peer_list *list);

/* Agrega una IP dada a la lista de pares conectados y actualiza el tamaño de la lista
   y su representación en cadena en consecuencia */
void add_peer(struct peer_list *list, uint32_t ip, uint32_t sock);

/* Elimina una IP dada de la lista de pares conectados y actualiza el tamaño de la lista
   y su representación en cadena en consecuencia */
void remove_peer(struct peer_list *list, uint32_t ip);

/* Devuelve 1 si la IP dada está actualmente en la lista de pares conectados, 0
   en caso contrario. Obviamente se usa para verificar si ya estamos conectados a una IP */
int is_connected(struct peer_list *list, uint32_t ip);

/* Imprime una lista de pares conectados. Solo para fines de depuración */
void print_list(struct peer_list *list);

/* Inicializa una estructura de lista de pares. Inicialmente, la lista tiene tamaño 0,
   y los nodos último y cabeza son los mismos (sin datos). Su representación en cadena es
   también un puntero NULL */
struct peer_list *init_list();
