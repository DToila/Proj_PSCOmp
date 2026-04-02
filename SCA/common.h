#ifndef COMMON_H
#define COMMON_H

#include <time.h>

#define NDIG 4             /* numero de digitos do codigo de acesso */
#define CODX "9999"        /* codigo especial de acesso */
#define MAXN 40            /* dimensao maxima do nome do utilizador */
#define CMAX 5             /* dimensao da cache de utilizadores */
#define UMAX 20            /* numero maximo de utilizadores */
#define FUTI "USERS.DAT"   /* ficheiro relativo aos utilizadores */
#define NREG 150           /* numero maximo de registos no ficheiro log */
#define FLOG "ACESSOS.LOG" /* ficheiro com historico dos acessos */
#define SERVS "/tmp/SERV"  /* nome do servidor (socket) */
#define SERVQ "/SERV"      /* nome do servidor (queue) */
#define NPOR 3             /* numero de portas a controlar */
#define TMAX 5             /* tempo para fecho automatico porta */

/* Nomes das Queues para os Controladores */
#define CTLP1 "/CTLP1"
#define CTLP2 "/CTLP2"
#define CTLP3 "/CTLP3"

typedef struct uti_s
{
    int num;
    char nome[MAXN];
    char codigo[NDIG + 1];
    unsigned char port[NPOR];
} uti_t;

typedef struct reg_s
{
    struct timespec t;
    char p;            /* ID da porta: 1, 2 ou 3 */
    char id[NDIG + 1]; /* Codigo usado */
} reg_t;

typedef struct apu_msg_s
{
    char t;
    int n;
    char p[NPOR + 1];
} apu_msg_t;

typedef enum ges_cmd_e
{
    GES_NUTI = 1,
    GES_LUTI,
    GES_EUTI,
    GES_MCU,
    GES_APU,
    GES_RPU,
    GES_LAPU,
    GES_TSER,
    GES_CEP,
    GES_MEP,
    GES_ACP,
    GES_TCTL
} ges_cmd_t;

typedef struct ges_req_s
{
    int cmd;
    int n;
    int p;
    char e;
    char nome[MAXN];
    char codigo[NDIG + 1];
    char portas[NPOR + 1];
    char t1[20];
    char t2[20];
} ges_req_t;

typedef struct ges_resp_s
{
    int ok;
    int p;
    char e;
    char msg[128];
    int count;
    uti_t lista[UMAX];
    reg_t regs[NREG];
} ges_resp_t;

typedef struct ctl_msg_s
{
    char type;
    char porta;
    char estado;
    int ok;
    struct timespec t;
    char codigo[NDIG + 1];
} ctl_msg_t;

#endif