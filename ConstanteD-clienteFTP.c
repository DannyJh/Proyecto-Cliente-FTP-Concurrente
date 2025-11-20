#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <fcntl.h>

#define BUFFER_SIZE 8192
#define CMD_SIZE 512
#define CONNECT_TIMEOUT 10

extern int connectTCP(const char*, const char*);
extern int errexit(const char*, ...);

typedef struct {
    int control_sock;
    char username[128];
    char password[128];
    int logged_in;
} FTPSession;

void handle_sigchld(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

int read_response(int sock, char* buffer, int size) {
    int n = 0, total = 0;
    char c;

    memset(buffer, 0, size);
    while (total < size - 1) {
        n = recv(sock, &c, 1, 0);
        if (n <= 0) break;
        buffer[total++] = c;
        if (total >= 4 && buffer[total - 2] == '\r' && buffer[total - 1] == '\n') {
            if (total < 4 || buffer[0] != ' ') {
                if (buffer[3] == ' ') break;
            }
        }
    }
    buffer[total] = '\0';
    return total;
}

int send_command(int sock, const char* cmd) {
    char buffer[CMD_SIZE];
    snprintf(buffer, CMD_SIZE, "%s\r\n", cmd);
    return send(sock, buffer, strlen(buffer), 0);
}

int get_response_code(const char* response) {
    if (strlen(response) >= 3) {
        return atoi(response);
    }
    return 0;
}

int ftp_login(FTPSession* session, const char* user, const char* pass) {
    char response[BUFFER_SIZE];
    char cmd[CMD_SIZE];

    snprintf(cmd, CMD_SIZE, "USER %s", user);
    send_command(session->control_sock, cmd);
    read_response(session->control_sock, response, BUFFER_SIZE);
    printf("%s", response);

    int code = get_response_code(response);
    if (code == 331 || code == 230) {
        if (code == 331) {
            snprintf(cmd, CMD_SIZE, "PASS %s", pass);
            send_command(session->control_sock, cmd);
            read_response(session->control_sock, response, BUFFER_SIZE);
            printf("%s", response);
            code = get_response_code(response);
        }

        if (code == 230) {
            session->logged_in = 1;
            strcpy(session->username, user);
            strcpy(session->password, pass);
            return 1;
        }
    }
    return 0;
}

int connect_with_timeout(const char* host, int port, int timeout_sec) {
    struct sockaddr_in addr;
    int sock, flags, result;
    fd_set write_fds;
    struct timeval tv;
    socklen_t len;
    int error;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    // Hacer el socket no bloqueante
    flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host, &addr.sin_addr) <= 0) {
        close(sock);
        return -1;
    }

    result = connect(sock, (struct sockaddr*)&addr, sizeof(addr));

    if (result < 0) {
        if (errno != EINPROGRESS) {
            close(sock);
            return -1;
        }

        FD_ZERO(&write_fds);
        FD_SET(sock, &write_fds);
        tv.tv_sec = timeout_sec;
        tv.tv_usec = 0;

        result = select(sock + 1, NULL, &write_fds, NULL, &tv);

        if (result <= 0) {
            close(sock);
            return -1;
        }

        len = sizeof(error);
        if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
            close(sock);
            return -1;
        }
    }

    // Restaurar a modo bloqueante
    fcntl(sock, F_SETFL, flags);

    return sock;
}

int enter_passive_mode(FTPSession* session, char* ip, int* port) {
    char response[BUFFER_SIZE];
    char* start;
    int h1, h2, h3, h4, p1, p2;

    send_command(session->control_sock, "PASV");
    read_response(session->control_sock, response, BUFFER_SIZE);
    printf("%s", response);

    if (get_response_code(response) != 227) {
        return 0;
    }

    start = strchr(response, '(');
    if (!start) return 0;
    start++;

    if (sscanf(start, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2) != 6) {
        return 0;
    }

    sprintf(ip, "%d.%d.%d.%d", h1, h2, h3, h4);
    *port = p1 * 256 + p2;

    return 1;
}

int enter_active_mode(FTPSession* session, int* data_sock) {
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);
    int listen_sock, port;
    char cmd[CMD_SIZE];
    char response[BUFFER_SIZE];
    unsigned char* ip_bytes;

    listen_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock < 0) return 0;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0;

    if (bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(listen_sock);
        return 0;
    }

    if (listen(listen_sock, 1) < 0) {
        close(listen_sock);
        return 0;
    }

    if (getsockname(listen_sock, (struct sockaddr*)&addr, &addr_len) < 0) {
        close(listen_sock);
        return 0;
    }

    if (getsockname(session->control_sock, (struct sockaddr*)&addr, &addr_len) < 0) {
        close(listen_sock);
        return 0;
    }

    ip_bytes = (unsigned char*)&addr.sin_addr.s_addr;

    if (getsockname(listen_sock, (struct sockaddr*)&addr, &addr_len) < 0) {
        close(listen_sock);
        return 0;
    }
    port = ntohs(addr.sin_port);

    snprintf(cmd, CMD_SIZE, "PORT %d,%d,%d,%d,%d,%d",
        ip_bytes[0], ip_bytes[1], ip_bytes[2], ip_bytes[3],
        port / 256, port % 256);

    send_command(session->control_sock, cmd);
    read_response(session->control_sock, response, BUFFER_SIZE);
    printf("%s", response);

    if (get_response_code(response) != 200) {
        close(listen_sock);
        return 0;
    }

    *data_sock = listen_sock;
    return 1;
}

