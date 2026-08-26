/* Host-buildable vector test for pass_core.c. The vectors lock the generator
 * output so CLI, desktop, Android, and web stay byte-compatible. */

#include "pass_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

static void
check_generate(int index, const char *site, const char *login, const char *master,
               PassOptions options, const char *want, int want_error)
{
    char out[256];
    char err[256];
    int result;

    memset(out, 0, sizeof(out));
    memset(err, 0, sizeof(err));
    result = pass_core_generate(site, login, master, &options, out, sizeof(out), err, sizeof(err));

    if(want_error) {
        if(result == 0) {
            printf("FAIL %d: expected error, got password \"%s\"\n", index, out);
            failures++;
        } else if(err[0] == '\0') {
            printf("FAIL %d: error returned but message is empty\n", index);
            failures++;
        } else {
            printf("ok   %d: rejected (%s)\n", index, err);
        }
        return;
    }

    if(result != 0) {
        printf("FAIL %d: unexpected error: %s\n", index, err);
        failures++;
    } else if(strcmp(out, want) != 0) {
        printf("FAIL %d: got \"%s\", want \"%s\"\n", index, out, want);
        failures++;
    } else {
        printf("ok   %d: \"%s\"\n", index, out);
    }
}

static void
check_derive(int index, const char *password, const char *salt, const char *want_hex)
{
    uint8_t out[32];
    char hex[65];
    int i;

    pass_core_derive_key(password, strlen(password), salt, strlen(salt), out);
    for(i = 0; i < 32; i++)
        sprintf(hex + i * 2, "%02x", out[i]);
    hex[64] = '\0';

    if(strcmp(hex, want_hex) != 0) {
        printf("FAIL derive %d: got %s, want %s\n", index, hex, want_hex);
        failures++;
    } else {
        printf("ok   derive %d: %s\n", index, hex);
    }
}

static void
check_sha256(void)
{
    uint8_t out[32];
    char hex[65];
    int i;

    pass_core_sha256("correct horse battery staple", strlen("correct horse battery staple"), out);
    for(i = 0; i < 32; i++)
        sprintf(hex + i * 2, "%02x", out[i]);
    hex[64] = '\0';

    if(strcmp(hex, "c4bbcb1fbec99d65bf59d85c8cb62ee2db963f0fe106f483d9afa73bd4e39a8a") != 0) {
        printf("FAIL sha256: got %s\n", hex);
        failures++;
    } else {
        printf("ok   sha256: %s\n", hex);
    }
}

static int
utf8_codepoint_count(const char *text)
{
    int count = 0;

    if(text == NULL)
        return 0;
    for(int i = 0; text[i] != '\0'; i++) {
        unsigned char c = (unsigned char)text[i];
        if((c & 0xC0u) != 0x80u)
            count++;
    }
    return count;
}

static void
check_master_emoji(void)
{
    char first[64];
    char again[64];
    char empty[64];
    char values[5][64];
    int table_count = 0;
    const int *table = pass_core_master_emoji_codepoints(&table_count);

    if(table == NULL || table_count != 64) {
        printf("FAIL master emoji: table count %d, want 64\n", table_count);
        failures++;
        return;
    }

    pass_core_master_emoji("correct horse battery staple", first, sizeof(first));
    pass_core_master_emoji("correct horse battery staple", again, sizeof(again));
    if(strcmp(first, again) != 0 ||
       utf8_codepoint_count(first) != PASS_MASTER_EMOJI_COUNT) {
        printf("FAIL master emoji: deterministic=%d count=%d value=%s\n",
               strcmp(first, again) == 0,
               utf8_codepoint_count(first), first);
        failures++;
        return;
    }

    pass_core_master_emoji("", empty, sizeof(empty));
    if(utf8_codepoint_count(empty) != PASS_MASTER_EMOJI_COUNT) {
        printf("FAIL master emoji: empty count=%d value=%s\n",
               utf8_codepoint_count(empty), empty);
        failures++;
        return;
    }

    pass_core_master_emoji("test", values[0], sizeof(values[0]));
    pass_core_master_emoji("test2", values[1], sizeof(values[1]));
    pass_core_master_emoji("tset", values[2], sizeof(values[2]));
    pass_core_master_emoji("hunter2", values[3], sizeof(values[3]));
    pass_core_master_emoji("correct horse battery staple", values[4], sizeof(values[4]));
    for(int i = 0; i < 5; i++) {
        for(int j = i + 1; j < 5; j++) {
            if(strcmp(values[i], values[j]) == 0) {
                printf("FAIL master emoji: collision %d/%d = %s\n", i, j, values[i]);
                failures++;
                return;
            }
        }
    }

    printf("ok   master emoji: %s\n", first);
}

