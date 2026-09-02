#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET mx_socket_t;
#define MX_INVALID_SOCKET INVALID_SOCKET
#define mx_close closesocket
#else
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
typedef int mx_socket_t;
#define MX_INVALID_SOCKET (-1)
#define mx_close close
#endif

static char respuesta[1024 * 1024];

static int enviar_todo(mx_socket_t socket, const char *datos, size_t longitud)
{
    while (longitud > 0) {
#ifdef _WIN32
        int enviado = send(socket, datos, (int)longitud, 0);
#else
        ssize_t enviado = send(socket, datos, longitud, 0);
#endif
        if (enviado <= 0) return 0;
        datos += enviado;
        longitud -= (size_t)enviado;
    }
    return 1;
}

static int analizar_url(const char *url, char *host, size_t host_size,
                        char *puerto, size_t puerto_size, char *ruta, size_t ruta_size)
{
    const char *inicio = NULL;
    if (strncmp(url, "http://", 7) == 0) inicio = url + 7;
    else if (strncmp(url, "https://", 8) == 0) return 0;
    else return 0;

    const char *separador = strpbrk(inicio, "/?#");
    size_t autoridad = separador ? (size_t)(separador - inicio) : strlen(inicio);
    const char *dos_puntos = memchr(inicio, ':', autoridad);
    size_t host_len = dos_puntos ? (size_t)(dos_puntos - inicio) : autoridad;
    if (host_len == 0 || host_len >= host_size) return 0;
    memcpy(host, inicio, host_len);
    host[host_len] = '\0';
    snprintf(puerto, puerto_size, "%s", dos_puntos ? dos_puntos + 1 : "80");
    snprintf(ruta, ruta_size, "%s", separador ? separador : "/");
    return 1;
}

long mx_http_request(const char *url, const char *metodo,
                     const char *cabeceras, const char *cuerpo)
{
    respuesta[0] = '\0';
    if (!url || !metodo) return 0;

#ifdef _WIN32
    WSADATA datos_socket;
    if (WSAStartup(MAKEWORD(2, 2), &datos_socket) != 0) return 0;
#endif
    char host[256], puerto[16], ruta[2048];
    if (!analizar_url(url, host, sizeof(host), puerto, sizeof(puerto),
                      ruta, sizeof(ruta))) {
#ifdef _WIN32
        WSACleanup();
#endif
        return 0;
    }

    struct addrinfo hints = {0};
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_UNSPEC;
    struct addrinfo *resultados = NULL;
    if (getaddrinfo(host, puerto, &hints, &resultados) != 0) {
#ifdef _WIN32
        WSACleanup();
#endif
        return 0;
    }

    mx_socket_t socket_fd = MX_INVALID_SOCKET;
    for (struct addrinfo *actual = resultados; actual; actual = actual->ai_next) {
        socket_fd = (mx_socket_t)socket(actual->ai_family, actual->ai_socktype, actual->ai_protocol);
        if (socket_fd == MX_INVALID_SOCKET) continue;
        if (connect(socket_fd, actual->ai_addr, (int)actual->ai_addrlen) == 0) break;
        mx_close(socket_fd);
        socket_fd = MX_INVALID_SOCKET;
    }
    freeaddrinfo(resultados);
    if (socket_fd == MX_INVALID_SOCKET) {
#ifdef _WIN32
        WSACleanup();
#endif
        return 0;
    }

    size_t cuerpo_len = cuerpo ? strlen(cuerpo) : 0;
    size_t headers_len = cabeceras ? strlen(cabeceras) : 0;
    size_t request_size = strlen(metodo) + strlen(ruta) + strlen(host) +
                          headers_len + cuerpo_len + 128;
    char *request = malloc(request_size);
    if (!request) {
        mx_close(socket_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return 0;
    }
    int written = snprintf(request, request_size, "%s %s HTTP/1.1\r\n"
                           "Host: %s\r\nConnection: close\r\n",
                           metodo, ruta, host);
    if (written < 0) written = 0;
    size_t used = (size_t)written;
    if (cabeceras && cabeceras[0]) {
        for (const char *line = cabeceras; *line;) {
            const char *end = strchr(line, '\n');
            size_t len = end ? (size_t)(end - line) : strlen(line);
            if (len && line[len - 1] == '\r') len--;
            if (used + len + 2 >= request_size) break;
            memcpy(request + used, line, len);
            used += len;
            memcpy(request + used, "\r\n", 2);
            used += 2;
            line = end ? end + 1 : line + len;
        }
    }
    if (cuerpo_len)
        used += (size_t)snprintf(request + used, request_size - used,
                                 "Content-Length: %zu\r\n", cuerpo_len);
    if (used + cuerpo_len + 3 >= request_size) {
        free(request);
        mx_close(socket_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return 0;
    }
    memcpy(request + used, "\r\n", 2);
    used += 2;
    if (cuerpo_len) { memcpy(request + used, cuerpo, cuerpo_len); used += cuerpo_len; }
    int ok = enviar_todo(socket_fd, request, used);
    free(request);
    if (ok) {
        size_t total = 0;
        while (total + 1 < sizeof(respuesta)) {
#ifdef _WIN32
            int recibido = recv(socket_fd, respuesta + total,
                                (int)(sizeof(respuesta) - total - 1), 0);
#else
            ssize_t recibido = recv(socket_fd, respuesta + total,
                                    sizeof(respuesta) - total - 1, 0);
#endif
            if (recibido <= 0) break;
            total += (size_t)recibido;
        }
        respuesta[total] = '\0';
    }
    mx_close(socket_fd);
#ifdef _WIN32
    WSACleanup();
#endif
    if (!ok) return 0;
    char *line_end = strstr(respuesta, "\r\n");
    if (!line_end) return 0;
    char *status = strchr(respuesta, ' ');
    return status ? strtol(status + 1, NULL, 10) : 0;
}

const char *mx_http_body(void)
{
    char *inicio = strstr(respuesta, "\r\n\r\n");
    return inicio ? inicio + 4 : "";
}

const char *mx_http_headers(void)
{
    static char cabeceras[1024 * 1024];
    const char *inicio = strstr(respuesta, "\r\n");
    const char *fin = inicio ? strstr(inicio + 2, "\r\n\r\n") : NULL;
    if (!inicio || !fin) {
        cabeceras[0] = '\0';
        return cabeceras;
    }
    size_t longitud = (size_t)(fin - (inicio + 2));
    if (longitud >= sizeof(cabeceras)) longitud = sizeof(cabeceras) - 1;
    memcpy(cabeceras, inicio + 2, longitud);
    cabeceras[longitud] = '\0';
    return cabeceras;
}
