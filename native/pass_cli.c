#include "pass_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef PASS_VERSION
#define PASS_VERSION "dev"
#endif

#define MASTER_ENV "LESSPASS_MASTER_PASSWORD"

typedef struct {
    PassOptions options;
    int prompt;
    int copy;
    int no_copy;
    int show_version;
    const char *site;
    const char *login;
    const char *master;
} Cli;

static void
usage(FILE *out)
{
    fputs(
        "Usage: pass [OPTIONS] SITE LOGIN [MASTER_PASSWORD]\n"
        "\n"
        "Derive a site-specific password.\n"
        "\n"
        "Options:\n"
        "  -L, --length N          password length (default 16, min 5, max 35)\n"
        "  -C, --counter N         generation counter (default 1)\n"
        "  -l, --lowercase         include lowercase letters\n"
        "  -u, --uppercase         include uppercase letters\n"
        "  -d, --digits            include digits\n"
        "  -s, --symbols           include symbols\n"
        "      --no-lowercase      exclude lowercase letters\n"
        "      --no-uppercase      exclude uppercase letters\n"
        "      --no-digits         exclude digits\n"
        "      --no-symbols        exclude symbols\n"
        "      --exclude CHARS     remove characters from all enabled classes\n"
        "  -p, --prompt            read the master password from stdin\n"
        "  -c, --copy              reserved for GUI/runtime clipboard support\n"
        "      --no-copy           print instead of copying\n"
        "  -v, --version           show version\n"
        "  -h, --help              show this help\n",
        out);
}

static int
parse_int(const char *value, int *out)
{
    char *end = NULL;
    long parsed;

    if(value == NULL || value[0] == '\0')
        return 0;
    parsed = strtol(value, &end, 10);
    if(end == NULL || *end != '\0')
        return 0;
    *out = (int)parsed;
    return 1;
}

static int
read_line(char *out, size_t out_size)
{
    size_t len;

    if(out == NULL || out_size == 0)
        return 0;
    if(fgets(out, (int)out_size, stdin) == NULL)
        return 0;
    len = strlen(out);
    while(len > 0 && (out[len - 1] == '\n' || out[len - 1] == '\r')) {
        out[len - 1] = '\0';
        len--;
    }
    return 1;
}

static int
take_value(int argc, char **argv, int *i, const char **value, const char *name)
{
    if(*i + 1 >= argc) {
        fprintf(stderr, "pass: %s requires a value\n", name);
        return 0;
    }
    *i += 1;
    *value = argv[*i];
    return 1;
}

static int
parse_args(int argc, char **argv, Cli *cli)
{
    int positional = 0;

    for(int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        const char *value = NULL;

        if(strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            usage(stdout);
            exit(0);
        } else if(strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0) {
            cli->show_version = 1;
        } else if(strcmp(arg, "-L") == 0 || strcmp(arg, "--length") == 0) {
            if(!take_value(argc, argv, &i, &value, arg) ||
               !parse_int(value, &cli->options.length))
                return 0;
        } else if(strcmp(arg, "-C") == 0 || strcmp(arg, "--counter") == 0) {
            int counter;

            if(!take_value(argc, argv, &i, &value, arg) ||
               !parse_int(value, &counter))
                return 0;
            cli->options.counter = (unsigned long long)(counter < 0 ? 0 : counter);
        } else if(strcmp(arg, "-l") == 0 || strcmp(arg, "--lowercase") == 0) {
            cli->options.lowercase = 1;
        } else if(strcmp(arg, "-u") == 0 || strcmp(arg, "--uppercase") == 0) {
            cli->options.uppercase = 1;
        } else if(strcmp(arg, "-d") == 0 || strcmp(arg, "--digits") == 0) {
            cli->options.digits = 1;
        } else if(strcmp(arg, "-s") == 0 || strcmp(arg, "--symbols") == 0) {
            cli->options.symbols = 1;
        } else if(strcmp(arg, "--no-lowercase") == 0) {
            cli->options.lowercase = 0;
        } else if(strcmp(arg, "--no-uppercase") == 0) {
            cli->options.uppercase = 0;
        } else if(strcmp(arg, "--no-digits") == 0) {
            cli->options.digits = 0;
        } else if(strcmp(arg, "--no-symbols") == 0) {
            cli->options.symbols = 0;
        } else if(strcmp(arg, "--exclude") == 0) {
            if(!take_value(argc, argv, &i, &value, arg))
                return 0;
            cli->options.exclude = value;
        } else if(strcmp(arg, "-p") == 0 || strcmp(arg, "--prompt") == 0) {
            cli->prompt = 1;
        } else if(strcmp(arg, "-c") == 0 || strcmp(arg, "--copy") == 0) {
            cli->copy = 1;
        } else if(strcmp(arg, "--no-copy") == 0) {
            cli->no_copy = 1;
        } else if(arg[0] == '-') {
            fprintf(stderr, "pass: unknown option %s\n", arg);
            return 0;
        } else if(positional == 0) {
            cli->site = arg;
            positional++;
        } else if(positional == 1) {
            cli->login = arg;
            positional++;
        } else if(positional == 2) {
            cli->master = arg;
            positional++;
        } else {
            fprintf(stderr, "pass: too many positional arguments\n");
            return 0;
        }
    }
    return 1;
}

int
main(int argc, char **argv)
{
    Cli cli;
    char master[1024];
    char out[256];
    char err[256];
    const char *env_master;

    memset(&cli, 0, sizeof(cli));
    cli.options.length = 16;
    cli.options.counter = 1;
    cli.options.lowercase = 1;
    cli.options.uppercase = 1;
    cli.options.digits = 1;
    cli.options.symbols = 1;
    cli.options.exclude = "";

    if(!parse_args(argc, argv, &cli)) {
        usage(stderr);
        return 2;
    }
    if(cli.show_version) {
        printf("pass %s\n", PASS_VERSION);
        return 0;
    }
    if(cli.site == NULL || cli.login == NULL) {
        usage(stderr);
        return 2;
    }
    if(cli.copy && !cli.no_copy) {
        fprintf(stderr, "pass: --copy is not available in the C CLI yet\n");
        return 2;
    }

    memset(master, 0, sizeof(master));
    if(cli.master != NULL) {
        snprintf(master, sizeof(master), "%s", cli.master);
    } else if(!cli.prompt && (env_master = getenv(MASTER_ENV)) != NULL) {
        snprintf(master, sizeof(master), "%s", env_master);
    } else {
        if(!read_line(master, sizeof(master))) {
            fprintf(stderr, "pass: missing master password\n");
            return 2;
        }
    }

    memset(out, 0, sizeof(out));
    memset(err, 0, sizeof(err));
    if(pass_core_generate(cli.site, cli.login, master, &cli.options,
                          out, sizeof(out), err, sizeof(err)) != 0) {
        fprintf(stderr, "pass: %s\n", err);
        return 1;
    }
    printf("%s\n", out);
    memset(master, 0, sizeof(master));
    memset(out, 0, sizeof(out));
    return 0;
}
