#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <limits.h>

#ifndef MX_OPENSSL_ENABLED
#define MX_OPENSSL_ENABLED 0
#endif

#if MX_OPENSSL_ENABLED
#include <openssl/err.h>
#include <openssl/ssl.h>
#endif

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET mx_socket_t;
typedef int mx_socklen_t;
#define MX_INVALID_SOCKET INVALID_SOCKET
#define mx_close closesocket
#else
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
typedef int mx_socket_t;
typedef socklen_t mx_socklen_t;
#define MX_INVALID_SOCKET (-1)
#define mx_close close
#endif

static char respuesta[1024 * 1024];

#define MX_MAX_OPEN_FILES 128
#define MX_FILE_BUFFER_SIZE 256
static FILE *archivos_mx[MX_MAX_OPEN_FILES];
static char buffer_archivo_mx[MX_FILE_BUFFER_SIZE];
static int debugger_step_mode = 1;
static long debugger_breakpoints[256];
static size_t debugger_breakpoint_count;
static char mensaje_excepcion_mx[1024];

#define MX_MAX_SOCKETS 128
#define MX_SOCKET_BUFFER_SIZE 256
static mx_socket_t sockets_mx[MX_MAX_SOCKETS];
static char buffers_socket_mx[MX_MAX_SOCKETS][MX_SOCKET_BUFFER_SIZE];
static char remitentes_socket_mx[MX_MAX_SOCKETS][NI_MAXHOST + NI_MAXSERV + 4];
#if MX_OPENSSL_ENABLED
static SSL *tls_sockets_mx[MX_MAX_SOCKETS];
static SSL_CTX *tls_servidores_mx[MX_MAX_SOCKETS];
#endif
#ifdef _WIN32
static int winsock_inicializado;
#endif
static int tabla_sockets_inicializada;

void mx_set_exception(const char *message)
{
    snprintf(mensaje_excepcion_mx, sizeof(mensaje_excepcion_mx), "%s",
             message ? message : "Excepcion sin mensaje");
}

const char *mx_last_exception(void)
{
    return mensaje_excepcion_mx;
}

const char *mx_exception_message(const char *message)
{
    return message ? message : "";
}

void mx_uncaught_exception(const char *message)
{
    mx_set_exception(message);
    fprintf(stderr, "Excepcion no capturada: %s\n", mensaje_excepcion_mx);
    exit(EXIT_FAILURE);
}

void mx_debugger_step(long line)
{
    int stop = debugger_step_mode;
    for (size_t i = 0; i < debugger_breakpoint_count; ++i)
        if (debugger_breakpoints[i] == line)
            stop = 1;
    if (!stop)
        return;

    char command[128];
    for (;;)
    {
        fprintf(stderr, "[debugger] linea %ld (s=paso, c=continuar, b LINEA, q=salir)> ", line);
        fflush(stderr);
        if (!fgets(command, sizeof(command), stdin))
        {
            debugger_step_mode = 0;
            return;
        }
        command[strcspn(command, "\r\n")] = '\0';
        if (strcmp(command, "s") == 0 || command[0] == '\0')
        {
            debugger_step_mode = 1;
            return;
        }
        if (strcmp(command, "c") == 0)
        {
            debugger_step_mode = 0;
            return;
        }
        if (strcmp(command, "q") == 0)
            exit(130);
        if (strncmp(command, "b ", 2) == 0)
        {
            char *end = NULL;
            long breakpoint = strtol(command + 2, &end, 10);
            if (end == command + 2 || *end != '\0' || breakpoint <= 0)
                fprintf(stderr, "[debugger] linea de breakpoint no valida\n");
            else if (debugger_breakpoint_count >= sizeof(debugger_breakpoints) / sizeof(debugger_breakpoints[0]))
                fprintf(stderr, "[debugger] limite de breakpoints alcanzado\n");
            else
            {
                debugger_breakpoints[debugger_breakpoint_count++] = breakpoint;
                fprintf(stderr, "[debugger] breakpoint agregado en linea %ld\n", breakpoint);
                debugger_step_mode = 0;
                return;
            }
            continue;
        }
        fprintf(stderr, "[debugger] comandos: s, c, b LINEA, q\n");
    }
}

static FILE *mx_get_file(long handle)
{
    if (handle <= 0 || (unsigned long)handle > MX_MAX_OPEN_FILES)
        return NULL;
    return archivos_mx[(unsigned long)handle - 1];
}

static void mx_file_error(const char *message)
{
    mx_set_exception(message);
    fprintf(stderr, "Error de archivo: %s\n", message);
}

static int mx_network_init(void)
{
#ifdef _WIN32
    if (!winsock_inicializado)
    {
        WSADATA datos;
        if (WSAStartup(MAKEWORD(2, 2), &datos) != 0)
        {
            mx_set_exception("No se pudo inicializar Winsock.");
            return 0;
        }
        winsock_inicializado = 1;
    }
#endif
    return 1;
}

static void mx_socket_error(const char *operation, int error_code)
{
    char message[256];
    snprintf(message, sizeof(message), "Socket: %s fallo (error %d).",
             operation, error_code);
    mx_set_exception(message);
}

static int mx_socket_slot(long handle)
{
    if (!tabla_sockets_inicializada)
    {
        for (size_t i = 0; i < MX_MAX_SOCKETS; ++i)
        {
            sockets_mx[i] = MX_INVALID_SOCKET;
#if MX_OPENSSL_ENABLED
            tls_sockets_mx[i] = NULL;
            tls_servidores_mx[i] = NULL;
#endif
        }
        tabla_sockets_inicializada = 1;
    }
    if (handle <= 0 || (unsigned long)handle > MX_MAX_SOCKETS ||
        sockets_mx[(unsigned long)handle - 1] == MX_INVALID_SOCKET)
        return -1;
    return (int)handle - 1;
}