void ftp_retrieve_file(FTPSession* session, const char* remote_file, const char* local_file) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return;
    }

    if (pid == 0) {
        char response[BUFFER_SIZE];
        char cmd[CMD_SIZE];
        char ip[32];
        int port, data_sock;
        FILE* fp;
        int n;
        char buffer[BUFFER_SIZE];

        printf("[PID %d] Starting download: %s\n", getpid(), remote_file);

        if (!enter_passive_mode(session, ip, &port)) {
            fprintf(stderr, "[PID %d] Failed to enter passive mode\n", getpid());
            exit(1);
        }

        printf("[PID %d] Connecting to %s:%d with timeout...\n", getpid(), ip, port);
        data_sock = connect_with_timeout(ip, port, CONNECT_TIMEOUT);

        if (data_sock < 0) {
            fprintf(stderr, "[PID %d] Failed to connect to data port %s:%d (timeout or refused)\n",
                getpid(), ip, port);
            fprintf(stderr, "[PID %d] Try with a smaller file or different server\n", getpid());
            exit(1);
        }

        printf("[PID %d] Data connection established\n", getpid());

        snprintf(cmd, CMD_SIZE, "RETR %s", remote_file);
        send_command(session->control_sock, cmd);
        read_response(session->control_sock, response, BUFFER_SIZE);
        printf("%s", response);

        if (get_response_code(response) != 150 && get_response_code(response) != 125) {
            close(data_sock);
            exit(1);
        }

        fp = fopen(local_file, "wb");
        if (!fp) {
            perror("fopen");
            close(data_sock);
            exit(1);
        }

        printf("[PID %d] Downloading %s...\n", getpid(), remote_file);
        while ((n = recv(data_sock, buffer, BUFFER_SIZE, 0)) > 0) {
            fwrite(buffer, 1, n, fp);
        }

        fclose(fp);
        close(data_sock);

        read_response(session->control_sock, response, BUFFER_SIZE);
        printf("%s", response);

        printf("[PID %d] Download complete: %s\n", getpid(), local_file);
        exit(0);
    }
}

void ftp_store_file(FTPSession* session, const char* local_file, const char* remote_file) {
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return;
    }

    if (pid == 0) {
        char response[BUFFER_SIZE];
        char cmd[CMD_SIZE];
        char ip[32];
        int port, data_sock;
        FILE* fp;
        int n;
        char buffer[BUFFER_SIZE];

        printf("[PID %d] Starting upload: %s\n", getpid(), local_file);

        fp = fopen(local_file, "rb");
        if (!fp) {
            perror("fopen");
            exit(1);
        }

        if (!enter_passive_mode(session, ip, &port)) {
            fprintf(stderr, "[PID %d] Failed to enter passive mode\n", getpid());
            fclose(fp);
            exit(1);
        }

        printf("[PID %d] Connecting to %s:%d with timeout...\n", getpid(), ip, port);
        data_sock = connect_with_timeout(ip, port, CONNECT_TIMEOUT);

        if (data_sock < 0) {
            fprintf(stderr, "[PID %d] Failed to connect to data port %s:%d (timeout or refused)\n",
                getpid(), ip, port);
            fprintf(stderr, "[PID %d] This may happen with large files on public servers\n", getpid());
            fprintf(stderr, "[PID %d] Try: 1) Smaller file, 2) Local FTP server, 3) Different server\n", getpid());
            fclose(fp);
            exit(1);
        }

        printf("[PID %d] Data connection established\n", getpid());

        snprintf(cmd, CMD_SIZE, "STOR %s", remote_file);
        send_command(session->control_sock, cmd);
        read_response(session->control_sock, response, BUFFER_SIZE);
        printf("%s", response);

        if (get_response_code(response) != 150 && get_response_code(response) != 125) {
            fclose(fp);
            close(data_sock);
            exit(1);
        }

        printf("[PID %d] Uploading %s...\n", getpid(), local_file);

        long total_bytes = 0;
        while ((n = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
            int sent = send(data_sock, buffer, n, 0);
            if (sent < 0) {
                fprintf(stderr, "[PID %d] Send error: %s\n", getpid(), strerror(errno));
                break;
            }
            total_bytes += sent;
            if (total_bytes % (1024 * 1024) == 0) {
                printf("[PID %d] Uploaded %ld MB...\n", getpid(), total_bytes / (1024 * 1024));
            }
        }

        fclose(fp);
        close(data_sock);

        read_response(session->control_sock, response, BUFFER_SIZE);
        printf("%s", response);

        printf("[PID %d] Upload complete: %s (%ld bytes)\n", getpid(), remote_file, total_bytes);
        exit(0);
    }
}

