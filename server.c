#include <sys/socket.h>       
#include <sys/types.h>        
#include <arpa/inet.h>        
#include <unistd.h>           
#include <signal.h>           
#include <stdlib.h>           
#include <stdio.h>            
#include <string.h>          
#include <errno.h>            
#include <fcntl.h>            
#include <sys/mman.h>         
#include <sys/types.h>        
#include <sys/stat.h>         
#include <dirent.h> 
#include <time.h>
#include <curl/curl.h>


// Socket para escuchar.
int listen_sckt;                   

// Aquí se guardan el código y el path que se le debe mandar al cliente.
typedef struct {
	int code;
	char *path;
} httpRequest;

// Aquí se guardan las variables que estarán en memoria compartida. 
typedef struct {
	pthread_mutex_t mutexlock;
	int totalbytes;
} sharedVariables;

// Headers que se le mandan al cliente.
char *header200 = "HTTP/1.0 200 OK\nServer: ServYi\nContent-Type: text/html\n\n";
char *header200D = "HTTP/1.0 200 OK\nServer: ServYi\nContent-Type: application/octet-stream\n";
char *header404 = "HTTP/1.0 404 Not Found\nServer: ServYi\nContent-Type: text/html\n\n";

// Enviar html que muestra los archivos y directorios que hay en el path.
void show_dir_content(int fd, char *temp, char *path);

// Coge el path que viene en Get.
char *getPath(char* msg){
    
    char *file;
    if( (file = malloc(sizeof(char) * strlen(msg))) == NULL)
    {
        fprintf(stderr, "Error allocating memory to file in getPath()\n");
        exit(EXIT_FAILURE);
    }
    
    sscanf(msg, "GET %s HTTP/1.1", file);
    return file;
}


// Mejora la url, poniéndole por ejemplo los espacios en blanco que tenía originalmente y que el navegador le quita.
char *changeUrlToPath(char *url) {
    
    CURL *curl = curl_easy_init();
    char *path;
    int pathSize;

    path = curl_easy_unescape(curl, url, (int) strlen(url), &pathSize);
    curl_easy_cleanup(curl);

    return path;
}

// Dada una petición HTTP se devuelven el código y el path que debe recibir el cliente.
httpRequest request(char *msg, char *initPath){
   
    httpRequest ret;
    
    char* url;
    if( (url = malloc(sizeof(char) * strlen(msg))) == NULL)
    {
        fprintf(stderr, "Error allocating memory to path in request()\n");
        exit(EXIT_FAILURE);
    }
    
    url = getPath(msg);

    char *path= changeUrlToPath(url);

    // Para revisar si el directorio pedido existe.
    DIR *d; 
    DIR *d1; 
	struct dirent *dir; 
	d = opendir(path); 

    if(d1 = opendir(initPath) == NULL){
        fprintf(stderr, "Error opening the directory\n");
        exit(EXIT_FAILURE);
    }
    
    int init = strcmp(path, "/");
    // Si es la primera conexión se devuelve el directorio montado en el servidor.
    if(init == 0)
    {
        ret.code = 200;
        ret.path = initPath;
    }
    
    // Si el cliente abre una carpeta se devuelve la información contenida ahí.
    else if( d != NULL )
    {
        ret.code = 200;
        ret.path = path;
    }

    // Si el cliente hace clic en un archivo se manda a descargar.
    else if( d == NULL )
    {
        ret.code = 201;
        ret.path = path;
    }
    
    else
    {
        ret.code = 404;
        char *nf = "\0";
        strcpy(nf, initPath);
        strcat(nf, "/");
        strcat(nf, "404.html");
        ret.path = nf;
    }
    
	closedir(d); 
    return ret;
}

// Mandar mensaje al file descriptor del socket.
int sendMessage(int fd, char *msg) {
    return write(fd, msg, strlen(msg));
}

