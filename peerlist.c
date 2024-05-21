#include "peerlist.h"

/* Este archivo implementa una estructura de datos de lista y sus funciones asociadas.
   La implementación específica de la lista contenida aquí está destinada a almacenar la lista de
   pares conectados, soportando la adición/eliminación de direcciones IP en la lista, así
   como mantener una representación en cadena pre-computada de la misma, para que la construcción
   de paquetes de red que contienen la lista sea razonablemente rápida.

   NOTA: Una tabla hash o incluso un conjunto ordenado (usando un árbol) definitivamente
   habrían sido mejores opciones para almacenar la lista de pares, pero sus implementaciones
   habrían sido más laboriosas, y dado que es poco probable que el tamaño de la lista crezca
   significativamente, no vale la pena el esfuerzo. */

/* Recomputa la representación en cadena de la lista, para actualizar los pares conectados después
   de la eliminación o adición de un par */
void list_to_str(struct peer_list *list)
{
	uint8_t *buf;
	uint32_t size;

	/* Obtiene el tamaño de la lista */
	size = list->size;

	/* Libera la cadena antigua y asigna memoria para la nueva */
	free(list->str);
	buf = (uint8_t *)malloc((5 + (size * 4)) * sizeof(uint8_t));

	/* El primer byte es el tipo de mensaje (2), los otros cuatro bytes son el número de pares */
	buf[0] = 2;
	buf[1] = (size >> 24) & 0xFF;
	buf[2] = (size >> 16) & 0xFF;
	buf[3] = (size >> 8) & 0xFF;
	buf[4] = size & 0xFF;

	/* Ahora, para cada nodo en la lista, convierte la IP de entero a arreglo de bytes,
	   en orden de bytes de red */
	struct node *aux = list->head->next;
	uint32_t i;
	for (i = 5; i < (5 + (size * 4)); i += 4)
	{
		buf[i + 3] = (aux->ip >> 24) & 0xFF;
		buf[i + 2] = (aux->ip >> 16) & 0xFF;
		buf[i + 1] = (aux->ip >> 8) & 0xFF;
		buf[i] = aux->ip & 0xFF;

		aux = aux->next;
	}

	list->str = buf;
}

/* Agrega una IP dada a la lista de pares conectados y actualiza el tamaño de la lista
   y su representación en cadena en consecuencia */
void add_peer(struct peer_list *list, uint32_t ip, uint32_t sock)
{
	struct node *aux;

	aux = list->last;

	aux->next = (struct node *)malloc(sizeof(struct node));
	aux->next->ip = ip;
	aux->next->sock = sock;
	aux->next->next = NULL;
	list->last = aux->next;

	list->size += 1;

	list_to_str(list);
}

/* Elimina una IP dada de la lista de pares conectados y actualiza el tamaño de la lista
   y su representación en cadena en consecuencia */
void remove_peer(struct peer_list *list, uint32_t ip)
{
	struct node *prev;

	prev = list->head;

	/* Intenta encontrar la IP en la lista, si la encontramos, rompe el bucle con prev = nodo anterior */
	while (prev != list->last)
	{
		if (prev->next->ip == ip)
		{
			break;
		}
		prev = prev->next;
	}

	/* Si la IP no está en la lista, retorna */
	if (prev->next == NULL)
	{
		return;
	}

	/* Obtiene el puntero al nodo que se va a eliminar y actualiza el puntero de prev */
	struct node *to_remove = prev->next;

	prev->next = to_remove->next;

	/* Si estamos eliminando el último nodo, actualiza el puntero */
	if (to_remove == list->last)
	{
		list->last = prev;
	}

	/* Libera la memoria y actualiza el tamaño de la lista */
	free(to_remove);
	list->size -= 1;

	/* Actualiza la representación en cadena de la lista */
	list_to_str(list);
}

/* Devuelve 1 si la IP dada está actualmente en la lista de pares conectados, 0
   en caso contrario. Obviamente se usa para verificar si ya estamos conectados a una IP */
int is_connected(struct peer_list *list, uint32_t ip)
{
	struct node *aux;

	aux = list->head;

	/* Busca la IP en la lista, devuelve 1 si la encuentra */
	while (aux != NULL)
	{
		if (aux->ip == ip)
		{
			return 1;
		}
		aux = aux->next;
	}

	/* Si llegamos aquí, no está en la lista */
	return 0;
}

/* Imprime una lista de pares conectados. Solo para fines de depuración */
void print_list(struct peer_list *list)
{
	struct node *aux;

	fprintf(stderr, "Lista de pares [tamaño %u]:\n", list->size);

	/* Lista vacía, retorna */
	if (list->head == list->last)
	{
		return;
	}

	/* La cabeza se deja vacía intencionalmente, comenzamos a imprimir desde el nodo 2 */
	aux = list->head->next;

	/* Imprime todos los nodos, luego el último */
	while (aux != list->last)
	{
		/* Como esto es solo para depuración, no nos molestamos en convertir a cadena */
		fprintf(stderr, "%u[%u] -> ", aux->ip, aux->sock);
		aux = aux->next;
	}
	fprintf(stderr, "%u[%u]\n", aux->ip, aux->sock);
}

/* Inicializa una estructura de lista de pares. Inicialmente, la lista tiene tamaño 0,
   y los nodos último y cabeza son los mismos (sin datos). Su representación en cadena es
   también un puntero NULL */
struct peer_list *init_list()
{
	struct peer_list *newlist;

	newlist = (struct peer_list *)malloc(sizeof(struct peer_list));

	newlist->size = 0;
	newlist->head = (struct node *)malloc(sizeof(struct node));
	newlist->head->next = NULL;
	newlist->head->ip = 0;
	newlist->last = newlist->head;

	newlist->str = NULL;

	return newlist;
}
