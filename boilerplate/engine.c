#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sched.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <sys/mount.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/ioctl.h>

#include "monitor_ioctl.h"

#define STACK_SIZE (1024 * 1024)
#define CONTROL_PATH "/tmp/mini_runtime.sock"

typedef struct container {
    char id[32];
    pid_t pid;
    int log_fd;
    struct container *next;
} container_t;

static container_t *head = NULL;
static char stack[STACK_SIZE];

struct child_args {
    char *rootfs;
    char *cmd;
    int pipe_fd;
};

int child_fn(void *arg) {
    struct child_args *args = (struct child_args *)arg;

    setpgid(0, 0);

    // 🔥 REDIRECT OUTPUT TO PIPE
    dup2(args->pipe_fd, STDOUT_FILENO);
    dup2(args->pipe_fd, STDERR_FILENO);
    close(args->pipe_fd);

    chroot(args->rootfs);
    chdir("/");

    mkdir("/proc", 0555);
    mount("proc", "/proc", "proc", 0, NULL);

    char *argv[] = {args->cmd, NULL};
    execv(args->cmd, argv);

    perror("exec failed");
    return 1;
}

void add_container(char *id, pid_t pid, int log_fd) {
    container_t *c = malloc(sizeof(container_t));
    strcpy(c->id, id);
    c->pid = pid;
    c->log_fd = log_fd;
    c->next = head;
    head = c;
}

container_t* find_container(char *id) {
    container_t *c = head;
    while (c) {
        if (strcmp(c->id, id) == 0)
            return c;
        c = c->next;
    }
    return NULL;
}

void handle_start(int fd, char *id, char *rootfs, char *cmd) {

    int pipefd[2];
    pipe(pipefd);

    struct child_args args;
    args.rootfs = rootfs;
    args.cmd = cmd;
    args.pipe_fd = pipefd[1];

    pid_t pid = clone(child_fn,
                      stack + STACK_SIZE,
                      CLONE_NEWPID | CLONE_NEWUTS | CLONE_NEWNS | CLONE_NEWIPC | SIGCHLD,
                      &args);

    if (pid < 0) {
        write(fd, "FAIL\n", 5);
        return;
    }

    close(pipefd[1]); // parent closes write end
    add_container(id, pid, pipefd[0]);

    // Register with kernel
    int mfd = open("/dev/container_monitor", O_RDWR);
    if (mfd >= 0) {
        struct monitor_request req;
        strcpy(req.container_id, id);
        req.pid = pid;
        req.soft_limit_bytes = 40 * 1024 * 1024;
        req.hard_limit_bytes = 64 * 1024 * 1024;
        ioctl(mfd, MONITOR_REGISTER, &req);
        close(mfd);
    }

    write(fd, "OK\n", 3);
}

void handle_logs(int fd, char *id) {
    container_t *c = find_container(id);
    if (!c) {
        write(fd, "NOT FOUND\n", 10);
        return;
    }

    char buffer[256];
    int n;

    while ((n = read(c->log_fd, buffer, sizeof(buffer))) > 0) {
        write(fd, buffer, n);
    }
}

void handle_ps(int fd) {
    char buf[256];
    container_t *c = head;

    while (c) {
        snprintf(buf, sizeof(buf), "%s : %d\n", c->id, c->pid);
        write(fd, buf, strlen(buf));
        c = c->next;
    }
}

void handle_stop(int fd, char *id) {
    container_t *c = find_container(id);
    if (!c) {
        write(fd, "NOT FOUND\n", 10);
        return;
    }

    kill(-c->pid, SIGKILL);
    write(fd, "STOPPED\n", 8);
}

void run_supervisor() {
    int server_fd, client_fd;
    struct sockaddr_un addr;

    unlink(CONTROL_PATH);

    server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, CONTROL_PATH);

    bind(server_fd, (struct sockaddr*)&addr, sizeof(addr));
    listen(server_fd, 5);

    printf("Supervisor running...\n");

    while (1) {
        int status;
        while (waitpid(-1, &status, WNOHANG) > 0);

        client_fd = accept(server_fd, NULL, NULL);

        char buf[256] = {0};
        read(client_fd, buf, sizeof(buf));

        char cmd[16], id[32], rootfs[128], prog[128];
        sscanf(buf, "%s %s %s %s", cmd, id, rootfs, prog);

        if (strcmp(cmd, "start") == 0)
            handle_start(client_fd, id, rootfs, prog);
        else if (strcmp(cmd, "ps") == 0)
            handle_ps(client_fd);
        else if (strcmp(cmd, "stop") == 0)
            handle_stop(client_fd, id);
        else if (strcmp(cmd, "logs") == 0)
            handle_logs(client_fd, id);

        close(client_fd);
    }
}

void send_req(char *msg) {
    int sock;
    struct sockaddr_un addr;

    sock = socket(AF_UNIX, SOCK_STREAM, 0);
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, CONTROL_PATH);

    connect(sock, (struct sockaddr*)&addr, sizeof(addr));
    write(sock, msg, strlen(msg));

    char buf[512];
    int n;

    while ((n = read(sock, buf, sizeof(buf)-1)) > 0) {
        buf[n] = 0;
        printf("%s", buf);
    }

    close(sock);
}

int main(int argc, char *argv[]) {

    if (argc < 2) return 1;

    if (strcmp(argv[1], "supervisor") == 0)
        run_supervisor();

    else if (strcmp(argv[1], "start") == 0) {
        char msg[256];
        sprintf(msg, "start %s %s %s", argv[2], argv[3], argv[4]);
        send_req(msg);
    }

    else if (strcmp(argv[1], "ps") == 0)
        send_req("ps");

    else if (strcmp(argv[1], "stop") == 0) {
        char msg[256];
        sprintf(msg, "stop %s", argv[2]);
        send_req(msg);
    }

    else if (strcmp(argv[1], "logs") == 0) {
        char msg[256];
        sprintf(msg, "logs %s", argv[2]);
        send_req(msg);
    }

    return 0;
}