// Manda a escribir en el file descriptor del socket la información para el cliente.
int sendHTML(int fd, char *path, char *initPath) {
  
    int totalsize;
    struct stat st;
    stat(path, &st);
    totalsize = st.st_size;
    size_t size = 1;
    
    char *temp;
    if(  (temp = malloc(sizeof(char) * size)) == NULL )
    {
        fprintf(stderr, "Error allocating memory to temp in sendHTML()\n");
        exit(EXIT_FAILURE);
    }

    DIR *d;
    struct dirent *dir;
    d = opendir(path); 

    // Si el path es de una carpeta se imprime el html que muestra los archivos y directorios que tiene.
    if (d != NULL) 
	{
        closedir(d);
        show_dir_content(fd, &temp, path);
    }
    // Si el path es de un archivo se descarga.
    else {
        int fi_des = open(path, O_RDONLY);
        off_t offset = 0;
        while (offset < st.st_size) {
            size_t chunk_size = 1024 * 1024;
            if (offset + chunk_size > st.st_size) {
                chunk_size = st.st_size - offset;
            } 
            ssize_t sent_bytes = sendfile(fd, fi_des, &offset, chunk_size);
        }

    close(fd);

    }
    
    sendMessage(fd, "\n");
    free(temp);
    return totalsize;
}