static long mx_socket_store(mx_socket_t socket_fd)
{
    if (!tabla_sockets_inicializada)
    {
        for (size_t i = 0; i < MX_MAX_SOCKETS; ++i)
        {
            sockets_mx[i] = MX_INVALID_SOCKET;
#if MX_OPENSSL_ENABLED
            tls_sockets_mx[i] = NULL;
            tls_servidores_mx[i] = NULL;
#endif
        }
        tabla_sockets_inicializada = 1;
    }
    for (size_t i = 0; i < MX_MAX_SOCKETS; ++i)
    {
        if (sockets_mx[i] == MX_INVALID_SOCKET)
        {
            sockets_mx[i] = socket_fd;
            buffers_socket_mx[i][0] = '\0';
            remitentes_socket_mx[i][0] = '\0';
            return (long)(i + 1);
        }
    }
    mx_close(socket_fd);
    mx_set_exception("Socket: se alcanzo el limite de conexiones abiertas.");
    return -1;
}

static void mx_socket_prevent_sigpipe(mx_socket_t socket_fd)
{
#if defined(SO_NOSIGPIPE)
    int enabled = 1;
    (void)setsockopt(socket_fd, SOL_SOCKET, SO_NOSIGPIPE,
                     (const char *)&enabled, sizeof(enabled));
#else
    (void)socket_fd;
#endif
}

static int mx_socket_set_remote(int slot, const struct sockaddr *address,
                                mx_socklen_t address_length)
{
    char host[NI_MAXHOST], service[NI_MAXSERV];
    int status = getnameinfo(address, address_length, host, sizeof(host),
                             service, sizeof(service), NI_NUMERICHOST | NI_NUMERICSERV);
    if (status != 0)
        return 0;
    if (strchr(host, ':'))
        snprintf(remitentes_socket_mx[slot], sizeof(remitentes_socket_mx[slot]),
                 "[%s]:%s", host, service);
    else
        snprintf(remitentes_socket_mx[slot], sizeof(remitentes_socket_mx[slot]),
                 "%s:%s", host, service);
    return 1;
}

#if MX_OPENSSL_ENABLED
static int mx_tls_report_error(const char *operation, SSL *ssl)
{
    unsigned long code = ERR_get_error();
    char detail[192] = "no hay detalle TLS disponible";
    if (code)
        ERR_error_string_n(code, detail, sizeof(detail));
#ifdef _WIN32
    int socket_error = WSAGetLastError();
#else
    int socket_error = errno;
#endif
    char message[512];
    snprintf(message, sizeof(message), "TLS: %s fallo: %s (socket %d).",
             operation, detail, socket_error);
    mx_set_exception(message);
    (void)ssl;
    return 0;
}

static SSL_CTX *mx_tls_client_context(void)
{
    SSL_CTX *context = SSL_CTX_new(TLS_client_method());
    if (!context)
    {
        mx_tls_report_error("creacion del contexto cliente", NULL);
        return NULL;
    }
    if (SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION) != 1)
    {
        mx_tls_report_error("configuracion de TLS minimo", NULL);
        SSL_CTX_free(context);
        return NULL;
    }
    SSL_CTX_set_verify(context, SSL_VERIFY_PEER, NULL);
    if (SSL_CTX_set_default_verify_paths(context) != 1)
    {
        mx_tls_report_error("carga de autoridades certificadoras", NULL);
        SSL_CTX_free(context);
        return NULL;
    }
    return context;
}
#endif

long mx_socket_tls_client(long handle, const char *hostname)
{
#if MX_OPENSSL_ENABLED
    int slot = mx_socket_slot(handle);
    if (slot < 0 || !hostname || !*hostname || tls_sockets_mx[slot] ||
        tls_servidores_mx[slot])
    {
        mx_set_exception("SocketTLSCliente: descriptor, hostname o estado invalido.");
        return 0;
    }
    SSL_CTX *context = mx_tls_client_context();
    if (!context)
        return 0;
    SSL *ssl = SSL_new(context);
    SSL_CTX_free(context);
    if (!ssl)
    {
        mx_tls_report_error("creacion de la sesion cliente", NULL);
        return 0;
    }
    if (SSL_set_tlsext_host_name(ssl, hostname) != 1 ||
        SSL_set1_host(ssl, hostname) != 1 ||
        SSL_set_fd(ssl, (int)sockets_mx[slot]) != 1)
    {
        mx_tls_report_error("configuracion de SNI/verificacion de hostname", ssl);
        SSL_free(ssl);
        return 0;
    }
    if (SSL_connect(ssl) != 1)
    {
        mx_tls_report_error("handshake cliente", ssl);
        SSL_free(ssl);
        return 0;
    }
    tls_sockets_mx[slot] = ssl;
    return 1;
#else
    (void)handle;
    (void)hostname;
    mx_set_exception("TLS no disponible: compile el programa con soporte OpenSSL.");
    return 0;
#endif
}

