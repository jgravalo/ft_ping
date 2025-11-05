#define _GNU_SOURCE  // Habilita funciones GNU (como strdup, etc.)
#include <stdio.h>   // Para printf, perror
#include <stdlib.h>  // Para exit, malloc, free
#include <string.h>  // Para memset, strcmp
#include <unistd.h>  // Para close, getpid, sleep
#include <netdb.h>   // Para getaddrinfo
#include <sys/socket.h> // Para socket, sendto, recvfrom
#include <arpa/inet.h>  // Para inet_ntop
#include <netinet/ip_icmp.h> // Para struct icmphdr (cabecera ICMP)
#include <netinet/ip.h>      // Para struct iphdr (cabecera IP)
#include <signal.h>          // Para signal() y SIGINT
#include <sys/time.h>        // Para gettimeofday()
#include <math.h>            // Para sqrt(), fmin(), fmax()
#include <errno.h>           // Para errno

#define PACKET_SIZE 64       // Tamaño del paquete ICMP a enviar (56 + 8 de cabecera ICMP)

// Variables globales
volatile int running = 1;    // Control de ejecución (se vuelve 0 al presionar Ctrl+C)
int verbose = 0;             // Si está activo el modo verbose con -v

// Variables de estadísticas
int transmitted = 0;         // Total de paquetes enviados
int received = 0;            // Total de respuestas recibidas
double rtt_min = 1e9;        // RTT mínimo inicializado a un número muy alto
double rtt_max = 0;          // RTT máximo inicializado a 0
double rtt_sum = 0;          // Suma total de RTT para calcular la media
double rtt_sum2 = 0;         // Suma de RTT al cuadrado para mdev
struct timeval start_time, end_time; // Tiempos de inicio y fin del programa

// Manejador de señal SIGINT (Ctrl+C)
void sigint_handler(/* int signo */) {
    running = 0; // Detiene el bucle principal
    gettimeofday(&end_time, NULL); // Guarda el tiempo cuando se interrumpe
}

// Calcula diferencia de tiempo en milisegundos
double time_diff_ms(struct timeval *start, struct timeval *end) {
    return (double)(end->tv_sec - start->tv_sec) * 1000.0 + // Diferencia en segundos
           (double)(end->tv_usec - start->tv_usec) / 1000.0; // Diferencia en microsegundos
}

// Muestra mensaje de ayuda
void print_help(const char *progname) {
    printf("Usage: %s [-v] [-?] <host>\n", progname); // Cómo usar el programa
    printf("Options:\n");
    printf("  -v    Verbose output (show ICMP errors)\n"); // Explicación de -v
    printf("  -?    Display this help message\n");         // Explicación de -?
}

// Muestra información del paquete recibido
void print_info_package(char *buf, char *ipstr, int bytes, struct timeval *send_time, struct timeval *recv_time) {
    struct iphdr *ip = (struct iphdr *)buf; // Cabecera IP
    struct icmphdr *recv_icmp = (struct icmphdr *)(buf + (ip->ihl * 4)); // Cabecera ICMP
    double rtt = time_diff_ms(send_time, recv_time); // RTT calculado
    // Verificar que sea Echo Reply del mismo proceso
    if (recv_icmp->type == ICMP_ECHOREPLY &&
        recv_icmp->un.echo.id == getpid()) {
        // Mostrar respuesta
        printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%.2f ms\n",
            bytes - (ip->ihl * 4), ipstr, recv_icmp->un.echo.sequence, ip->ttl, rtt);

        received++;              // Incrementa contador de recibidos
        rtt_min = fmin(rtt_min, rtt); // Actualiza mínimo
        rtt_max = fmax(rtt_max, rtt); // Actualiza máximo
        rtt_sum += rtt;              // Acumula para media
        rtt_sum2 += rtt * rtt;       // Acumula para desviación
    } else if (verbose) {
        // Si se recibe otro tipo de ICMP y -v está activo
        printf("ICMP type=%d code=%d received from %s\n",
            recv_icmp->type, recv_icmp->code, ipstr);
    }
}

