#include <fcntl.h>
#include <mqueue.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "comum.h"

typedef struct cache_s
{
    int valid;
    char codigo[NDIG + 1];
    int idx_uti;
} cache_t;

uti_t *utilizadores = MAP_FAILED;
reg_t *historico = MAP_FAILED;
int fd_futi = -1, fd_flog = -1, sd_sock = -1;
mqd_t mq_srv = (mqd_t)-1;
pthread_mutex_t mux = PTHREAD_MUTEX_INITIALIZER;
cache_t cache[CMAX];
int cache_pos = 0;
int running = 1;
int ctl_online[NPOR] = {0};
char modo_porta[NPOR] = {'N', 'N', 'N'};
int log_idx = 0;

static int parse_ts(const char *s, struct timespec *out)
{
    int d, m, y, hh, mm, ss;
    struct tm tmv;
    time_t t;

    if (s[0] == '\0')
        return 0;

    memset(&tmv, 0, sizeof(tmv));
    if (sscanf(s, "%d/%d/%d_%d:%d:%d", &d, &m, &y, &hh, &mm, &ss) == 6)
    {
        tmv.tm_mday = d;
        tmv.tm_mon = m - 1;
        tmv.tm_year = y - 1900;
        tmv.tm_hour = hh;
        tmv.tm_min = mm;
        tmv.tm_sec = ss;
        t = mktime(&tmv);
        if (t == (time_t)-1)
            return -1;
        out->tv_sec = t;
        out->tv_nsec = 0;
        return 1;
    }
    return -1;
}

static int find_user_by_num(int n)
{
    int i;
    for (i = 0; i < UMAX; i++)
    {
        if (utilizadores[i].num == n)
            return i;
    }
    return -1;
}

int extrai_porta(char p)
{
    if (p >= '1' && p <= ('0' + NPOR))
        return p - '0';
    if (p >= 1 && p <= NPOR)
        return (int)p;
    return 0;
}

void envia_ctrl_msg(int porta, const char *msg)
{
    (void)porta;
    (void)msg;
}

int cache_lookup(const char *codigo)
{
    int i;
    for (i = 0; i < CMAX; i++)
    {
        if (cache[i].valid && strcmp(cache[i].codigo, codigo) == 0)
            return cache[i].idx_uti;
    }
    return -1;
}

void cache_store(const char *codigo, int idx_uti)
{
    cache[cache_pos].valid = 1;
    strncpy(cache[cache_pos].codigo, codigo, NDIG);
    cache[cache_pos].codigo[NDIG] = '\0';
    cache[cache_pos].idx_uti = idx_uti;
    cache_pos = (cache_pos + 1) % CMAX;
}

int find_user_by_code_locked(const char *codigo)
{
    int i;
    int idx = cache_lookup(codigo);

    if (idx >= 0 && idx < UMAX && utilizadores[idx].num != 0 && strcmp(utilizadores[idx].codigo, codigo) == 0)
        return idx;

    for (i = 0; i < UMAX; i++)
    {
        if (utilizadores[i].num != 0 && strcmp(utilizadores[i].codigo, codigo) == 0)
        {
            cache_store(codigo, i);
            return i;
        }
    }

    return -1;
}

static void send_ctl_cmd(int p, char type, char estado, int ok, const char *codigo)
{
    char q_name[20];
    mqd_t q;
    ctl_msg_t msg;

    if (p < 1 || p > NPOR)
        return;

    memset(&msg, 0, sizeof(msg));
    msg.type = type;
    msg.porta = (char)p;
    msg.estado = estado;
    msg.ok = ok;
    if (codigo != NULL)
        strncpy(msg.codigo, codigo, NDIG);

    snprintf(q_name, sizeof(q_name), "/CTLP%d", p);
    q = mq_open(q_name, O_WRONLY);
    if (q == (mqd_t)-1)
        return;
    mq_send(q, (char *)&msg, sizeof(msg), 0);
    mq_close(q);
}

static void resp_ok(ges_resp_t *resp, const char *m)
{
    resp->ok = 1;
    strncpy(resp->msg, m, sizeof(resp->msg) - 1);
}

static void resp_err(ges_resp_t *resp, const char *m)
{
    resp->ok = 0;
    strncpy(resp->msg, m, sizeof(resp->msg) - 1);
}