long mx_socket_tls_server(long handle, const char *certificate_path,
                          const char *private_key_path)
{
#if MX_OPENSSL_ENABLED
    int slot = mx_socket_slot(handle);
    if (slot < 0 || !certificate_path || !*certificate_path ||
        !private_key_path || !*private_key_path || tls_sockets_mx[slot] ||
        tls_servidores_mx[slot])
    {
        mx_set_exception("SocketTLSServidor: descriptor o rutas de certificado invalidas.");
        return 0;
    }
    SSL_CTX *context = SSL_CTX_new(TLS_server_method());
    if (!context)
    {
        mx_tls_report_error("creacion del contexto servidor", NULL);
        return 0;
    }
    if (SSL_CTX_set_min_proto_version(context, TLS1_2_VERSION) != 1 ||
        SSL_CTX_use_certificate_chain_file(context, certificate_path) != 1 ||
        SSL_CTX_use_PrivateKey_file(context, private_key_path, SSL_FILETYPE_PEM) != 1 ||
        SSL_CTX_check_private_key(context) != 1)
    {
        mx_tls_report_error("carga o validacion del certificado/clave", NULL);
        SSL_CTX_free(context);
        return 0;
    }
    tls_servidores_mx[slot] = context;
    return 1;
#else
    (void)handle;
    (void)certificate_path;
    (void)private_key_path;
    mx_set_exception("TLS no disponible: compile el programa con soporte OpenSSL.");
    return 0;
#endif
}

long mx_socket_tcp_connect(const char *host, long port)
{
    if (!host || !*host || port < 1 || port > 65535 || !mx_network_init())
    {
        mx_set_exception("SocketTCPConectar: host o puerto no valido.");
        return -1;
    }
    char service[16];
    snprintf(service, sizeof(service), "%ld", port);
    struct addrinfo hints = {0}, *addresses = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    int status = getaddrinfo(host, service, &hints, &addresses);
    if (status != 0)
    {
        mx_socket_error("resolucion de host", status);
        return -1;
    }

    mx_socket_t socket_fd = MX_INVALID_SOCKET;
    for (struct addrinfo *address = addresses; address; address = address->ai_next)
    {
        socket_fd = (mx_socket_t)socket(address->ai_family, address->ai_socktype,
                                        address->ai_protocol);
        if (socket_fd == MX_INVALID_SOCKET)
            continue;
        mx_socket_prevent_sigpipe(socket_fd);
        if (connect(socket_fd, address->ai_addr, (mx_socklen_t)address->ai_addrlen) == 0)
            break;
        mx_close(socket_fd);
        socket_fd = MX_INVALID_SOCKET;
    }
    freeaddrinfo(addresses);
    if (socket_fd == MX_INVALID_SOCKET)
    {
#ifdef _WIN32
        mx_socket_error("conexion TCP", WSAGetLastError());
#else
        mx_socket_error("conexion TCP", errno);
#endif
        return -1;
    }
    long handle = mx_socket_store(socket_fd);
    if (handle > 0)
    {
        struct sockaddr_storage peer;
        mx_socklen_t peer_length = (mx_socklen_t)sizeof(peer);
        if (getpeername(socket_fd, (struct sockaddr *)&peer, &peer_length) == 0)
            (void)mx_socket_set_remote((int)handle - 1,
                                       (struct sockaddr *)&peer, peer_length);
    }
    return handle;
}

long mx_socket_tcp_listen(long port, long backlog)
{
    if (port < 0 || port > 65535 || backlog < 1 || backlog > INT_MAX ||
        !mx_network_init())
    {
        mx_set_exception("SocketTCPEscuchar: puerto o backlog no valido.");
        return -1;
    }
    char service[16];
    snprintf(service, sizeof(service), "%ld", port);
    struct addrinfo hints = {0}, *addresses = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    int status = getaddrinfo(NULL, service, &hints, &addresses);
    if (status != 0)
    {
        mx_socket_error("resolucion del puerto", status);
        return -1;
    }

    mx_socket_t listener = MX_INVALID_SOCKET;
    for (struct addrinfo *address = addresses; address; address = address->ai_next)
    {
        listener = (mx_socket_t)socket(address->ai_family, address->ai_socktype,
                                       address->ai_protocol);
        if (listener == MX_INVALID_SOCKET)
            continue;
        if (address->ai_family == AF_INET6)
        {
            int dual_stack = 0;
            if (setsockopt(listener, IPPROTO_IPV6, IPV6_V6ONLY,
                           (const char *)&dual_stack, sizeof(dual_stack)) != 0)
            {
                mx_close(listener);
                listener = MX_INVALID_SOCKET;
                continue;
            }
        }
        int enabled = 1;
        (void)setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
                         (const char *)&enabled, sizeof(enabled));
        mx_socket_prevent_sigpipe(listener);
        if (bind(listener, address->ai_addr, (mx_socklen_t)address->ai_addrlen) == 0 &&
            listen(listener, (int)backlog) == 0)
            break;
        mx_close(listener);
        listener = MX_INVALID_SOCKET;
    }
    freeaddrinfo(addresses);
    if (listener == MX_INVALID_SOCKET)
    {
#ifdef _WIN32
        mx_socket_error("escucha TCP", WSAGetLastError());
#else
        mx_socket_error("escucha TCP", errno);
#endif
        return -1;
    }
    return mx_socket_store(listener);
}

