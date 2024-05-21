#include "archive.h"

/*
   En este archivo, implementamos todas las estructuras de datos y operaciones relacionadas con los archivos de chat.
   Esto incluye la estructura que almacena un archivo, así como las operaciones para modificarlo.
   También se abarcan todas las operaciones relacionadas con archivos entrantes, como la validación de hash y otros procesos.

   Analizamos los mensajes verificando si todos los caracteres son válidos (imprimibles).
   Para los mensajes válidos, devolvemos el número de caracteres del mensaje.
   En el caso de cadenas no válidas (vacías o que contengan caracteres ilegales), devolvemos 0.
*/

int parse_message(uint8_t *msg)
{
  int count = 0;

  /* Itera sobre los caracteres de una cadena, validando y contando cada uno */
  while (*msg)
  {
    /* La nueva línea indica el fin del mensaje, no se incluye en el conteo ni en el contenido */
    if (*msg == 10)
    {
      break;
    }

    /* Verifica caracteres ilegales */
    if (*msg < 32 || *msg > 126)
    {
      return 0;
    }

    count++;
    msg++;
  }

  return count;
}

/*
   Intentamos insertar el mensaje 'msg' en el archivo de chat proporcionado. Para ello,
   verificamos si el mensaje es válido y luego extraemos un código de 16 bytes que genera
   un hash MD5 válido para la cadena. Posteriormente, formateamos la cadena con el mensaje
   completo y los metadatos de manera adecuada e incluimos esto en la estructura del archivo,
   actualizándolo en consecuencia.
   Devolvemos 1 si el mensaje se agregó correctamente, 0 en caso contrario.

   Cabe destacar que no validamos el archivo antes de intentar agregar el mensaje,
   simplemente asumimos que ya es válido, ya que todos los archivos se validan
   al ser recibidos inicialmente.
*/

int add_message(struct archive *arch, uint8_t *msg)
{
  uint16_t len;
  uint8_t *code, *md5;

  /* Analiza el mensaje y obtiene la longitud, devuelve 0 si el mensaje no es válido */
  len = parse_message(msg);
  if (len == 0)
  {
    return 0;
  }

  /* Imprime el mensaje al usuario */
  int i;
  fprintf(stdout, "\nLongitud del mensaje = %d\nContenido: ", len);
  for (i = 0; i < len; i++)
  {
    fprintf(stdout, "%c", msg[i]);
  }
  fprintf(stdout, "\n");

  /* Reasigna la cadena de archivo para que se ajuste al nuevo mensaje y luego concatenarlo */
  arch->str = realloc(arch->str, arch->len + len + 33);
  *(arch->str + arch->len) = len;
  memcpy(arch->str + arch->len + 1, msg, len);

  /* Obtiene punteros al comienzo del código y secciones de hash MD5 */
  code = arch->str + arch->len + len + 1;
  md5 = code + 16;

  /* Puntero de 128 bits para la comparación del hash, puntero de 16 bits para verificar los primeros 2 bytes */
  unsigned __int128 *mineptr = (unsigned __int128 *)code;
  uint16_t *check = (uint16_t *)md5;

  /* Extrae un código que genera un hash MD5 válido */
  *mineptr = (unsigned __int128)0;
  while (1)
  {
    MD5(arch->str + arch->offset, (arch->len - arch->offset + len + 17), md5);
    /* Si los primeros 2 bytes son 0, hemos encontrado el hash válido */
    if (*check == 0)
    {
      break;
    }
    *mineptr += 1;
  }

  /* Imprime el código extraído y el hash del mensaje */
  fprintf(stdout, "código: ");
  for (i = 0; i < 16; i++)
  {
    fprintf(stdout, "%02x", *(code + i));
  }
  fprintf(stdout, "\nmd5: ");
  for (i = 0; i < 16; i++)
  {
    fprintf(stdout, "%02x", *(md5 + i));
  }
  fprintf(stdout, "\n\n");

  /* Actualiza el tamaño y la longitud del archivo, ajusta el offset si es necesario */
  arch->size += 1;
  arch->len += len + 33;
  if (arch->size >= 20)
  {
    arch->offset += *(arch->str + arch->offset) + 33;
  }

  /* Actualiza la representación de bytes del tamaño del archivo */
  uint8_t *aux = arch->str + 1;
  uint32_t old_size = ((aux[0] << 24) | (aux[1] << 16) | (aux[2] << 8) | aux[3]);
  old_size++;
  aux[0] = (old_size >> 24) & 0xFF;
  aux[1] = (old_size >> 16) & 0xFF;
  aux[2] = (old_size >> 8) & 0xFF;
  aux[3] = old_size & 0xFF;

  return 1;
}