void *thread_gestao(void *arg)
{
    struct sockaddr_un client_addr;
    socklen_t len = sizeof(client_addr);
    ges_req_t req;
    ges_resp_t resp;

    (void)arg;

    while (running)
    {
        int i;
        int n = recvfrom(sd_sock, &req, sizeof(req), 0, (struct sockaddr *)&client_addr, &len);
        if (n < (int)sizeof(req))
            continue;

        memset(&resp, 0, sizeof(resp));
        pthread_mutex_lock(&mux);
        if (req.cmd == GES_NUTI)
        {
            int idx = find_user_by_num(req.n);
            if (idx < 0)
            {
                idx = find_user_by_num(0);
                if (idx < 0)
                {
                    resp_err(&resp, "sem espaco UMAX");
                    goto send_resp;
                }
            }
            memset(&utilizadores[idx], 0, sizeof(uti_t));
            utilizadores[idx].num = req.n;
            strncpy(utilizadores[idx].nome, req.nome, MAXN - 1);
            strncpy(utilizadores[idx].codigo, req.codigo, NDIG);
            resp_ok(&resp, "utilizador criado");
        }
        else if (req.cmd == GES_LUTI)
        {
            for (i = 0; i < UMAX; i++)
            {
                if (utilizadores[i].num == 0)
                    continue;
                resp.lista[resp.count++] = utilizadores[i];
            }
            resp_ok(&resp, "lista utilizadores");
        }
        else if (req.cmd == GES_EUTI)
        {
            int idx = find_user_by_num(req.n);
            if (idx < 0)
                resp_err(&resp, "utilizador inexistente");
            else
            {
                memset(&utilizadores[idx], 0, sizeof(uti_t));
                resp_ok(&resp, "utilizador eliminado");
            }
        }
        else if (req.cmd == GES_MCU)
        {
            int idx = find_user_by_num(req.n);
            if (idx < 0)
                resp_err(&resp, "utilizador inexistente");
            else
            {
                strncpy(utilizadores[idx].codigo, req.codigo, NDIG);
                cache_store(req.codigo, idx);
                resp_ok(&resp, "codigo alterado");
            }
        }
        else if (req.cmd == GES_APU || req.cmd == GES_RPU)
        {
            int idx = find_user_by_num(req.n);
            if (idx < 0)
                resp_err(&resp, "utilizador inexistente");
            else
            {
                int j;
                for (j = 0; req.portas[j] != '\0'; j++)
                {
                    int k = req.portas[j] - '1';
                    if (k >= 0 && k < NPOR)
                        utilizadores[idx].port[k] = (req.cmd == GES_APU) ? 1 : 0;
                }
                resp_ok(&resp, (req.cmd == GES_APU) ? "portas adicionadas" : "portas removidas");
            }
        }
        else if (req.cmd == GES_LAPU)
        {
            struct timespec t1, t2;
            int has_t1 = parse_ts(req.t1, &t1);
            int has_t2 = parse_ts(req.t2, &t2);
            int idx = find_user_by_num(req.n);
            if (idx < 0)
            {
                resp_err(&resp, "utilizador inexistente");
            }
            else
            {
                for (i = 0; i < NREG; i++)
                {
                    if (historico[i].t.tv_sec == 0)
                        continue;
                    if (strcmp(historico[i].id, utilizadores[idx].codigo) != 0)
                        continue;
                    if (has_t1 > 0 && historico[i].t.tv_sec < t1.tv_sec)
                        continue;
                    if (has_t2 > 0 && historico[i].t.tv_sec > t2.tv_sec)
                        continue;
                    if (resp.count < NREG)
                        resp.regs[resp.count++] = historico[i];
                }
                resp_ok(&resp, "lista acessos");
            }
        }
        else if (req.cmd == GES_CEP)
        {
            if (req.p < 0 || req.p > NPOR)
                resp_err(&resp, "porta invalida");
            else if (req.p == 0)
            {
                snprintf(resp.msg, sizeof(resp.msg), "P1=%c P2=%c P3=%c", modo_porta[0], modo_porta[1], modo_porta[2]);
                resp.ok = 1;
            }
            else
            {
                resp.ok = 1;
                resp.p = req.p;
                resp.e = modo_porta[req.p - 1];
                snprintf(resp.msg, sizeof(resp.msg), "porta %d = %c", req.p, resp.e);
            }
        }
        else if (req.cmd == GES_MEP)
        {
            if (req.p < 0 || req.p > NPOR || (req.e != 'N' && req.e != 'A' && req.e != 'F'))
                resp_err(&resp, "argumentos invalidos");
            else
            {
                if (req.p == 0)
                {
                    for (i = 1; i <= NPOR; i++)
                    {
                        modo_porta[i - 1] = req.e;
                        send_ctl_cmd(i, 'M', req.e, 1, NULL);
                    }
                }
                else
                {
                    modo_porta[req.p - 1] = req.e;
                    send_ctl_cmd(req.p, 'M', req.e, 1, NULL);
                }
                resp_ok(&resp, "modo atualizado");
            }
        }
        else if (req.cmd == GES_ACP)
        {
            if (req.p < 0 || req.p > NPOR)
                resp_err(&resp, "porta invalida");
            else
            {
                if (req.p == 0)
                    for (i = 1; i <= NPOR; i++)
                        send_ctl_cmd(i, 'C', 0, 1, NULL);
                else
                    send_ctl_cmd(req.p, 'C', 0, 1, NULL);
                resp_ok(&resp, "cache limpa");
            }
        }
        else if (req.cmd == GES_TCTL)
        {
            if (req.p < 0 || req.p > NPOR)
                resp_err(&resp, "porta invalida");
            else
            {
                if (req.p == 0)
                    for (i = 1; i <= NPOR; i++)
                        send_ctl_cmd(i, 'T', 0, 1, NULL);
                else
                    send_ctl_cmd(req.p, 'T', 0, 1, NULL);
                resp_ok(&resp, "termino controlador pedido");
            }
        }
        else if (req.cmd == GES_TSER)
        {
            resp_ok(&resp, "termino servidor");
            running = 0;
        }
        else
        {
            resp_err(&resp, "comando desconhecido");
        }

    send_resp:
        pthread_mutex_unlock(&mux);
        sendto(sd_sock, &resp, sizeof(resp), 0, (struct sockaddr *)&client_addr, len);

        if (!running)
            break;
    }

    return NULL;
}