void ftp_pwd(FTPSession* session) {
    char response[BUFFER_SIZE];
    send_command(session->control_sock, "PWD");
    read_response(session->control_sock, response, BUFFER_SIZE);
    printf("%s", response);
}

void ftp_mkd(FTPSession* session, const char* dirname) {
    char response[BUFFER_SIZE];
    char cmd[CMD_SIZE];
    snprintf(cmd, CMD_SIZE, "MKD %s", dirname);
    send_command(session->control_sock, cmd);
    read_response(session->control_sock, response, BUFFER_SIZE);
    printf("%s", response);
}

void ftp_dele(FTPSession* session, const char* filename) {
    char response[BUFFER_SIZE];
    char cmd[CMD_SIZE];
    snprintf(cmd, CMD_SIZE, "DELE %s", filename);
    send_command(session->control_sock, cmd);
    read_response(session->control_sock, response, BUFFER_SIZE);
    printf("%s", response);
}

void print_usage(const char* prog) {
    printf("Usage: %s <host> <port>\n", prog);
    printf("Commands:\n");
    printf("  login <user> <pass>  - Login to FTP server\n");
    printf("  get <remote> <local> - Download file (concurrent)\n");
    printf("  put <local> <remote> - Upload file (concurrent)\n");
    printf("  pwd                  - Print working directory\n");
    printf("  mkdir <dir>          - Create directory\n");
    printf("  delete <file>        - Delete file\n");
    printf("  quit                 - Exit\n");
}

int main(int argc, char* argv[]) {
    FTPSession session;
    char response[BUFFER_SIZE];
    char input[CMD_SIZE];
    char cmd[64], arg1[256], arg2[256];

    if (argc != 3) {
        print_usage(argv[0]);
        return 1;
    }

    signal(SIGCHLD, handle_sigchld);

    printf("Connecting to %s:%s...\n", argv[1], argv[2]);
    session.control_sock = connectTCP(argv[1], argv[2]);
    session.logged_in = 0;

    read_response(session.control_sock, response, BUFFER_SIZE);
    printf("%s", response);

    printf("\n");
    print_usage(argv[0]);
    printf("\n");

    while (1) {
        printf("ftp> ");
        fflush(stdout);

        if (!fgets(input, CMD_SIZE, stdin)) break;

        input[strcspn(input, "\n")] = 0;

        if (strlen(input) == 0) continue;

        memset(cmd, 0, sizeof(cmd));
        memset(arg1, 0, sizeof(arg1));
        memset(arg2, 0, sizeof(arg2));

        sscanf(input, "%s %s %s", cmd, arg1, arg2);

        if (strcmp(cmd, "quit") == 0 || strcmp(cmd, "exit") == 0) {
            send_command(session.control_sock, "QUIT");
            read_response(session.control_sock, response, BUFFER_SIZE);
            printf("%s", response);
            break;
        }
        else if (strcmp(cmd, "login") == 0) {
            if (strlen(arg1) == 0 || strlen(arg2) == 0) {
                printf("Usage: login <user> <pass>\n");
                continue;
            }
            ftp_login(&session, arg1, arg2);
        }
        else if (strcmp(cmd, "get") == 0) {
            if (!session.logged_in) {
                printf("Please login first\n");
                continue;
            }
            if (strlen(arg1) == 0 || strlen(arg2) == 0) {
                printf("Usage: get <remote> <local>\n");
                continue;
            }
            ftp_retrieve_file(&session, arg1, arg2);
        }
        else if (strcmp(cmd, "put") == 0) {
            if (!session.logged_in) {
                printf("Please login first\n");
                continue;
            }
            if (strlen(arg1) == 0 || strlen(arg2) == 0) {
                printf("Usage: put <local> <remote>\n");
                continue;
            }
            ftp_store_file(&session, arg1, arg2);
        }
        else if (strcmp(cmd, "pwd") == 0) {
            if (!session.logged_in) {
                printf("Please login first\n");
                continue;
            }
            ftp_pwd(&session);
        }
        else if (strcmp(cmd, "mkdir") == 0) {
            if (!session.logged_in) {
                printf("Please login first\n");
                continue;
            }
            if (strlen(arg1) == 0) {
                printf("Usage: mkdir <dir>\n");
                continue;
            }
            ftp_mkd(&session, arg1);
        }
        else if (strcmp(cmd, "delete") == 0) {
            if (!session.logged_in) {
                printf("Please login first\n");
                continue;
            }
            if (strlen(arg1) == 0) {
                printf("Usage: delete <file>\n");
                continue;
            }
            ftp_dele(&session, arg1);
        }
        else {
            printf("Unknown command: %s\n", cmd);
            print_usage(argv[0]);
        }
    }

    close(session.control_sock);
    printf("Connection closed\n");

    return 0;
}