#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Declaracao das funcoes que estao no comando.c */
extern void cmd_sair(int, char **);
extern void cmd_nuti(int, char **);
extern void cmd_luti(int, char **);
extern void cmd_euti(int, char **);
extern void cmd_mcu(int, char **);
extern void cmd_apu(int, char **);
extern void cmd_rpu(int, char **);
extern void cmd_lapu(int, char **);
extern void cmd_tser(int, char **);
extern void cmd_cep(int, char **);
extern void cmd_mep(int, char **);
extern void cmd_acp(int, char **);
extern void cmd_tctl(int, char **);
void cmd_sos(int, char **);

struct command_d
{
    void (*cmd_fnct)(int, char **);
    char *cmd_name;
    char *cmd_help;
} const commands[] = {
    {cmd_sos, "sos", "ajuda"},
    {cmd_sair, "sair", "sair do programa"},
    {cmd_nuti, "nuti", "<num> <nome> <cod> - novo utilizador"},
    {cmd_luti, "luti", "- listar utilizadores"},
    {cmd_euti, "euti", "<num> - eliminar utilizador"},
    {cmd_mcu, "mcu", "<num> <codigo> - alterar codigo do utilizador"},
    {cmd_apu, "apu", "<num> <portas> - adicionar portas"},
    {cmd_rpu, "rpu", "<num> <portas> - remover portas"},
    {cmd_lapu, "lapu", "<num> [t1] [t2] - listar acessos do utilizador"},
    {cmd_tser, "tser", "- terminar servidor"},
    {cmd_cep, "cep", "<p> - consultar estado da porta"},
    {cmd_mep, "mep", "<p> <N|A|F> - alterar estado da porta"},
    {cmd_acp, "acp", "<p> - apagar cache da porta"},
    {cmd_tctl, "tctl", "<p> - terminar controlador da porta"},
    {cmd_sos, "help", "mostrar comandos disponiveis"}};

#define NCOMMANDS (sizeof(commands) / sizeof(struct command_d))
#define ARGVECSIZE 10
#define MAX_LINE 100

void cmd_sos(int argc, char **argv)
{
    int i;
    (void)argc;
    (void)argv;
    for (i = 0; i < NCOMMANDS; i++)
        printf("%s %s\n", commands[i].cmd_name, commands[i].cmd_help);
}

int my_getline(char **argv, int argvsize)
{
    static char line[MAX_LINE];
    char *p;
    int argc;

    if (fgets(line, MAX_LINE, stdin) == NULL)
        return 0;

    for (argc = 0, p = line; (*line != '\0') && (argc < argvsize); p = NULL, argc++)
    {
        p = strtok(p, " \t\n");
        argv[argc] = p;
        if (p == NULL)
            return argc;
    }

    argv[argc] = p;
    return argc;
}

void monitor(void)
{
    static char *argv[ARGVECSIZE + 1], *p;
    int argc, i;

    printf("\n SCA - Gestao (Escreve sos para ajuda)\n");
    for (;;)
    {
        printf("SCA-Gestao> ");
        if ((argc = my_getline(argv, ARGVECSIZE)) > 0)
        {
            for (p = argv[0]; *p != '\0'; *p = tolower(*p), p++)
            {
            }
            for (i = 0; i < NCOMMANDS; i++)
            {
                if (strcmp(argv[0], commands[i].cmd_name) == 0)
                    break;
            }
            if (i < NCOMMANDS)
            {
                commands[i].cmd_fnct(argc, argv);
            }
            else
            {
                printf("Comando Invalido!\n");
            }
        }
    }
}