void *thread_controladores(void *arg)
{
    ctl_msg_t msg;

    (void)arg;

    while (running)
    {
        if (mq_receive(mq_srv, (char *)&msg, sizeof(msg), NULL) > 0)
        {
            int idx;
            int porta = extrai_porta(msg.porta);

            pthread_mutex_lock(&mux);
            if (msg.type == 'R')
            {
                historico[log_idx].t = msg.t;
                historico[log_idx].p = msg.porta;
                strncpy(historico[log_idx].id, msg.codigo, NDIG);
                historico[log_idx].id[NDIG] = '\0';
                log_idx = (log_idx + 1) % NREG;
            }
            else if (msg.type == 'V')
            {
                int ok = 0;
                idx = find_user_by_code_locked(msg.codigo);
                if (porta >= 1 && porta <= NPOR && idx >= 0 && utilizadores[idx].port[porta - 1])
                    ok = 1;
                pthread_mutex_unlock(&mux);
                send_ctl_cmd(porta, 'v', 0, ok, msg.codigo);
                continue;
            }
            else if (msg.type == 'O')
            {
                if (porta >= 1 && porta <= NPOR)
                    ctl_online[porta - 1] = 1;
            }
            else if (msg.type == 'X')
            {
                if (porta >= 1 && porta <= NPOR)
                    ctl_online[porta - 1] = 0;
            }
            pthread_mutex_unlock(&mux);
        }
    }

    return NULL;
}

void encerra(int sn)
{
    (void)sn;
    running = 0;
    if (utilizadores != MAP_FAILED)
        munmap(utilizadores, UMAX * sizeof(uti_t));
    if (historico != MAP_FAILED)
        munmap(historico, NREG * sizeof(reg_t));
    if (fd_futi >= 0)
        close(fd_futi);
    if (fd_flog >= 0)
        close(fd_flog);
    if (sd_sock >= 0)
        close(sd_sock);
    if (mq_srv != (mqd_t)-1)
        mq_close(mq_srv);
    unlink(SERVS);
    mq_unlink(SERVQ);
    exit(0);
}

int main(void)
{
    pthread_t t1, t2;
    int i;
    struct sockaddr_un srv_addr;
    struct mq_attr attr;

    signal(SIGTERM, encerra);
    signal(SIGINT, encerra);

    for (i = 0; i < CMAX; i++)
        cache[i].valid = 0;

    fd_futi = open(FUTI, O_RDWR | O_CREAT, 0666);
    if (fd_futi < 0)
    {
        perror("open FUTI");
        return 1;
    }
    if (ftruncate(fd_futi, UMAX * sizeof(uti_t)) < 0)
    {
        perror("ftruncate FUTI");
        encerra(0);
    }
    utilizadores = mmap(NULL, UMAX * sizeof(uti_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd_futi, 0);
    if (utilizadores == MAP_FAILED)
    {
        perror("mmap FUTI");
        encerra(0);
    }

    fd_flog = open(FLOG, O_RDWR | O_CREAT, 0666);
    if (fd_flog < 0)
    {
        perror("open FLOG");
        encerra(0);
    }
    if (ftruncate(fd_flog, NREG * sizeof(reg_t)) < 0)
    {
        perror("ftruncate FLOG");
        encerra(0);
    }
    historico = mmap(NULL, NREG * sizeof(reg_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd_flog, 0);
    if (historico == MAP_FAILED)
    {
        perror("mmap FLOG");
        encerra(0);
    }

    sd_sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sd_sock < 0)
    {
        perror("socket");
        encerra(0);
    }
    unlink(SERVS);
    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sun_family = AF_UNIX;
    strncpy(srv_addr.sun_path, SERVS, sizeof(srv_addr.sun_path) - 1);
    if (bind(sd_sock, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0)
    {
        perror("bind");
        encerra(0);
    }

    memset(&attr, 0, sizeof(attr));
    attr.mq_maxmsg = 10;
    attr.mq_msgsize = sizeof(ctl_msg_t);
    mq_unlink(SERVQ);
    mq_srv = mq_open(SERVQ, O_RDONLY | O_CREAT, 0666, &attr);
    if (mq_srv == (mqd_t)-1)
    {
        perror("mq_open SERVQ");
        encerra(0);
    }

    if (pthread_create(&t1, NULL, thread_gestao, NULL) != 0)
    {
        perror("pthread_create gestao");
        encerra(0);
    }
    if (pthread_create(&t2, NULL, thread_controladores, NULL) != 0)
    {
        perror("pthread_create controladores");
        encerra(0);
    }

    printf("Servidor SCA pronto\n");
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    encerra(0);
    return 0;
}
