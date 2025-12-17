#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

static volatile sig_atomic_t caught_signal = 0;
static int server_fd = -1;

static void signal_handler(int signo)
{
    if (signo == SIGINT || signo == SIGTERM) {
        caught_signal = 1;
    }
}

static int setup_signal_handlers(void)
{
    struct sigaction sa;
    
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        syslog(LOG_ERR, "Failed to set SIGINT handler: %s", strerror(errno));
        return -1;
    }
    
    if (sigaction(SIGTERM, &sa, NULL) == -1) {
        syslog(LOG_ERR, "Failed to set SIGTERM handler: %s", strerror(errno));
        return -1;
    }
    
    return 0;
}

static int append_to_file(const char *data, size_t len)
{
    int fd = open(DATA_FILE, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) {
        syslog(LOG_ERR, "Failed to open %s: %s", DATA_FILE, strerror(errno));
        return -1;
    }
    
    ssize_t written = write(fd, data, len);
    close(fd);
    
    if (written == -1 || (size_t)written != len) {
        syslog(LOG_ERR, "Failed to write to %s: %s", DATA_FILE, strerror(errno));
        return -1;
    }
    
    return 0;
}

static int send_file_contents(int client_fd)
{
    int fd = open(DATA_FILE, O_RDONLY);
    if (fd == -1) {
        if (errno == ENOENT) {
            return 0;
        }
        syslog(LOG_ERR, "Failed to open %s for reading: %s", DATA_FILE, strerror(errno));
        return -1;
    }
    
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    
    while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
        ssize_t total_sent = 0;
        while (total_sent < bytes_read) {
            ssize_t sent = send(client_fd, buffer + total_sent, bytes_read - total_sent, 0);
            if (sent == -1) {
                syslog(LOG_ERR, "Failed to send data to client: %s", strerror(errno));
                close(fd);
                return -1;
            }
            total_sent += sent;
        }
    }
    
    close(fd);
    
    if (bytes_read == -1) {
        syslog(LOG_ERR, "Failed to read from %s: %s", DATA_FILE, strerror(errno));
        return -1;
    }
    
    return 0;
}

static void handle_client(int client_fd, const char *client_ip)
{
    char *recv_buffer = NULL;
    size_t recv_buffer_size = 0;
    size_t recv_buffer_len = 0;
    char temp_buffer[BUFFER_SIZE];
    
    while (!caught_signal) {
        ssize_t bytes_received = recv(client_fd, temp_buffer, sizeof(temp_buffer), 0);
        
        if (bytes_received <= 0) {
            if (bytes_received == 0) break;
            if (errno == EINTR) continue;
            syslog(LOG_ERR, "recv error: %s", strerror(errno));
            break;
        }
        
        /* Grow buffer if needed */
        size_t new_len = recv_buffer_len + bytes_received;
        if (new_len > recv_buffer_size) {
            size_t new_size = recv_buffer_size == 0 ? BUFFER_SIZE : recv_buffer_size * 2;
            while (new_size < new_len) {
                new_size *= 2;
            }
            char *new_buffer = realloc(recv_buffer, new_size);
            if (new_buffer == NULL) {
                syslog(LOG_ERR, "Failed to allocate memory: %s", strerror(errno));
                break;
            }
            recv_buffer = new_buffer;
            recv_buffer_size = new_size;
        }
        
        /* Append received data to buffer */
        memcpy(recv_buffer + recv_buffer_len, temp_buffer, bytes_received);
        recv_buffer_len = new_len;
        
        /* Check for complete packets (newline-terminated) */
        char *newline;
        char *search_start = recv_buffer;
        size_t remaining = recv_buffer_len;
        
        while ((newline = memchr(search_start, '\n', remaining)) != NULL) {
            size_t packet_len = newline - search_start + 1; /* Include the newline */
            
            /* Append packet to file */
            if (append_to_file(search_start, packet_len) == -1) {
                syslog(LOG_ERR, "Failed to append data to file");
            }
            
            /* Send file contents to client */
            if (send_file_contents(client_fd) == -1) {
                syslog(LOG_ERR, "Failed to send file contents to client");
            }
            
            search_start = newline + 1;
            remaining = recv_buffer_len - (search_start - recv_buffer);
        }
        
        /* Move any remaining partial data to start of buffer */
        if (search_start > recv_buffer && remaining > 0) {
            memmove(recv_buffer, search_start, remaining);
        }
        recv_buffer_len = remaining;
    }
    
    free(recv_buffer);
    syslog(LOG_INFO, "Closed connection from %s", client_ip);
}

