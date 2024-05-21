# Obtener el sistema operativo, porque MacOS no enlaza/incluye OpenSSL por defecto
# Asumimos que está instalado en la carpeta predeterminada /usr/local/opt/openssl/,
# Si no es así, se puede cambiar
UNAME := $(shell uname -s)
ifeq ($(UNAME), Darwin)
	SSLINCLUDE = -I/usr/local/opt/openssl/include
	SSLLIB = -L/usr/local/opt/openssl/lib
endif

# Compilar con algunas advertencias adicionales
CFLAGS = -c -Wall -Wextra

# Esto debería funcionar para la mayoría de las distribuciones de Linux
LIBFLAGS = -lpthread -lcrypto

# Reglas de objetivos reales
all: blockchain

blockchain: main.o peerlist.o archive.o
	gcc $(SSLLIB) main.o peerlist.o archive.o -o blockchain $(LIBFLAGS)

main.o: main.c
	gcc $(SSLINCLUDE) $(CFLAGS) main.c

peerlist.o: peerlist.c
	gcc $(CFLAGS) peerlist.c

archive.o: archive.c
	gcc $(SSLINCLUDE) $(CFLAGS) archive.c

clean:
	rm *.o blockchain*