long mx_socket_tcp_accept(long handle)
{
    int slot = mx_socket_slot(handle);
    if (slot < 0)
    {
        mx_set_exception("SocketTCPAceptar: descriptor invalido.");
        return -1;
    }
    struct sockaddr_storage peer;
    mx_socklen_t peer_length = (mx_socklen_t)sizeof(peer);
    mx_socket_t accepted = (mx_socket_t)accept(sockets_mx[slot],
                                               (struct sockaddr *)&peer, &peer_length);
    if (accepted == MX_INVALID_SOCKET)
    {
#ifdef _WIN32
        mx_socket_error("aceptacion TCP", WSAGetLastError());
#else
        mx_socket_error("aceptacion TCP", errno);
#endif
        return -1;
    }
    mx_socket_prevent_sigpipe(accepted);
#if MX_OPENSSL_ENABLED
    SSL *ssl = NULL;
    if (tls_servidores_mx[slot])
    {
        ssl = SSL_new(tls_servidores_mx[slot]);
        if (!ssl || SSL_set_fd(ssl, (int)accepted) != 1 || SSL_accept(ssl) != 1)
        {
            mx_tls_report_error("handshake servidor", ssl);
            if (ssl)
                SSL_free(ssl);
            mx_close(accepted);
            return -1;
        }
    }
#endif
    long new_handle = mx_socket_store(accepted);
    if (new_handle > 0)
    {
#if MX_OPENSSL_ENABLED
        if (ssl)
            tls_sockets_mx[new_handle - 1] = ssl;
#endif
        (void)mx_socket_set_remote((int)new_handle - 1, (struct sockaddr *)&peer, peer_length);
    }
#if MX_OPENSSL_ENABLED
    else if (ssl)
        SSL_free(ssl);
#endif
    return new_handle;
}

long mx_socket_send(long handle, const char *text)
{
    int slot = mx_socket_slot(handle);
    if (slot < 0 || !text)
    {
        mx_set_exception("SocketEnviar: descriptor o texto invalido.");
        return -1;
    }
    size_t length = strlen(text), sent = 0;
#if MX_OPENSSL_ENABLED
    SSL *tls = tls_sockets_mx[slot];
#endif
    while (sent < length)
    {
        size_t remaining = length - sent;
        size_t amount_sent = 0;
        int send_failed = 0;
#if MX_OPENSSL_ENABLED
        if (tls)
        {
            int chunk = remaining > INT_MAX ? INT_MAX : (int)remaining;
            int result = SSL_write(tls, text + sent, chunk);
            if (result <= 0)
                send_failed = 1;
            else
                amount_sent = (size_t)result;
        }
        else
#endif
        {
        long result;
#ifdef _WIN32
        int amount = remaining > INT_MAX ? INT_MAX : (int)remaining;
        result = send(sockets_mx[slot], text + sent, amount, 0);
#else
        int flags = 0;
#ifdef MSG_NOSIGNAL
        flags |= MSG_NOSIGNAL;
#endif
        result = (long)send(sockets_mx[slot], text + sent, remaining, flags);
#endif
        if (result <= 0)
            send_failed = 1;
        else
            amount_sent = (size_t)result;
        }
        if (send_failed)
        {
#if MX_OPENSSL_ENABLED
            if (tls)
                mx_tls_report_error("envio", tls);
            else
#endif
            {
#ifdef _WIN32
            mx_socket_error("envio TCP", WSAGetLastError());
#else
            mx_socket_error("envio TCP", errno);
#endif
            }
            return sent ? (long)sent : -1;
        }
        sent += amount_sent;
    }
    return (long)sent;
}

const char *mx_socket_receive(long handle, long max_bytes)
{
    int slot = mx_socket_slot(handle);
    if (slot < 0 || max_bytes < 1 || max_bytes >= MX_SOCKET_BUFFER_SIZE)
    {
        mx_set_exception("SocketRecibir: descriptor invalido o tamano fuera de 1..255.");
        return "";
    }
    int received;
#if MX_OPENSSL_ENABLED
    SSL *tls = tls_sockets_mx[slot];
    if (tls)
        received = SSL_read(tls, buffers_socket_mx[slot], (int)max_bytes);
    else
#endif
    {
#ifdef _WIN32
    received = recv(sockets_mx[slot], buffers_socket_mx[slot], (int)max_bytes, 0);
#else
    received = (int)recv(sockets_mx[slot], buffers_socket_mx[slot],
                         (size_t)max_bytes, 0);
#endif
    }
    if (received < 0)
    {
#if MX_OPENSSL_ENABLED
        if (tls)
            mx_tls_report_error("recepcion", tls);
        else
#endif
        {
#ifdef _WIN32
        mx_socket_error("recepcion TCP", WSAGetLastError());
#else
        mx_socket_error("recepcion TCP", errno);
#endif
        }
        buffers_socket_mx[slot][0] = '\0';
        return buffers_socket_mx[slot];
    }
    buffers_socket_mx[slot][received] = '\0';
    return buffers_socket_mx[slot];
}