// Muestra estadísticas finales
void print_final_stats(char *host) {
    // Mostrar estadísticas cuando se interrumpe con Ctrl+C
    double elapsed = time_diff_ms(&start_time, &end_time); // Tiempo total de ejecución
    printf("\n--- %s ping statistics ---\n", host);
    printf("%d packets transmitted, %d received, %.0f%% packet loss, time %.0fms\n",
           transmitted, received,
           transmitted > 0 ? 100.0 * (transmitted - received) / transmitted : 0.0,
           elapsed);
    // Mostrar estadísticas de RTT si hubo respuestas
    if (received > 0) {
        double avg = rtt_sum / received; // Promedio
        double mdev = sqrt((rtt_sum2 / received) - (avg * avg)); // Desviación estándar
        printf("rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n", rtt_min, avg, rtt_max, mdev);
    }
}

// Calcula el checksum para ICMP
unsigned short checksum(void *b, int len) {
    unsigned short *buf = b; // Interpreta los datos como enteros de 16 bits
    unsigned int sum = 0;    // Suma acumulativa

    // Suma de palabras de 16 bits
    for (; len > 1; len -= 2)
        sum += *buf++;

    // Si queda un byte suelto
    if (len == 1)
        sum += *(unsigned char *)buf;

    // Reducción del carry (suma las partes altas con las bajas)
    sum = (sum >> 16) + (sum & 0xFFFF);
    sum += (sum >> 16);

    return ~sum; // Devuelve el complemento a uno (operación estándar de checksum)
}

// Procesa argumentos y obtiene el host
char *get_host(int argc, char *argv[]) {
    if (argc < 2) { // Verifica que se haya pasado al menos un argumento
        print_help(argv[0]);
        exit(1);
    }

    char *host = NULL; // Almacena el nombre del host a hacer ping

    // Procesamiento de argumentos
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-v") == 0)
            verbose = 1;         // Activa modo verbose
        else if (strcmp(argv[i], "-?") == 0) {               // Muestra ayuda
            print_help(argv[0]);
            exit(0);
        } else
            host = argv[i]; // Guarda el host si no es una opción
    }

    if (!host) { // Si no se especificó ningún host
        fprintf(stderr, "Error: no host specified\n");
        exit(1);
    }
    return host;
}

// Resuelve el host y obtiene la IP en formato texto
char *get_ipstr(const char *host, struct addrinfo **res) {
    // Preparar para resolver el nombre del host
    struct addrinfo hints = {0};
    hints.ai_family = AF_INET; // Solo IPv4
    // Resolver host → IP
    if (getaddrinfo(host, NULL, &hints, res) != 0) {
        perror("getaddrinfo");
        exit(1);
    }
    // Obtener la IP en formato texto
    struct sockaddr_in *addr = (struct sockaddr_in *)(*res)->ai_addr;
    char ipstr[INET_ADDRSTRLEN]; // Cadena para guardar la IP
    inet_ntop(AF_INET, &(addr->sin_addr), ipstr, sizeof(ipstr)); // Convierte IP a string
    return strdup(ipstr);
}

// char *do_packet(/*int sockfd, */int *seq) {
char *do_packet(int *seq) {
    // Preparar paquete ICMP
    char packet[PACKET_SIZE] = {0}; // Inicializa el paquete a 0
    struct icmphdr *icmp = (struct icmphdr *)packet; // Cabecera ICMP

    // Configurar cabecera ICMP
    icmp->type = ICMP_ECHO;              // Tipo: Echo Request
    icmp->code = 0;                      // Código: 0
    icmp->un.echo.id = getpid();         // ID: PID del proceso
    icmp->un.echo.sequence = (*seq)++;      // Número de secuencia
    icmp->checksum = checksum(icmp, PACKET_SIZE); // Calcular checksum

    return strdup(packet); // Devuelve una copia del paquete
}

