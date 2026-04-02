#define _POSIX_C_SOURCE 200809L

#include <mqueue.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "common.h"

int p_id;
mqd_t mq_srv;
mqd_t mq_self;
char self_q_name[20];
char lock_path[64];
int lock_fd = -1;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int waiting_validation = 0;
int validation_ready = 0;
int validation_ok = 0;
int running = 1;
int modo_isolado = 0;
char modo_porta = 'N';
char cache_cod[CMAX][NDIG + 1];
int cache_valid[CMAX];
int cache_pos = 0;

static void cleanup_ctl(void)
{
    if (mq_srv != (mqd_t)-1)
    {
        mq_close(mq_srv);
        mq_srv = (mqd_t)-1;
    }
    if (mq_self != (mqd_t)-1)
    {
        mq_close(mq_self);
        mq_self = (mqd_t)-1;
    }
    if (self_q_name[0] != '\0')
        mq_unlink(self_q_name);
    if (lock_fd >= 0)
    {
        close(lock_fd);
        lock_fd = -1;
    }
    if (lock_path[0] != '\0')
        unlink(lock_path);
}

static void encerra_ctl(int sn)
{
    (void)sn;
    running = 0;
    cleanup_ctl();
    exit(0);
}

static int create_controller_lock(void)
{
    int pid_file;
    char buf[32];
    ssize_t n;
    long old_pid;

    snprintf(lock_path, sizeof(lock_path), "/tmp/SCActl_%d.lock", p_id);

    lock_fd = open(lock_path, O_CREAT | O_EXCL | O_RDWR, 0666);
    if (lock_fd < 0 && errno == EEXIST)
    {
        pid_file = open(lock_path, O_RDONLY);
        if (pid_file >= 0)
        {
            n = read(pid_file, buf, sizeof(buf) - 1);
            close(pid_file);
            if (n > 0)
            {
                buf[n] = '\0';
                old_pid = strtol(buf, NULL, 10);
                if (old_pid > 0 && kill((pid_t)old_pid, 0) == 0)
                    return -1;
            }
        }

        unlink(lock_path);
        lock_fd = open(lock_path, O_CREAT | O_EXCL | O_RDWR, 0666);
    }

    if (lock_fd < 0)
        return -1;

    snprintf(buf, sizeof(buf), "%ld\n", (long)getpid());
    write(lock_fd, buf, strlen(buf));
    return 0;
}

static int cache_find(const char *cod)
{
    int i;
    for (i = 0; i < CMAX; i++)
    {
        if (cache_valid[i] && strcmp(cache_cod[i], cod) == 0)
            return 1;
    }
    return 0;
}

static void cache_add(const char *cod)
{
    cache_valid[cache_pos] = 1;
    strncpy(cache_cod[cache_pos], cod, NDIG);
    cache_cod[cache_pos][NDIG] = '\0';
    cache_pos = (cache_pos + 1) % CMAX;
}

static void cache_clear(void)
{
    int i;
    for (i = 0; i < CMAX; i++)
        cache_valid[i] = 0;
}

void *thread_servidor(void *arg)
{
    ctl_msg_t msg;

    (void)arg;

    while (running)
    {
        if (mq_receive(mq_self, (char *)&msg, sizeof(msg), NULL) <= 0)
            continue;

        pthread_mutex_lock(&mtx);
        if (msg.type == 'v')
        {
            validation_ok = msg.ok;
            validation_ready = 1;
            pthread_cond_signal(&cond);
        }
        else if (msg.type == 'M')
        {
            modo_porta = msg.estado;
            printf("\nSrv: modo porta %d = %c\n", p_id, modo_porta);
        }
        else if (msg.type == 'C')
        {
            cache_clear();
            printf("\nSrv: cache limpa\n");
        }
        else if (msg.type == 'T')
        {
            running = 0;
            printf("\nSrv: termino controlador\n");
            pthread_mutex_unlock(&mtx);
            encerra_ctl(0);
        }
        pthread_mutex_unlock(&mtx);
    }

    return NULL;
}

