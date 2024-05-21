#include <stdint.h>      //tipos portátiles (uint8_t, uint32_t, etc...)
#include <stdlib.h>      //funciones para gestión de memoria como malloc, calloc, free y similares
#include <stdio.h>       //impresión, principalmente para depuración e informes de errores
#include <string.h>      //funciones de manipulación de memoria como memset, memcpy y otras
#include <openssl/md5.h> //hashing MD5

/* Estructura que almacena un archivo de chat. Descripción breve de sus campos:
   size   -> número de mensajes de chat en el archivo
   str    -> representación en cadena de todo el archivo, en el formato que se envía a otros
             (en bytes de red y demás)
   len    -> longitud de la representación en cadena del archivo, en bytes
   offset -> almacena un desplazamiento desde el puntero base hasta donde se encuentra el mensaje 19
             desde el final del archivo, para que podamos acceder fácilmente a la secuencia que
             necesitamos hashear para agregar nuevos mensajes.
             Este desplazamiento se define por primera vez al validar un archivo por primera vez,
             y se actualiza si se agregan mensajes. */
struct archive
{
  uint8_t *str;
  uint32_t offset;
  uint32_t size;
  uint32_t len;
};

/* Analiza el mensaje, verificando si todos los caracteres son válidos (imprimibles).
   Para mensajes válidos, devuelve el número de caracteres del mensaje.
   Devuelve 0 para cadenas no válidas (vacías o que contienen caracteres ilegales). */
int parse_message(uint8_t *msg);

/* Intenta insertar el mensaje 'msg' en el archivo de chat dado. Para ello,
   verificamos si el mensaje es válido y luego extraemos un código de 16 bytes que genera
   un hash MD5 válido para la cadena. Posteriormente, formateamos la cadena con el mensaje
   completo y los metadatos de manera adecuada e incluimos esto en la estructura del archivo,
   actualizándolo en consecuencia.
   Devuelve 1 si el mensaje se agregó correctamente, 0 en caso contrario.

   Cabe destacar que no validamos el archivo antes de intentar agregar el mensaje,
   simplemente asumimos que ya es válido, ya que todos los archivos se validan
   al ser recibidos inicialmente. */
int add_message(struct archive *arch, uint8_t *msg);

/* Dado un archivo de entrada, validamos los hashes MD5 de todos sus mensajes y
   determinamos si el archivo completo es válido o no. Devolvemos 1 si el archivo es válido,
   y 0 en caso contrario. */
int is_valid(struct archive *arch);

/* Imprime un archivo en el flujo dado, para depuración o actualización del archivo */
void print_archive(struct archive *arch, FILE *stream);

/* Inicializa una nueva estructura de archivo y la devuelve. Los archivos nuevos tienen tamaño 0,
   de modo que cualquier archivo nuevo y válido puede sobrescribirlos. Su representación en cadena es
   inicialmente de 5 caracteres de longitud, conteniendo solo el tipo de mensaje y los 4 bytes
   que indican la cantidad de mensajes (que obviamente es 0).
   El desplazamiento es inicialmente 5, ya que no hay mensajes en el archivo (obviamente), y
   ignoramos los bytes de tipo y tamaño. */
struct archive *init_archive();