const char *mx_socket_receive_line(long handle, long max_bytes)
{
    int slot = mx_socket_slot(handle);
    if (slot < 0 || max_bytes < 1 || max_bytes >= MX_SOCKET_BUFFER_SIZE)
    {
        mx_set_exception("SocketRecibirLinea: descriptor invalido o tamano fuera de 1..255.");
        return "";
    }

    size_t received = 0;
    int overflow = 0;
    for (;;)
    {
        char byte;
        int result;
#if MX_OPENSSL_ENABLED
        SSL *tls = tls_sockets_mx[slot];
        if (tls)
            result = SSL_read(tls, &byte, 1);
        else
#endif
        {
#ifdef _WIN32
            result = recv(sockets_mx[slot], &byte, 1, 0);
#else
            result = (int)recv(sockets_mx[slot], &byte, 1, 0);
#endif
        }
        if (result <= 0)
        {
            if (result < 0)
            {
#if MX_OPENSSL_ENABLED
                if (tls)
                    mx_tls_report_error("recepcion de linea", tls);
                else
#endif
                {
#ifdef _WIN32
                    mx_socket_error("recepcion TCP", WSAGetLastError());
#else
                    mx_socket_error("recepcion TCP", errno);
#endif
                }
            }
            break;
        }
        if (byte == '\n')
            break;
        if (byte == '\r')
            continue;
        if (received < (size_t)max_bytes)
            buffers_socket_mx[slot][received++] = byte;
        else
            overflow = 1;
    }
    buffers_socket_mx[slot][received] = '\0';
    if (overflow)
        mx_set_exception("SocketRecibirLinea: mensaje truncado al exceder el tamano maximo.");
    return buffers_socket_mx[slot];
}

long mx_socket_udp_bind(long port)
{
    if (port < 0 || port > 65535 || !mx_network_init())
    {
        mx_set_exception("SocketUDP: puerto no valido.");
        return -1;
    }
    char service[16];
    snprintf(service, sizeof(service), "%ld", port);
    struct addrinfo hints = {0}, *addresses = NULL;
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;
    int status = getaddrinfo(NULL, service, &hints, &addresses);
    if (status != 0)
    {
        mx_socket_error("resolucion UDP", status);
        return -1;
    }

    mx_socket_t socket_fd = MX_INVALID_SOCKET;
    for (struct addrinfo *address = addresses; address; address = address->ai_next)
    {
        socket_fd = (mx_socket_t)socket(address->ai_family, address->ai_socktype,
                                        address->ai_protocol);
        if (socket_fd == MX_INVALID_SOCKET)
            continue;
        if (address->ai_family == AF_INET6)
        {
            int dual_stack = 0;
            if (setsockopt(socket_fd, IPPROTO_IPV6, IPV6_V6ONLY,
                           (const char *)&dual_stack, sizeof(dual_stack)) != 0)
            {
                mx_close(socket_fd);
                socket_fd = MX_INVALID_SOCKET;
                continue;
            }
        }
        int enabled = 1;
        (void)setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR,
                         (const char *)&enabled, sizeof(enabled));
        if (bind(socket_fd, address->ai_addr, (mx_socklen_t)address->ai_addrlen) == 0)
            break;
        mx_close(socket_fd);
        socket_fd = MX_INVALID_SOCKET;
    }
    freeaddrinfo(addresses);
    if (socket_fd == MX_INVALID_SOCKET)
    {
#ifdef _WIN32
        mx_socket_error("bind UDP", WSAGetLastError());
#else
        mx_socket_error("bind UDP", errno);
#endif
        return -1;
    }
    return mx_socket_store(socket_fd);
}

long mx_socket_local_port(long handle)
{
    int slot = mx_socket_slot(handle);
    if (slot < 0)
    {
        mx_set_exception("SocketPuertoLocal: descriptor invalido.");
        return -1;
    }
    struct sockaddr_storage local;
    mx_socklen_t length = (mx_socklen_t)sizeof(local);
    if (getsockname(sockets_mx[slot], (struct sockaddr *)&local, &length) != 0)
    {
#ifdef _WIN32
        mx_socket_error("consulta del puerto local", WSAGetLastError());
#else
        mx_socket_error("consulta del puerto local", errno);
#endif
        return -1;
    }
    if (local.ss_family == AF_INET)
        return (long)ntohs(((struct sockaddr_in *)&local)->sin_port);
    if (local.ss_family == AF_INET6)
        return (long)ntohs(((struct sockaddr_in6 *)&local)->sin6_port);
    mx_set_exception("SocketPuertoLocal: familia de direccion no compatible.");
    return -1;
}

long mx_socket_udp_send(long handle, const char *host, long port, const char *text)
{
    int slot = mx_socket_slot(handle);
    if (slot < 0 || !host || port < 1 || port > 65535 || !text)
    {
        mx_set_exception("SocketEnviarUDP: argumentos invalidos.");
        return -1;
    }
    char service[16];
    snprintf(service, sizeof(service), "%ld", port);
    struct addrinfo hints = {0}, *addresses = NULL;
    struct sockaddr_storage local_address;
    mx_socklen_t local_length = (mx_socklen_t)sizeof(local_address);
    if (getsockname(sockets_mx[slot], (struct sockaddr *)&local_address,
                    &local_length) != 0)
    {
        mx_set_exception("SocketEnviarUDP: no se pudo consultar el socket local.");
        return -1;
    }
    hints.ai_family = local_address.ss_family;
    hints.ai_socktype = SOCK_DGRAM;
    int status = getaddrinfo(host, service, &hints, &addresses);
    if (status != 0)
    {
        mx_socket_error("resolucion del destino UDP", status);
        return -1;
    }
    size_t length = strlen(text);
    long sent = -1;
    for (struct addrinfo *address = addresses; address; address = address->ai_next)
    {
        struct sockaddr_in6 mapped_address;
        const struct sockaddr *destination = address->ai_addr;
        mx_socklen_t destination_length = (mx_socklen_t)address->ai_addrlen;
        if (local_address.ss_family == AF_INET6 && address->ai_family == AF_INET)
        {
            const struct sockaddr_in *ipv4 = (const struct sockaddr_in *)address->ai_addr;
            memset(&mapped_address, 0, sizeof(mapped_address));
            mapped_address.sin6_family = AF_INET6;
            mapped_address.sin6_port = ipv4->sin_port;
            mapped_address.sin6_addr.s6_addr[10] = 0xff;
            mapped_address.sin6_addr.s6_addr[11] = 0xff;
            memcpy(&mapped_address.sin6_addr.s6_addr[12], &ipv4->sin_addr, 4);
            destination = (const struct sockaddr *)&mapped_address;
            destination_length = (mx_socklen_t)sizeof(mapped_address);
        }
#ifdef _WIN32
        int result = sendto(sockets_mx[slot], text,
                            length > INT_MAX ? INT_MAX : (int)length, 0,
                            destination, (int)destination_length);
#else
        ssize_t result = sendto(sockets_mx[slot], text, length, 0,
                                destination, destination_length);
#endif
        if (result >= 0)
        {
            sent = (long)result;
            break;
        }
    }
    freeaddrinfo(addresses);
    if (sent < 0)
    {
#ifdef _WIN32
        mx_socket_error("envio UDP", WSAGetLastError());
#else
        mx_socket_error("envio UDP", errno);
#endif
    }
    return sent;
}

