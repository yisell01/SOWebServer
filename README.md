# SOWebServer
Para compilar el servidor puede usar el archivo makefile o escribir en la consola gcc server.c -lrt -lcurl -o server
Para ejecutarlo necesita dos parámetros: el puerto y la ruta que desea abrir. Ejemplo:
./server 8000 /home/user/Documents

# Funcionalidades 

Permite listar directorios y archivos en una computadora. Entra a todas las carpetas y descarga los archivos. Muestra el nombre, la fecha de modificación, tamaño y permisos. Permite ordenar por ellos. Permite múltiples peticiones de varios clientes simultáneamente y permite varias peticiones desde un mismo cliente.