// Enviar html que muestra los archivos y directorios que hay en el path.
void show_dir_content(int fd, char *temp, char *path) {
     
    DIR *d;
    struct dirent *dir;
    struct stat st; 
    d = opendir(path); 

    strcpy(temp, "<html><head>Directorio ");
    sendMessage(fd, temp);
    strcpy(temp, path);
    sendMessage(fd, temp);
    strcpy(temp, "</head><body><table cellspacing=\"15\"><tr><th style=\"cursor:pointer;\" onclick=\"sortT(0,'str')\">Name</th><th style=\"cursor:pointer;\" onclick=\"sortT(1,'int')\">Size</th><th style=\"cursor:pointer;\" onclick=\"sortT(2,'str')\">Date</th><th style=\"cursor:pointer;\" onclick=\"sortT(3,'str')\">Permisos</th></tr>");
    sendMessage(fd, temp);
	
	while ((dir = readdir(d)) != NULL) 
	{ 
	    if (dir->d_type == DT_REG) { 
            char aux[strlen(path) + strlen(dir->d_name) + 1];
            strcpy(aux, path);
            strcat(aux, "/");
            strcat(aux, dir->d_name);
            stat(aux, &st);
            struct tm *tm;
            tm = localtime(&st.st_mtime);

            strcpy(temp, "<tr><td><a href=\"");
            sendMessage(fd, temp);
            strcpy(temp, path);
            sendMessage(fd, temp);
            strcpy(temp, "/");
            sendMessage(fd, temp);
            strcpy(temp, dir->d_name);
            sendMessage(fd, temp);
            strcpy(temp, "\" download=\"");
            sendMessage(fd, temp);
            strcpy(temp, dir->d_name);
            sendMessage(fd, temp);
            strcpy(temp, "\">");
            sendMessage(fd, temp);
            strcpy(temp, dir->d_name);
            sendMessage(fd, temp);
            strcpy(temp, "</a></td><td>");
            sendMessage(fd, temp);
            sprintf(temp, "%ld", st.st_size);
            sendMessage(fd, temp);
            strcpy(temp, "</td><td>");
            sendMessage(fd, temp);
            sprintf(temp, "%d-%02d-%02d %02d:%02d:%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
            sendMessage(fd, temp);
            strcpy(temp, "</td><td>");
            sendMessage(fd, temp);
            strcpy(temp, (S_ISDIR(st.st_mode)) ? "d" : "-");
            sendMessage(fd, temp);
            strcpy(temp, (st.st_mode & S_IRUSR) ? "r" : "-");
            sendMessage(fd, temp);
            strcpy(temp, (st.st_mode & S_IWUSR) ? "w" : "-");
            sendMessage(fd, temp);
            strcpy(temp, (st.st_mode & S_IXUSR) ? "x" : "-");
            sendMessage(fd, temp);
            strcpy(temp, (st.st_mode & S_IRGRP) ? "r" : "-");
            sendMessage(fd, temp);
            strcpy(temp, (st.st_mode & S_IWGRP) ? "w" : "-");
            sendMessage(fd, temp);
            strcpy(temp, (st.st_mode & S_IXGRP) ? "x" : "-");
            sendMessage(fd, temp);
            strcpy(temp, (st.st_mode & S_IROTH) ? "r" : "-");
            sendMessage(fd, temp);
            strcpy(temp, (st.st_mode & S_IWOTH) ? "w" : "-");
            sendMessage(fd, temp);
            strcpy(temp, (st.st_mode & S_IXOTH) ? "x" : "-");
            sendMessage(fd, temp);
            strcpy(temp, "</td></tr>");
            sendMessage(fd, temp);
         
		}
			
		if (dir->d_type == DT_DIR && strcmp(dir->d_name,".")!=0 && strcmp(dir->d_name,"..")!=0) { 
            char aux[strlen(path) + strlen(dir->d_name) + 1];
            strcpy(aux, path);
            strcat(aux, "/");
            strcat(aux, dir->d_name);
            stat(aux, &st);
            struct tm *tm;
            tm = localtime(&st.st_mtime);
            
            strcpy(temp, "<tr><td><a href=\"");
            sendMessage(fd, temp);
            strcpy(temp, path);
            sendMessage(fd, temp);
            strcpy(temp, "/");
            sendMessage(fd, temp);
            strcpy(temp, dir->d_name);
            sendMessage(fd, temp);
            strcpy(temp, "\">");
            sendMessage(fd, temp);
            strcpy(temp, dir->d_name);
            sendMessage(fd, temp);
            strcpy(temp, "</a></td><td>0</td><td>");
            sendMessage(fd, temp);
            sprintf(temp, "%d-%02d-%02d %02d:%02d:%02d", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
            sendMessage(fd, temp);
            strcpy(temp, "</td><td>");
            sendMessage(fd, temp);
            strcpy(temp, (S_ISDIR(st.st_mode)) ? "d" : "-");
            sendMessage(fd, temp);
            strcpy(temp, (st.st_mode & S_IRUSR) ? "r" : "-");
            sendMessage(fd, temp);
            strcpy(temp, (st.st_mode & S_IWUSR) ? "w" : "-");
            sendMessage(fd, temp);
            strcpy(temp, (st.st_mode & S_IXUSR) ? "x" : "-");
            sendMessage(fd, temp);
            strcpy(temp, (st.st_mode & S_IRGRP) ? "r" : "-");
            sendMessage(fd, temp);
            strcpy(temp, (st.st_mode & S_IWGRP) ? "w" : "-");
            sendMessage(fd, temp);
            strcpy(temp, (st.st_mode & S_IXGRP) ? "x" : "-");
            sendMessage(fd, temp);
            strcpy(temp, (st.st_mode & S_IROTH) ? "r" : "-");
            sendMessage(fd, temp);
            strcpy(temp, (st.st_mode & S_IWOTH) ? "w" : "-");
            sendMessage(fd, temp);
            strcpy(temp, (st.st_mode & S_IXOTH) ? "x" : "-");
            sendMessage(fd, temp);
            strcpy(temp, "</td></tr>");
            sendMessage(fd, temp);
		}

	} 
    // Permite ordernar por nombre, tamaño, fecha y permisos.
    strcpy(temp, "</table><script> function sortT(n, type) { var table, rows, switching, i, x, y, shouldSwitch, dir, switchcount = 0; table = document.getElementsByTagName(\"table\")[0]; switching = true; dir = \"asc\" ;" 
    "while (switching) { switching = false; rows = table.rows; for (i = 1; i < (rows.length - 1); i++) { shouldSwitch = false; x = rows[i].getElementsByTagName(\"td\" )[n]; y = rows[i + 1].getElementsByTagName(\"td\" )[n]; "
    "if (dir == \"asc\" ) { if ((type == 'str' && x.innerHTML.toLowerCase() > y.innerHTML.toLowerCase()) || (type == 'int' && parseInt(x.innerHTML) > parseInt(y.innerHTML))) { shouldSwitch = true; break; } } "
    "else if (dir == \"desc\" ) { if ((type == 'str' && x.innerHTML.toLowerCase() < y.innerHTML.toLowerCase()) || (type == 'int' && parseInt(x.innerHTML) < parseInt(y.innerHTML))) { shouldSwitch = true; break; } } } "
    "if (shouldSwitch) { rows[i].parentNode.insertBefore(rows[i + 1], rows[i]); switching = true; switchcount++; } else { if (switchcount == 0 && dir == \"asc\" ) { dir = \"desc\" ; switching = true; } } } } </script></body></html>");
    sendMessage(fd, temp);
    
    closedir(d); 
}

// Mandar el header al cliente.
int sendHeader(int fd, int code,  int totalsize)
{
    switch (code)
    {
        // Abrir un directorio.
        case 200:
        sendMessage(fd, header200);
        return strlen(header200);
        break;

        // Descargar un archivo.
        case 201:
        sendMessage(fd, header200D);
        char *temp;
        int tam=0;
        temp = malloc(sizeof(char) * 20);
        tam = strlen(header200D);
        strcpy(temp, "Content-Length: ");
        tam= tam+strlen(temp);
        sendMessage(fd, temp);
        sprintf(temp, "%d", totalsize);
        sendMessage(fd, temp);
        tam= tam+strlen(temp);
        strcpy(temp, "\n\n");
        tam= tam+strlen(temp);
        sendMessage(fd, temp);
        return tam;
        break;
        
        // No se encontró la petición del cliente.
        case 404:
        sendMessage(fd, header404);
        return strlen(header404);
        break;
    }
}

// Para limpiar el socket por el que se estaba escuchando cuando se pone el comando ctrl-c.
void cleanup(int sig) {
    
    if (close(listen_sckt) < 0) {
        fprintf(stderr, "Error calling close()\n");
        exit(EXIT_FAILURE);
    }
    
    // Cerrar la memoria compartida que se usó.
    shm_unlink("/sharedmem");
    
    exit(EXIT_SUCCESS);
}

// Contador de la cantidad de bytes mandados por todos los procesos.
int recordTotalBytes(int bytes_sent, sharedVariables *mempointer)
{
    // Lock el mutex.
    pthread_mutex_lock(&(*mempointer).mutexlock);
    // Incrementamos los bytes enviados.
    (*mempointer).totalbytes += bytes_sent;
    // Unlock el mutex.
    pthread_mutex_unlock(&(*mempointer).mutexlock);
    return (*mempointer).totalbytes;
}

// Obtener el mensaje del socket hasta que se reciba una línea en blanco.
char *getMessage(int fd) {
  
    FILE *sstream;
    
    // Para abrir el socket y guardar la información en un file stream.
    if( (sstream = fdopen(fd, "r")) == NULL)
    {
        fprintf(stderr, "Error opening file descriptor in getMessage()\n");
        exit(EXIT_FAILURE);
    }
    
    size_t size = 1;
    
    char *block;
    
    if( (block = malloc(sizeof(char) * size)) == NULL )
    {
        fprintf(stderr, "Error allocating memory to block in getMessage\n");
        exit(EXIT_FAILURE);
    }
    *block = '\0';
    
    char *tmp;
    if( (tmp = malloc(sizeof(char) * size)) == NULL )
    {
        fprintf(stderr, "Error allocating memory to tmp in getMessage\n");
        exit(EXIT_FAILURE);
    }
    *tmp = '\0';
    
    int end;
    int oldsize = 1;
    
    while( (end = getline( &tmp, &size, sstream)) > 0)
    {
        if( strcmp(tmp, "\r\n") == 0)
        {
            break;
        }
        
        block = realloc(block, size+oldsize);
        oldsize += size;
        strcat(block, tmp);
    }
    if (block == NULL){

    }
    
    free(tmp);
    
    return block;
}

int main(int argc, char *argv[]) {
    
    // Socket para la conexión.
    int conn_s;    
    // Número de puerto.              
    short int port = atoi(argv[1]);  
    // Estructura de dirección del socket.
    struct sockaddr_in servaddr; 

    if (argc != 3) {
        fprintf(stderr, "USAGE: %s <port number> <website directory>\n", argv[0]);
        exit(-1);
    }
    char *initPath = argv[2]; 

    printf("Listening in port: %i\n", port);
    printf("Serving Directory: %s\n", initPath);
    
    // Señal para ctrl-c.
    (void) signal(SIGINT, cleanup);
    
    // Se crea el socket para escuchar.
    if ((listen_sckt = socket(AF_INET, SOCK_STREAM, 0)) < 0 ) {
        fprintf(stderr, "Error creating listening socket.\n");
        exit(EXIT_FAILURE);
    }
    
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(port);
    
    if (bind(listen_sckt, (struct sockaddr *) &servaddr, sizeof(servaddr)) < 0 ) {
        fprintf(stderr, "Error calling bind()\n");
        exit(EXIT_FAILURE);
    }
    
    if( (listen(listen_sckt, 1024)) == -1)
    {
        fprintf(stderr, "Error listening\n");
        exit(EXIT_FAILURE);
    } 
    
    shm_unlink("/sharedmem");
    
    int sharedmem;
    
    // Abrir la memoria compartida.
    if( (sharedmem = shm_open("/sharedmem", O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR)) == -1)
    {
        fprintf(stderr, "Error opening sharedmem in main() errno is: %s ", strerror(errno));
        exit(EXIT_FAILURE);
    }
    
    ftruncate(sharedmem, sizeof(sharedVariables) );
    
    sharedVariables *mempointer;
    
    mempointer = mmap(NULL, sizeof(sharedVariables), PROT_READ | PROT_WRITE, MAP_SHARED, sharedmem, 0); 
    
    if( mempointer == MAP_FAILED )
    {
        fprintf(stderr, "Error setting shared memory for sharedVariables in recordTotalBytes(): %d \n ", errno);
        exit(EXIT_FAILURE);
    }

    pthread_mutex_init(&(*mempointer).mutexlock, NULL);
    (*mempointer).totalbytes = 0;

    
    int addr_size = sizeof(servaddr);
    int headersize;
    int pagesize;
    int totaldata;

    int children = 0;
    pid_t pid;
    
    // Loop infinito para responder peticiones.
    while(1)
    {
    
        if( children <= 1024) 
        {
            pid = fork();
            children++;
        }
        
        if( pid == -1)
        {
            fprintf(stderr,"Error %d in fork\n" , errno);
            exit (1);
        }
        
        if ( pid == 0)
        {	
            while(1)
            {
                conn_s = accept(listen_sckt, (struct sockaddr *)&servaddr, &addr_size);
                    
                if(conn_s == -1)
                {
                    fprintf(stderr,"Error accepting connection \n");
                    exit (1);
                }
                
                char *msg = getMessage(conn_s);
                
                httpRequest details = request(msg, initPath);
                
                free(msg);
                
                int totalsize;
                struct stat st;
                stat(details.path, &st);
                totalsize = st.st_size;

                headersize = sendHeader(conn_s, details.code, totalsize);
                
                pagesize = sendHTML(conn_s, details.path, initPath);
                
                totaldata = recordTotalBytes(headersize+pagesize, mempointer);
                
                close(conn_s);
            }
        }
    }
    
    return EXIT_SUCCESS;
}
