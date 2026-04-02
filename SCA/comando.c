#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#include "comum.h"

static int send_req(const ges_req_t *req, ges_resp_t *resp)
{
    int sd;
    struct sockaddr_un srv_addr;
    struct sockaddr_un my_addr;
    struct timeval tv;
    socklen_t len;

    sd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sd < 0)
    {
        perror("socket");
        return -1;
    }

    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sun_family = AF_UNIX;
    snprintf(my_addr.sun_path, sizeof(my_addr.sun_path), "/tmp/SCAges_%d", getpid());
    unlink(my_addr.sun_path);
    if (bind(sd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0)
    {
        perror("bind");
        close(sd);
        return -1;
    }

    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sun_family = AF_UNIX;
    strncpy(srv_addr.sun_path, SERVS, sizeof(srv_addr.sun_path) - 1);

    if (sendto(sd, req, sizeof(*req), 0, (struct sockaddr *)&srv_addr, sizeof(srv_addr)) < 0)
    {
        perror("sendto");
        close(sd);
        unlink(my_addr.sun_path);
        return -1;
    }

    len = sizeof(srv_addr);
    if (recvfrom(sd, resp, sizeof(*resp), 0, (struct sockaddr *)&srv_addr, &len) < 0)
    {
        printf("Erro: timeout/comunicacao com servidor\n");
        close(sd);
        unlink(my_addr.sun_path);
        return -1;
    }

    close(sd);
    unlink(my_addr.sun_path);
    return 0;
}

static void print_resp_msg(const ges_resp_t *resp)
{
    if (resp->ok)
        printf("OK: %s\n", resp->msg);
    else
        printf("ERRO: %s\n", resp->msg);
}

void cmd_sair(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    exit(0);
}

void cmd_nuti(int argc, char **argv)
{
    ges_req_t req;
    ges_resp_t resp;

    if (argc < 4)
        return;

    memset(&req, 0, sizeof(req));
    req.cmd = GES_NUTI;
    req.n = atoi(argv[1]);
    strncpy(req.nome, argv[2], MAXN - 1);
    strncpy(req.codigo, argv[3], NDIG);

    if (send_req(&req, &resp) == 0)
        print_resp_msg(&resp);
}

void cmd_luti(int argc, char **argv)
{
    int i;
    ges_req_t req;
    ges_resp_t resp;

    (void)argc;
    (void)argv;

    memset(&req, 0, sizeof(req));
    req.cmd = GES_LUTI;

    if (send_req(&req, &resp) != 0)
        return;
    if (!resp.ok)
    {
        print_resp_msg(&resp);
        return;
    }

    for (i = 0; i < resp.count; i++)
    {
        printf("ID:%d Nome:%s Cod:%s Portas:%d%d%d\n",
               resp.lista[i].num,
               resp.lista[i].nome,
               resp.lista[i].codigo,
               resp.lista[i].port[0],
               resp.lista[i].port[1],
               resp.lista[i].port[2]);
    }
}

void cmd_euti(int argc, char **argv)
{
    ges_req_t req;
    ges_resp_t resp;
    if (argc < 2)
        return;

    memset(&req, 0, sizeof(req));
    req.cmd = GES_EUTI;
    req.n = atoi(argv[1]);
    if (send_req(&req, &resp) == 0)
        print_resp_msg(&resp);
}

void cmd_mcu(int argc, char **argv)
{
    ges_req_t req;
    ges_resp_t resp;
    if (argc < 3)
        return;

    memset(&req, 0, sizeof(req));
    req.cmd = GES_MCU;
    req.n = atoi(argv[1]);
    strncpy(req.codigo, argv[2], NDIG);
    if (send_req(&req, &resp) == 0)
        print_resp_msg(&resp);
}

void cmd_apu(int argc, char **argv)
{
    ges_req_t req;
    ges_resp_t resp;

    if (argc < 3)
        return;

    memset(&req, 0, sizeof(req));
    req.cmd = GES_APU;
    req.n = atoi(argv[1]);
    strncpy(req.portas, argv[2], NPOR);
    if (send_req(&req, &resp) == 0)
        print_resp_msg(&resp);
}

void cmd_rpu(int argc, char **argv)
{
    ges_req_t req;
    ges_resp_t resp;

    if (argc < 3)
        return;

    memset(&req, 0, sizeof(req));
    req.cmd = GES_RPU;
    req.n = atoi(argv[1]);
    strncpy(req.portas, argv[2], NPOR);
    if (send_req(&req, &resp) == 0)
        print_resp_msg(&resp);
}

void cmd_lapu(int argc, char **argv)
{
    int i;
    ges_req_t req;
    ges_resp_t resp;

    if (argc < 2)
        return;

    memset(&req, 0, sizeof(req));
    req.cmd = GES_LAPU;
    req.n = atoi(argv[1]);
    if (argc >= 3)
        strncpy(req.t1, argv[2], sizeof(req.t1) - 1);
    if (argc >= 4)
        strncpy(req.t2, argv[3], sizeof(req.t2) - 1);

    if (send_req(&req, &resp) != 0)
        return;
    if (!resp.ok)
    {
        print_resp_msg(&resp);
        return;
    }
    for (i = 0; i < resp.count; i++)
    {
        printf("%ld.%09ld porta=%d codigo=%s\n",
               (long)resp.regs[i].t.tv_sec,
               resp.regs[i].t.tv_nsec,
               (int)resp.regs[i].p,
               resp.regs[i].id);
    }
}

void cmd_tser(int argc, char **argv)
{
    ges_req_t req;
    ges_resp_t resp;
    (void)argc;
    (void)argv;
    memset(&req, 0, sizeof(req));
    req.cmd = GES_TSER;
    if (send_req(&req, &resp) == 0)
        print_resp_msg(&resp);
}

void cmd_cep(int argc, char **argv)
{
    ges_req_t req;
    ges_resp_t resp;
    if (argc < 2)
        return;
    memset(&req, 0, sizeof(req));
    req.cmd = GES_CEP;
    req.p = atoi(argv[1]);
    if (send_req(&req, &resp) == 0)
        print_resp_msg(&resp);
}

void cmd_mep(int argc, char **argv)
{
    ges_req_t req;
    ges_resp_t resp;
    if (argc < 3)
        return;
    memset(&req, 0, sizeof(req));
    req.cmd = GES_MEP;
    req.p = atoi(argv[1]);
    req.e = argv[2][0];
    if (send_req(&req, &resp) == 0)
        print_resp_msg(&resp);
}

void cmd_acp(int argc, char **argv)
{
    ges_req_t req;
    ges_resp_t resp;
    if (argc < 2)
        return;
    memset(&req, 0, sizeof(req));
    req.cmd = GES_ACP;
    req.p = atoi(argv[1]);
    if (send_req(&req, &resp) == 0)
        print_resp_msg(&resp);
}

void cmd_tctl(int argc, char **argv)
{
    ges_req_t req;
    ges_resp_t resp;
    if (argc < 2)
        return;
    memset(&req, 0, sizeof(req));
    req.cmd = GES_TCTL;
    req.p = atoi(argv[1]);
    if (send_req(&req, &resp) == 0)
        print_resp_msg(&resp);
}
