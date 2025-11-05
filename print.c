#include "ft_ping.h"

// Muestra mensaje de ayuda
void print_help(const char *progname) {
    printf("Usage: %s [-v] [-?] <host>\n", progname); // Cómo usar el programa
    printf("Options:\n");
    printf("  -v    Verbose output (show ICMP errors)\n"); // Explicación de -v
    printf("  -?    Display this help message\n");         // Explicación de -?
}


void print_info_package(char *buf, char *ipstr, int bytes, struct timeval *send_time, struct timeval *recv_time) {
    struct iphdr *ip = (struct iphdr *)buf; // Cabecera IP
    // printf("buf: %s\n", buf); // BORRAR !!!!!!
    struct icmphdr *recv_icmp = (struct icmphdr *)(buf + (ip->ihl * 4)); // Cabecera ICMP
    // printf("SEGFAULT\n");

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