const char *mx_socket_udp_receive(long handle, long max_bytes)
{
    int slot = mx_socket_slot(handle);
    if (slot < 0 || max_bytes < 1 || max_bytes >= MX_SOCKET_BUFFER_SIZE)
    {
        mx_set_exception("SocketRecibirUDP: descriptor invalido o tamano fuera de 1..255.");
        return "";
    }
    struct sockaddr_storage peer;
    mx_socklen_t peer_length = (mx_socklen_t)sizeof(peer);
#ifdef _WIN32
    int received = recvfrom(sockets_mx[slot], buffers_socket_mx[slot],
                            (int)max_bytes, 0, (struct sockaddr *)&peer, &peer_length);
#else
    ssize_t received = recvfrom(sockets_mx[slot], buffers_socket_mx[slot],
                                (size_t)max_bytes, 0,
                                (struct sockaddr *)&peer, &peer_length);
#endif
    if (received < 0)
    {
#ifdef _WIN32
        mx_socket_error("recepcion UDP", WSAGetLastError());
#else
        mx_socket_error("recepcion UDP", errno);
#endif
        buffers_socket_mx[slot][0] = '\0';
        return buffers_socket_mx[slot];
    }
    buffers_socket_mx[slot][received] = '\0';
    if (!mx_socket_set_remote(slot, (struct sockaddr *)&peer, peer_length))
        remitentes_socket_mx[slot][0] = '\0';
    return buffers_socket_mx[slot];
}

const char *mx_socket_remote_address(long handle)
{
    int slot = mx_socket_slot(handle);
    if (slot < 0)
    {
        mx_set_exception("SocketDireccionRemota: descriptor invalido.");
        return "";
    }
    return remitentes_socket_mx[slot];
}

long mx_socket_set_timeout(long handle, long receive_ms, long send_ms)
{
    int slot = mx_socket_slot(handle);
    if (slot < 0 || receive_ms < 0 || send_ms < 0)
    {
        mx_set_exception("SocketTimeout: descriptor o timeout invalido.");
        return 0;
    }
#ifdef _WIN32
    DWORD receive_timeout = (DWORD)receive_ms;
    DWORD send_timeout = (DWORD)send_ms;
    int receive_ok = setsockopt(sockets_mx[slot], SOL_SOCKET, SO_RCVTIMEO,
                                (const char *)&receive_timeout, sizeof(receive_timeout));
    int send_ok = setsockopt(sockets_mx[slot], SOL_SOCKET, SO_SNDTIMEO,
                             (const char *)&send_timeout, sizeof(send_timeout));
#else
    struct timeval receive_timeout = {
        (time_t)(receive_ms / 1000), (suseconds_t)((receive_ms % 1000) * 1000)};
    struct timeval send_timeout = {
        (time_t)(send_ms / 1000), (suseconds_t)((send_ms % 1000) * 1000)};
    int receive_ok = setsockopt(sockets_mx[slot], SOL_SOCKET, SO_RCVTIMEO,
                                &receive_timeout, sizeof(receive_timeout));
    int send_ok = setsockopt(sockets_mx[slot], SOL_SOCKET, SO_SNDTIMEO,
                             &send_timeout, sizeof(send_timeout));
#endif
    if (receive_ok != 0 || send_ok != 0)
    {
#ifdef _WIN32
        mx_socket_error("configuracion de timeout", WSAGetLastError());
#else
        mx_socket_error("configuracion de timeout", errno);
#endif
        return 0;
    }
    return 1;
}

long mx_socket_close(long handle)
{
    int slot = mx_socket_slot(handle);
    if (slot < 0)
    {
        mx_set_exception("SocketCerrar: descriptor invalido.");
        return 0;
    }
    mx_socket_t socket_fd = sockets_mx[slot];
    sockets_mx[slot] = MX_INVALID_SOCKET;
    buffers_socket_mx[slot][0] = '\0';
    remitentes_socket_mx[slot][0] = '\0';
#if MX_OPENSSL_ENABLED
    if (tls_sockets_mx[slot])
    {
        (void)SSL_shutdown(tls_sockets_mx[slot]);
        SSL_free(tls_sockets_mx[slot]);
        tls_sockets_mx[slot] = NULL;
    }
    if (tls_servidores_mx[slot])
    {
        SSL_CTX_free(tls_servidores_mx[slot]);
        tls_servidores_mx[slot] = NULL;
    }
#endif
#ifdef _WIN32
    if (mx_close(socket_fd) == SOCKET_ERROR)
#else
    if (mx_close(socket_fd) != 0)
#endif
    {
#ifdef _WIN32
        mx_socket_error("cierre", WSAGetLastError());
#else
        mx_socket_error("cierre", errno);
#endif
        return 0;
    }
    return 1;
}

