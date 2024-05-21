# Proyecto Servicios

Una implementación de un nodo de protocolo distribuido de cadena de bloques P2P. Este proyecto mantiene un archivo de mensajes distribuido, con "transacciones" (adiciones de mensajes) que se validan mediante hashes MD5, los cuales deben ser minados para cada mensaje. Es similar a un mini-bitcoin, pero destinado a la comunicación (En este caso simular un chat para mantener la comunicación de entes gubernamentales en caso de desastres naturales, que tengan la capacidad de comunicación por medio de un chat)en lugar de la transferencia de moneda ficticia. Utiliza exclusivamente IPv4 debido a la complejidad añadida de IPv6. 

Se ha incluido un nivel detallado de comentarios debido a que es un proyecto, lo cual facilita su comprensión y revisión.

# Integrantes

- Andres Esteban Manjarres (2190826)
- Christian David Posada (2205107)
- Jaime Humberto Ñañez (2202097)
- Juan Manuel Ospina (2205539)
- Juan David Bohorquez (2201732)

# Antes de compilar

La ejecución y testing de este miniproyecto fue realizado en un entorno virtualizado, en este caso, en Vagrant.

Por aquí puedes descargar el Vagrantfile: https://www.mediafire.com/file/wad89ku46yr7is5/Vagrantfile/file

# Compilación

Para compilar el proyecto, simplemente ejecuta `make`. Las reglas de destino del Makefile deberían funcionar en la mayoría de las distribuciones de Linux, así como en MacOS, siempre que OpenSSL esté instalado.

**NOTA:** Debe ser compilado con `gcc` para arquitecturas de 64 bits, ya que se utilizan algunas características específicas de la implementación x64, como los tipos primitivos de 128 bits.

# Ejecución

Para ejecutar el programa desde la línea de comandos, utiliza la siguiente sintaxis:

./blockchain <IP del par inicial> <IP local>

Donde la IP del par inicial es la dirección IPv4 de un par al que deseas conectarte activamente al inicio de la ejecución. Ingresa una IP inválida para no conectarte a ningún par y simplemente escuchar conexiones de manera pasiva.

La IP local debe ser la dirección IPv4 de la interfaz en la que el programa escuchará conexiones, para evitar intentos de autoconexión. Esto podría haberse implementado de manera más elegante utilizando un protocolo STUN, pero eso habría añadido una complejidad significativa al proyecto, por lo que se utiliza esta solución alternativa.

# Funcionalidades

Cuando el programa está en ejecución, el terminal solicitará al usuario que ingrese mensajes para ser añadidos al archivo activo actual. Si algún mensaje ingresado es válido, se insertará en el archivo y el nuevo archivo se publicará a todos los pares conectados.

Para cada par conectado, la implementación crea un archivo de registro en la carpeta de ejecución, con el formato `x.log`, donde `x` es el ID del descriptor de archivo asociado con el par. Esto evita que los flujos de salida estándar (stderr/stdout) se inunden con información de los diferentes pares. Para observar el comportamiento de la comunicación con cualquier par, simplemente consulta el archivo de registro correspondiente.

Si se escribe `exit` en el terminal principal, el programa se cerrará, garantizando que los búferes de salida se vacíen adecuadamente, lo que no ocurre al interrumpir con el comando habitual `CTRL+C`.

# Si ocurren errores

Recomendamos utilizar los siguientes comandos para solventar cualquier problema:

- sudo apt-get update 
- sudo apt install make
- sudo apt install gcc
- sudo apt install libssl-dev




