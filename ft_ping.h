#ifndef FT_PING_H
#define FT_PING_H

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

// Prototipos de funciones
unsigned short checksum(void *b, int len);
double time_diff_ms(struct timeval *start, struct timeval *end);
void sigint_handler(/* int signo */);
char *get_host(int argc, char *argv[]);
char *get_ipstr(const char *host, struct addrinfo **res);
int wait_for_response(int sockfd, int seq);
void print_help(const char *progname);
void print_info_package(char *buf, char *ipstr, int bytes, struct timeval *send_time, struct timeval *recv_time);
void print_final_stats(char *host);

#endif