/**
 * Cleanup resources and exit
 */
static void cleanup_and_exit(void)
{
    syslog(LOG_INFO, "Caught signal, exiting");
    
    if (server_fd != -1) {
        close(server_fd);
        server_fd = -1;
    }
    
    /* Delete the data file */
    unlink(DATA_FILE);
    
    closelog();
}

int main(int argc, char *argv[])
{
    int ret = 0;
    int daemon_mode = 0;
    int opt;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    
    /* Parse command line arguments */
    while ((opt = getopt(argc, argv, "d")) != -1) {
        switch (opt) {
            case 'd':
                daemon_mode = 1;
                break;
            default:
                fprintf(stderr, "Usage: %s [-d]\n", argv[0]);
                return -1;
        }
    }
    
    /* Open syslog */
    openlog("aesdsocket", LOG_PID | LOG_CONS, LOG_USER);
    
    /* Setup signal handlers */
    if (setup_signal_handlers() == -1) {
        closelog();
        return -1;
    }
    
    /* Create socket */
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) {
        syslog(LOG_ERR, "Failed to create socket: %s", strerror(errno));
        closelog();
        return -1;
    }
    
    /* Set socket options */
    int reuse = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) == -1) {
        syslog(LOG_ERR, "Failed to set socket options: %s", strerror(errno));
        close(server_fd);
        closelog();
        return -1;
    }
    
    /* Bind to port */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        syslog(LOG_ERR, "Failed to bind socket: %s", strerror(errno));
        close(server_fd);
        closelog();
        return -1;
    }
    
    /* Fork to daemon mode after successful bind */
    if (daemon_mode) {
        pid_t pid = fork();
        if (pid == -1) {
            syslog(LOG_ERR, "Failed to fork: %s", strerror(errno));
            close(server_fd);
            closelog();
            return -1;
        }
        if (pid > 0) {
            /* Parent exits */
            exit(0);
        }
        /* Child continues as daemon */
        
        /* Create new session */
        if (setsid() == -1) {
            syslog(LOG_ERR, "Failed to create new session: %s", strerror(errno));
            close(server_fd);
            closelog();
            return -1;
        }
        
        /* Change working directory to root */
        if (chdir("/") == -1) {
            syslog(LOG_ERR, "Failed to change directory: %s", strerror(errno));
        }
        
        /* Redirect stdin, stdout, stderr to /dev/null */
        int devnull = open("/dev/null", O_RDWR);
        if (devnull != -1) {
            dup2(devnull, STDIN_FILENO);
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            if (devnull > STDERR_FILENO) {
                close(devnull);
            }
        }
    }
    
    /* Listen for connections */
    if (listen(server_fd, 10) == -1) {
        syslog(LOG_ERR, "Failed to listen on socket: %s", strerror(errno));
        close(server_fd);
        closelog();
        return -1;
    }
    
    syslog(LOG_INFO, "Server listening on port %d", PORT);
    
    while (!caught_signal) {
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_len);
        
        if (client_fd == -1) {
            if (errno == EINTR) {
                continue;
            }
            syslog(LOG_ERR, "Failed to accept connection: %s", strerror(errno));
            continue;
        }
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);
        
        handle_client(client_fd, client_ip);
        
        close(client_fd);
    }
    
    cleanup_and_exit();
    
    return ret;
}