long mx_file_open(const char *path, const char *mode)
{
    if (!path || !mode)
    {
        mx_file_error("ruta o modo nulo");
        return -1;
    }
    if (strcmp(mode, "r") && strcmp(mode, "w") && strcmp(mode, "a") &&
        strcmp(mode, "r+") && strcmp(mode, "w+") && strcmp(mode, "a+"))
    {
        mx_file_error("modo no soportado; use r, w, a, r+, w+ o a+");
        return -1;
    }
    for (size_t i = 0; i < MX_MAX_OPEN_FILES; ++i)
    {
        if (!archivos_mx[i])
        {
            FILE *file = fopen(path, mode);
            if (!file)
            {
                mx_file_error("no se pudo abrir el archivo");
                return -1;
            }
            archivos_mx[i] = file;
            return (long)(i + 1);
        }
    }
    mx_file_error("se alcanzo el limite de archivos abiertos");
    return -1;
}

const char *mx_file_read_line(long handle)
{
    FILE *file = mx_get_file(handle);
    if (!file)
    {
        mx_file_error("descriptor invalido al leer");
        return "";
    }
    if (!fgets(buffer_archivo_mx, sizeof(buffer_archivo_mx), file))
    {
        if (ferror(file))
            mx_file_error("fallo la lectura de linea");
        return "";
    }
    return buffer_archivo_mx;
}

const char *mx_file_read_all(long handle)
{
    FILE *file = mx_get_file(handle);
    if (!file)
    {
        mx_file_error("descriptor invalido al leer");
        return "";
    }
    size_t used = 0;
    while (used + 1 < sizeof(buffer_archivo_mx))
    {
        size_t count = fread(buffer_archivo_mx + used, 1,
                             sizeof(buffer_archivo_mx) - used - 1, file);
        used += count;
        if (count == 0)
            break;
    }
    if (ferror(file))
    {
        mx_file_error("fallo la lectura del archivo");
        return "";
    }
    if (used + 1 == sizeof(buffer_archivo_mx) && fgetc(file) != EOF)
    {
        mx_file_error("el contenido excede el maximo de 255 bytes");
        return "";
    }
    buffer_archivo_mx[used] = '\0';
    return buffer_archivo_mx;
}

long mx_file_write(long handle, const char *text)
{
    FILE *file = mx_get_file(handle);
    if (!file || !text)
    {
        mx_file_error("descriptor o texto invalido al escribir");
        return -1;
    }
    size_t length = strlen(text);
    size_t written = fwrite(text, 1, length, file);
    if (written != length)
    {
        mx_file_error("fallo la escritura del archivo");
        return -1;
    }
    return (long)written;
}

long mx_file_eof(long handle)
{
    FILE *file = mx_get_file(handle);
    if (!file)
    {
        mx_file_error("descriptor invalido al consultar fin de archivo");
        return 0;
    }
    return feof(file);
}

long mx_file_close(long handle)
{
    FILE *file = mx_get_file(handle);
    if (!file)
    {
        mx_file_error("descriptor invalido al cerrar");
        return 0;
    }
    archivos_mx[(unsigned long)handle - 1] = NULL;
    if (fclose(file) != 0)
    {
        mx_file_error("fallo al cerrar el archivo");
        return 0;
    }
    return 1;
}

long mx_file_remove(const char *path)
{
    if (!path || remove(path) != 0)
    {
        mx_file_error("no se pudo eliminar el archivo");
        return 0;
    }
    return 1;
}

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
                        char *puerto, size_t puerto_size, char *ruta,
                        size_t ruta_size, int *secure)
{
    const char *inicio = NULL;
    if (strncmp(url, "http://", 7) == 0) inicio = url + 7;
    else if (strncmp(url, "https://", 8) == 0)
    {
        inicio = url + 8;
        *secure = 1;
    }
    else return 0;

    const char *separador = strpbrk(inicio, "/?#");
    size_t autoridad = separador ? (size_t)(separador - inicio) : strlen(inicio);
    const char *dos_puntos = memchr(inicio, ':', autoridad);
    size_t host_len = dos_puntos ? (size_t)(dos_puntos - inicio) : autoridad;
    if (host_len == 0 || host_len >= host_size) return 0;
    memcpy(host, inicio, host_len);
    host[host_len] = '\0';
    snprintf(puerto, puerto_size, "%s", dos_puntos ? dos_puntos + 1 :
             (*secure ? "443" : "80"));
    snprintf(ruta, ruta_size, "%s", separador ? separador : "/");
    return 1;
}

#if MX_OPENSSL_ENABLED
static int enviar_todo_tls(SSL *ssl, const char *datos, size_t longitud)
{
    while (longitud > 0)
    {
        int chunk = longitud > INT_MAX ? INT_MAX : (int)longitud;
        int enviado = SSL_write(ssl, datos, chunk);
        if (enviado <= 0)
            return 0;
        datos += enviado;
        longitud -= (size_t)enviado;
    }
    return 1;
}
#endif