static void
check_shape(void)
{
    PassOptions options;
    char out[256];
    char err[256];
    static const char *exclude = "abcXYZ019!@";
    static const char *lower = "abcdefghijklmnopqrstuvwxyz";
    static const char *upper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    static const char *digits = "0123456789";
    static const char *symbols = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
    int i, j, has_lower, has_upper, has_digits, has_symbols;

    memset(&options, 0, sizeof(options));
    options.length = 35;
    options.counter = 3;
    options.lowercase = 1;
    options.uppercase = 1;
    options.digits = 1;
    options.symbols = 1;
    options.exclude = exclude;

    if(pass_core_generate("host", "account", "secret", &options, out, sizeof(out), err, sizeof(err)) != 0) {
        printf("FAIL shape: %s\n", err);
        failures++;
        return;
    }
    if((int)strlen(out) != 35) {
        printf("FAIL shape: length %zu, want 35\n", strlen(out));
        failures++;
        return;
    }
    for(i = 0; exclude[i] != '\0'; i++) {
        if(strchr(out, exclude[i]) != NULL) {
            printf("FAIL shape: contains excluded character '%c'\n", exclude[i]);
            failures++;
            return;
        }
    }
    has_lower = has_upper = has_digits = has_symbols = 0;
    for(i = 0; out[i] != '\0'; i++) {
        if(strchr(lower, out[i]))
            has_lower = 1;
        if(strchr(upper, out[i]))
            has_upper = 1;
        if(strchr(digits, out[i]))
            has_digits = 1;
        for(j = 0; symbols[j] != '\0'; j++) {
            if(out[i] == symbols[j])
                has_symbols = 1;
        }
    }
    if(!has_lower || !has_upper || !has_digits || !has_symbols) {
        printf("FAIL shape: class coverage lower=%d upper=%d digits=%d symbols=%d\n",
               has_lower, has_upper, has_digits, has_symbols);
        failures++;
        return;
    }
    printf("ok   shape: 35 chars, all classes covered, exclusions honored\n");
}

int
main(void)
{
    PassOptions defaults;

    check_derive(1, "password", "salt",
        "0394a2ede332c9a13eb82e9b24631604c31df978b4e2f0fbd2c549944f9d79a5");
    check_derive(2, "", "examplealice1",
        "caa46554f5a676b76c15b368c655b0b24eaaad8595ef919999785c68e60fd5f5");
    check_derive(3, "test", "sitelogina",
        "91b368e6337bd9c7007041922ade15c2907bd53c450f73e0ba1a001e10bff7eb");
    check_sha256();
    check_master_emoji();

    memset(&defaults, 0, sizeof(defaults));
    defaults.length = 16;
    defaults.counter = 1;
    defaults.lowercase = 1;
    defaults.uppercase = 1;
    defaults.digits = 1;
    defaults.symbols = 1;
    defaults.exclude = "";

    check_generate(1, "example.com", "alice", "correct horse battery staple",
                   defaults, "&Lf4'/-cSk4DPv_8", 0);

    {
        PassOptions o = defaults;

        o.length = 20;
        o.counter = 2;
        check_generate(2, "service.test", "person@example.net", "master",
                       o, "j:x_Lo5b1XL_j0we%z`e", 0);
    }

    {
        PassOptions o;

        memset(&o, 0, sizeof(o));
        o.length = 12;
        o.counter = 7;
        o.lowercase = 1;
        o.digits = 1;
        check_generate(3, "\xce\xb4\xce\xbf\xce\xba\xce\xb9\xce\xbc\xce\xae.example",
                       "\xe3\x83\xa6\xe3\x83\xbc\xe3\x82\xb6\xe3\x83\xbc",
                       "p\xc3\xa4ssword", o, "oih5omhygh2v", 0);
    }

    {
        PassOptions o = defaults;

        o.length = 24;
        o.exclude = "0Ool1I!|";
        check_generate(4, "example.com", "alice", "correct horse battery staple",
                       o, "7,.Cp}YnF'eeHHaqbX7#PQhg", 0);
    }

    {
        PassOptions o;

        memset(&o, 0, sizeof(o));
        o.length = 5;
        o.counter = 0;
        o.uppercase = 1;
        o.digits = 1;
        check_generate(5, "x", "y", "", o, "C9O9I", 0);
    }

    {
        PassOptions o = defaults;

        check_generate(6, "lesspass.com", "contact@lesspass.com", "password",
                       o, "\\g-A1-.OHEwrXjT#", 0);
    }

    {
        PassOptions o = defaults;

        o.counter = 10;
        check_generate(7, "site", "login", "test",
                       o, "XFt0F*,r619:+}[.", 0);
        o.counter = 16;
        check_generate(8, "site", "login", "test",
                       o, "l:`nzj>S7+0#uL_d", 0);
    }

    {
        PassOptions o;

        memset(&o, 0, sizeof(o));
        o.length = 0;
        o.lowercase = 1;
        check_generate(9, "site", "login", "master", o, "", 1);
    }

    {
        PassOptions o;

        memset(&o, 0, sizeof(o));
        o.length = 8;
        check_generate(10, "site", "login", "master", o, "", 1);
    }

    {
        PassOptions o;

        memset(&o, 0, sizeof(o));
        o.length = 2;
        o.lowercase = 1;
        o.uppercase = 1;
        o.digits = 1;
        check_generate(11, "site", "login", "master", o, "", 1);
    }

    {
        PassOptions o;

        memset(&o, 0, sizeof(o));
        o.length = 8;
        o.digits = 1;
        o.exclude = "0123456789";
        check_generate(12, "site", "login", "master", o, "", 1);
    }

    {
        PassOptions o = defaults;

        o.length = 4;
        check_generate(13, "site", "login", "master", o, "", 1);
    }

    check_shape();

    if(failures != 0) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("all vectors pass\n");
    return 0;
}