int wait_for_response(int sockfd, int seq) {
    fd_set read_fds;
    struct timeval timeout;
    FD_ZERO(&read_fds);
    FD_SET(sockfd, &read_fds);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;

    // Espera hasta 1 segundo por respuesta
    int ret = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
    if (ret < 0) {
        if (errno == EINTR)
            return 1; // Interrupción por Ctrl+C
        perror("select");
        return -1;
    } else if (ret == 0) {
        if (verbose)
            printf("Request timeout for icmp_seq %d\n", seq - 1);
        return 1; // Timeout: no respuesta
    }
    return 0;
}

int main(int argc, char *argv[]) {
    char *host = get_host(argc, argv); // Almacena el nombre del host a hacer ping
    struct addrinfo *res;
    char *ipstr = get_ipstr(host, &res); // Resuelve el host y obtiene la IP en formato texto
    // Mostrar cabecera inicial del ping
    printf("PING %s (%s) 56(84) bytes of data.\n", host, ipstr);
    // Crear socket raw ICMP
    int sockfd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }
    signal(SIGINT, sigint_handler); // Captura Ctrl+C
    gettimeofday(&start_time, NULL); // Guarda tiempo inicial
    int seq = 0; // Número de secuencia de los paquetes
    while (running) {
        // Preparar paquete ICMP
        char packet[PACKET_SIZE] = {0}; // Inicializa el paquete a 0
        struct icmphdr *icmp = (struct icmphdr *)packet; // Cabecera ICMP

        // Configurar cabecera ICMP
        icmp->type = ICMP_ECHO;              // Tipo: Echo Request
        icmp->code = 0;                      // Código: 0
        icmp->un.echo.id = getpid();         // ID: PID del proceso
        icmp->un.echo.sequence = seq++;      // Número de secuencia
        icmp->checksum = checksum(icmp, PACKET_SIZE); // Calcular checksum

        struct timeval send_time, recv_time;
        gettimeofday(&send_time, NULL); // Tiempo antes de enviar

        // Enviar paquete
        if (sendto(sockfd, packet, PACKET_SIZE, 0, res->ai_addr, res->ai_addrlen) <= 0) {
            perror("sendto");
            continue;
        }
        transmitted++; // Incrementa contador de enviados

        // Esperar respuesta
        char buf[1024]; // Buffer para recibir respuesta
        struct sockaddr_in r_addr; // Dirección origen de la respuesta
        socklen_t len = sizeof(r_addr);
        
        fd_set read_fds;
        struct timeval timeout;
        FD_ZERO(&read_fds);
        FD_SET(sockfd, &read_fds);
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        // Espera hasta 1 segundo por respuesta
        int ret = select(sockfd + 1, &read_fds, NULL, NULL, &timeout);
        if (ret < 0) {
            if (errno == EINTR) continue; // Interrupción por Ctrl+C
            perror("select");
            break;
        } else if (ret == 0) {
            if (verbose)
                printf("Request timeout for icmp_seq %d\n", seq - 1);
            continue; // Timeout: no respuesta
        }
        // int server = wait_for_response(sockfd, seq);
        // if (server < 0) {
        //     if (server == -1) continue; // Timeout o interrupción
        //     else break; // Error grave
        // }

        // Recibir paquete
        int bytes = recvfrom(sockfd, buf, sizeof(buf), 0, (struct sockaddr *)&r_addr, &len);
        if (bytes < 0) {
            perror("recvfrom");
            continue;
        }
        gettimeofday(&recv_time, NULL); // Tiempo después de recibir
        print_info_package(buf, ipstr, bytes, &send_time, &recv_time); // Procesar y mostrar info del paquete
        sleep(1); // Espera 1 segundo antes de enviar otro paquete
    }
    print_final_stats(host); // Mostrar estadísticas finales
    // Liberar recursos
    freeaddrinfo(res); // Libera memoria de resolución DNS
    close(sockfd);     // Cierra el socket
    return 0;          // Fin del programa
}