static long mx_http_request_impl(const char *url, const char *metodo,
                                const char *cabeceras, const char *cuerpo,
                                int require_tls)
{
    respuesta[0] = '\0';
    if (!url || !metodo) return 0;

#ifdef _WIN32
    WSADATA datos_socket;
    if (WSAStartup(MAKEWORD(2, 2), &datos_socket) != 0) return 0;
#endif
    char host[256], puerto[16], ruta[2048];
    int secure = 0;
    if (!analizar_url(url, host, sizeof(host), puerto, sizeof(puerto),
                      ruta, sizeof(ruta), &secure)) {
        mx_set_exception("Http: URL invalida; use http:// o https://.");
#ifdef _WIN32
        WSACleanup();
#endif
        return 0;
    }
    if (require_tls && !secure)
    {
        mx_set_exception("HttpTLS requiere una URL https://.");
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
        mx_set_exception("Http: no se pudo resolver el servidor.");
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
        mx_set_exception("Http: no se pudo establecer la conexion.");
#ifdef _WIN32
        WSACleanup();
#endif
        return 0;
    }

#if MX_OPENSSL_ENABLED
    SSL_CTX *tls_context = NULL;
    SSL *tls = NULL;
#endif
    if (secure)
    {
#if MX_OPENSSL_ENABLED
        tls_context = mx_tls_client_context();
        if (tls_context)
            tls = SSL_new(tls_context);
        if (!tls || SSL_set_tlsext_host_name(tls, host) != 1 ||
            SSL_set1_host(tls, host) != 1 ||
            SSL_set_fd(tls, (int)socket_fd) != 1 || SSL_connect(tls) != 1)
        {
            if (tls)
                mx_tls_report_error("handshake HTTPS", tls);
            else if (tls_context)
                mx_tls_report_error("creacion de la sesion HTTPS", NULL);
            else
                mx_set_exception("HTTPS requiere soporte OpenSSL en el programa.");
            if (tls)
                SSL_free(tls);
            if (tls_context)
                SSL_CTX_free(tls_context);
            mx_close(socket_fd);
#ifdef _WIN32
            WSACleanup();
#endif
            return 0;
        }
#else
        (void)require_tls;
        mx_set_exception("HTTPS no disponible: compile el programa con soporte OpenSSL.");
        mx_close(socket_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return 0;
#endif
    }

    size_t cuerpo_len = cuerpo ? strlen(cuerpo) : 0;
    size_t headers_len = cabeceras ? strlen(cabeceras) : 0;
    size_t request_size = strlen(metodo) + strlen(ruta) + strlen(host) +
                          headers_len + cuerpo_len + 128;
    char *request = malloc(request_size);
    if (!request) {
#if MX_OPENSSL_ENABLED
        if (tls)
        {
            (void)SSL_shutdown(tls);
            SSL_free(tls);
        }
        if (tls_context)
            SSL_CTX_free(tls_context);
#endif
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
#if MX_OPENSSL_ENABLED
        if (tls)
        {
            (void)SSL_shutdown(tls);
            SSL_free(tls);
        }
        if (tls_context)
            SSL_CTX_free(tls_context);
#endif
        mx_close(socket_fd);
#ifdef _WIN32
        WSACleanup();
#endif
        return 0;
    }
    memcpy(request + used, "\r\n", 2);
    used += 2;
    if (cuerpo_len) { memcpy(request + used, cuerpo, cuerpo_len); used += cuerpo_len; }
#if MX_OPENSSL_ENABLED
    int ok = tls ? enviar_todo_tls(tls, request, used)
                 : enviar_todo(socket_fd, request, used);
#else
    int ok = enviar_todo(socket_fd, request, used);
#endif
    free(request);
    if (ok) {
        size_t total = 0;
        while (total + 1 < sizeof(respuesta)) {
#ifdef _WIN32
            int recibido;
#else
            ssize_t recibido;
#endif
#if MX_OPENSSL_ENABLED
            if (tls)
            {
                size_t remaining = sizeof(respuesta) - total - 1;
                int chunk = remaining > INT_MAX ? INT_MAX : (int)remaining;
                recibido = SSL_read(tls, respuesta + total, chunk);
            }
            else
#endif
            {
#ifdef _WIN32
                recibido = recv(socket_fd, respuesta + total,
                                (int)(sizeof(respuesta) - total - 1), 0);
#else
                recibido = recv(socket_fd, respuesta + total,
                                sizeof(respuesta) - total - 1, 0);
#endif
            }
            if (recibido <= 0) break;
            total += (size_t)recibido;
        }
        respuesta[total] = '\0';
    }
#if MX_OPENSSL_ENABLED
    if (tls)
    {
        (void)SSL_shutdown(tls);
        SSL_free(tls);
    }
    if (tls_context)
        SSL_CTX_free(tls_context);
#endif
    mx_close(socket_fd);
#ifdef _WIN32
    WSACleanup();
#endif
    if (!ok)
    {
        mx_set_exception("Http: fallo al enviar la solicitud.");
        return 0;
    }
    char *line_end = strstr(respuesta, "\r\n");
    if (!line_end)
    {
        mx_set_exception("Http: respuesta HTTP invalida o vacia.");
        return 0;
    }
    char *status = strchr(respuesta, ' ');
    return status ? strtol(status + 1, NULL, 10) : 0;
}

long mx_http_request(const char *url, const char *metodo,
                     const char *cabeceras, const char *cuerpo)
{
    return mx_http_request_impl(url, metodo, cabeceras, cuerpo, 0);
}

long mx_https_request(const char *url, const char *metodo,
                      const char *cabeceras, const char *cuerpo)
{
    return mx_http_request_impl(url, metodo, cabeceras, cuerpo, 1);
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