static int valida_remota(const char *pin)
{
    ctl_msg_t req;
    struct timespec ts;
    int rc;

    if (modo_isolado || mq_srv == (mqd_t)-1)
        return 0;

    memset(&req, 0, sizeof(req));
    req.type = 'V';
    req.porta = (char)p_id;
    strncpy(req.codigo, pin, NDIG);

    pthread_mutex_lock(&mtx);
    waiting_validation = 1;
    validation_ready = 0;
    mq_send(mq_srv, (char *)&req, sizeof(req), 0);

    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += 2;
    while (!validation_ready)
    {
        rc = pthread_cond_timedwait(&cond, &mtx, &ts);
        if (rc == ETIMEDOUT)
        {
            waiting_validation = 0;
            pthread_mutex_unlock(&mtx);
            modo_isolado = 1;
            return 0;
        }
    }

    waiting_validation = 0;
    rc = validation_ok;
    pthread_mutex_unlock(&mtx);
    return rc;
}

int main(int argc, char **argv)
{
    pthread_t t;
    ctl_msg_t msg;
    struct mq_attr attr;
    int i;
    char pin[NDIG + 1];

    if (argc < 2)
        return 1;

    p_id = atoi(argv[1]);
    if (p_id < 1 || p_id > NPOR)
        return 1;

    for (i = 0; i < CMAX; i++)
        cache_valid[i] = 0;

    mq_srv = (mqd_t)-1;
    mq_self = (mqd_t)-1;
    self_q_name[0] = '\0';
    lock_path[0] = '\0';

    atexit(cleanup_ctl);
    signal(SIGINT, encerra_ctl);
    signal(SIGTERM, encerra_ctl);

    if (create_controller_lock() != 0)
    {
        fprintf(stderr, "Erro: ja existe controlador para porta %d\n", p_id);
        return 1;
    }

    snprintf(self_q_name, sizeof(self_q_name), "/CTLP%d", p_id);
    memset(&attr, 0, sizeof(attr));
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(ctl_msg_t);
    mq_unlink(self_q_name);
    mq_self = mq_open(self_q_name, O_RDONLY | O_CREAT, 0666, &attr);
    if (mq_self == (mqd_t)-1)
    {
        perror("mq_open self");
        return 1;
    }

    mq_srv = mq_open(SERVQ, O_WRONLY);
    if (mq_srv == (mqd_t)-1)
    {
        modo_isolado = 1;
        printf("Aviso: modo isolado (sem servidor)\n");
    }
    else
    {
        memset(&msg, 0, sizeof(msg));
        msg.type = 'O';
        msg.porta = (char)p_id;
        mq_send(mq_srv, (char *)&msg, sizeof(msg), 0);
    }

    pthread_create(&t, NULL, thread_servidor, NULL);

    while (running)
    {
        int aceite = 0;

        printf("Porta %d - PIN: ", p_id);
        if (scanf("%4s", pin) != 1)
            break;

        if (strcmp(pin, CODX) == 0)
        {
            aceite = 1;
        }
        else if (modo_porta == 'A')
        {
            aceite = 1;
        }
        else if (modo_porta == 'F')
        {
            aceite = 0;
        }
        else
        {
            if (cache_find(pin))
                aceite = 1;
            else
            {
                aceite = valida_remota(pin);
                if (aceite)
                    cache_add(pin);
            }
        }

        if (aceite)
        {
            printf("Aberto\n");
            if (modo_porta == 'N')
            {
                sleep(TMAX);
                printf("Fechado\n");
            }

            if (!modo_isolado && mq_srv != (mqd_t)-1)
            {
                memset(&msg, 0, sizeof(msg));
                msg.type = 'R';
                msg.porta = (char)p_id;
                strncpy(msg.codigo, pin, NDIG);
                clock_gettime(CLOCK_REALTIME, &msg.t);
                mq_send(mq_srv, (char *)&msg, sizeof(msg), 0);
            }
        }
        else
        {
            printf("Acesso negado\n");
        }
    }

    running = 0;

    if (!modo_isolado && mq_srv != (mqd_t)-1)
    {
        memset(&msg, 0, sizeof(msg));
        msg.type = 'X';
        msg.porta = (char)p_id;
        mq_send(mq_srv, (char *)&msg, sizeof(msg), 0);
        mq_close(mq_srv);
    }

    return 0;
}