/*
   Dado un archivo de entrada, validamos los hashes MD5 de todos sus mensajes y
   determinamos si el archivo completo es válido o no. Devolvemos 1 si el archivo es válido,
   y 0 en caso contrario.
*/

int is_valid(struct archive *arch)
{
  uint8_t *begin, *end, md5[16];
  unsigned __int128 *calc_hash, *orig_hash;

  /* Omite bytes de tipo/tamaño de mensaje */
  begin = arch->str + 5;
  end = arch->str + 5;

  /* Nuestro hash calculado siempre está en la misma dirección de memoria */
  calc_hash = (unsigned __int128 *)md5;

  /* Ahora repetimos el proceso para cada mensaje en el archivo */
  uint32_t i, md5len = 0;
  for (i = 1; i <= arch->size; i++)
  {
    /* Primero calcula la longitud del mensaje actual */
    uint8_t len = *end;

    /* Itera hasta el final del mensaje y realiza un seguimiento de cuántos bytes se harán hash */
    end += len + 17;
    md5len += len + 17;

    /* Verifica los primeros 2 bytes del hash, usamos un puntero de 2 bytes para simplificar */
    uint16_t *f2bytes = (uint16_t *)end;
    if (*f2bytes != 0)
    {
      fprintf(stderr, "Bytes no nulos en el hash MD5. ¡Archivo inválido!\n");
      return 0;
    }

    /* Actualiza el desplazamiento a partir del mensaje 20 */
    if (i > 19)
    {
      arch->offset += ((*begin) + 33);
    }

    /* Si la secuencia tiene más de 20 mensajes, elimina el primer mensaje de la cadena de entrada del MD5
       y vuelve a calcular su longitud */
    if (i > 20)
    {
      md5len -= ((*begin) + 33);
      begin += ((*begin) + 33);
    }

    /* Calcula el hash para la secuencia de bytes y compáralo con el hash original */
    MD5(begin, md5len, md5);

    orig_hash = (unsigned __int128 *)end;

    if (*calc_hash != *orig_hash)
    {
      fprintf(stderr, "¡Desajuste de hash! Archivo inválido.\n");
      return 0;
    }

    /* Actualiza el puntero final después del hash MD5 y actualiza la longitud de la cadena de entrada del MD5 */
    end += 16;
    md5len += 16;
  }
  return 1;
}

/* Imprime un archivo en el flujo dado, para depuración o actualización del archivo */
void print_archive(struct archive *arch, FILE *stream)
{
  uint8_t *ptr;
  uint32_t size;

  ptr = arch->str;
  size = arch->size;

  fprintf(stream, "\n---------- INICIO DEL ARCHIVO ----------\n");
  /* Bytes de tipo y tamaño de mensaje */
  fprintf(stream, "tamaño: %u, longitud: %u\n", arch->size, arch->len);

  ptr += 5;

  /* Itera sobre los mensajes */
  uint32_t i, j;
  for (i = 0; i < size; i++)
  {
    uint8_t len;
    len = *ptr++;

    fprintf(stream, "msg[%d]: ", len);

    /* Contenido del mensaje */
    for (j = 0; j < len; j++, ptr++)
    {
      fprintf(stream, "%c", *ptr);
    }

    /* Código de hash de 16 bytes */
    fprintf(stream, "\ncódigo: ");
    for (j = 0; j < 16; j++, ptr++)
    {
      fprintf(stream, "%02x", *ptr);
    }

    /* Hash MD5 de 16 bytes */
    fprintf(stream, "\nmd5: ");
    for (j = 0; j < 16; j++, ptr++)
    {
      fprintf(stream, "%02x", *ptr);
    }
    fprintf(stream, "\n");
  }

  fprintf(stream, "---------- FIN DEL ARCHIVO ----------\n");
}

/* Inicializa una nueva estructura de archivo y la devuelve. Los archivos nuevos tienen tamaño 0,
   de modo que cualquier archivo nuevo y válido puede sobrescribirlos. Su representación en cadena es
   inicialmente de 5 caracteres de longitud, conteniendo solo el tipo de mensaje y los 4 bytes
   que indican la cantidad de mensajes (que obviamente es 0).
   El desplazamiento es inicialmente 5, ya que no hay mensajes en el archivo (obviamente), y
   ignoramos los bytes de tipo y tamaño.
*/
struct archive *init_archive()
{
  struct archive *newarchive;

  newarchive = (struct archive *)malloc(sizeof(struct archive));

  uint8_t *str = (uint8_t *)malloc(5);
  str[0] = 4;
  str[1] = str[2] = str[3] = str[4] = 0;
  newarchive->str = str;
  newarchive->offset = 5;

  newarchive->len = 5;
  newarchive->size = 0;

  return newarchive;
}
