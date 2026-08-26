/* LessPass-compatible generator used by the CLI and every Kry app target.
 * native/pass_core_test.c checks fixed vectors for byte stability. */

#include "pass_core.h"

#include <stdio.h>
#include <string.h>

static const char LOWERCASE_CHARACTERS[] = "abcdefghijklmnopqrstuvwxyz";
static const char UPPERCASE_CHARACTERS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
static const char DIGIT_CHARACTERS[] = "0123456789";
static const char SYMBOL_CHARACTERS[] = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";

/* ------------------------------------------------------------------ */
/* SHA-256                                                             */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t buffer[64];
    size_t buffer_len;
} Sha256;

static const uint32_t SHA256_K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu,
    0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u,
    0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u,
    0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu,
    0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u,
    0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u,
    0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u, 0x1e376c08u,
    0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu,
    0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
};

static uint32_t
rotr32(uint32_t value, unsigned bits)
{
    return (value >> bits) | (value << (32 - bits));
}

static void
sha256_init(Sha256 *ctx)
{
    ctx->state[0] = 0x6a09e667u;
    ctx->state[1] = 0xbb67ae85u;
    ctx->state[2] = 0x3c6ef372u;
    ctx->state[3] = 0xa54ff53au;
    ctx->state[4] = 0x510e527fu;
    ctx->state[5] = 0x9b05688cu;
    ctx->state[6] = 0x1f83d9abu;
    ctx->state[7] = 0x5be0cd19u;
    ctx->bitlen = 0;
    ctx->buffer_len = 0;
}

static void
sha256_block(Sha256 *ctx, const uint8_t block[64])
{
    uint32_t w[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t t1, t2;
    int i;

    for(i = 0; i < 16; i++)
        w[i] = ((uint32_t)block[i * 4] << 24) |
               ((uint32_t)block[i * 4 + 1] << 16) |
               ((uint32_t)block[i * 4 + 2] << 8) |
               (uint32_t)block[i * 4 + 3];
    for(i = 16; i < 64; i++) {
        uint32_t s0 = rotr32(w[i - 15], 7) ^ rotr32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr32(w[i - 2], 17) ^ rotr32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    a = ctx->state[0];
    b = ctx->state[1];
    c = ctx->state[2];
    d = ctx->state[3];
    e = ctx->state[4];
    f = ctx->state[5];
    g = ctx->state[6];
    h = ctx->state[7];

    for(i = 0; i < 64; i++) {
        uint32_t s1 = rotr32(e, 6) ^ rotr32(e, 11) ^ rotr32(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        t1 = h + s1 + ch + SHA256_K[i] + w[i];
        uint32_t s0 = rotr32(a, 2) ^ rotr32(a, 13) ^ rotr32(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        t2 = s0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }

    ctx->state[0] += a;
    ctx->state[1] += b;
    ctx->state[2] += c;
    ctx->state[3] += d;
    ctx->state[4] += e;
    ctx->state[5] += f;
    ctx->state[6] += g;
    ctx->state[7] += h;
}

static void
sha256_update(Sha256 *ctx, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;

    ctx->bitlen += (uint64_t)len * 8u;
    while(len > 0) {
        size_t take = 64 - ctx->buffer_len;

        if(take > len)
            take = len;
        memcpy(ctx->buffer + ctx->buffer_len, p, take);
        ctx->buffer_len += take;
        p += take;
        len -= take;
        if(ctx->buffer_len == 64) {
            sha256_block(ctx, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }
}

static void
sha256_final(Sha256 *ctx, uint8_t out[32])
{
    uint64_t bitlen = ctx->bitlen;
    int i;

    ctx->buffer[ctx->buffer_len++] = 0x80u;
    if(ctx->buffer_len > 56) {
        while(ctx->buffer_len < 64)
            ctx->buffer[ctx->buffer_len++] = 0;
        sha256_block(ctx, ctx->buffer);
        ctx->buffer_len = 0;
    }
    while(ctx->buffer_len < 56)
        ctx->buffer[ctx->buffer_len++] = 0;
    for(i = 7; i >= 0; i--)
        ctx->buffer[ctx->buffer_len++] = (uint8_t)(bitlen >> (i * 8));
    sha256_block(ctx, ctx->buffer);

    for(i = 0; i < 8; i++) {
        out[i * 4] = (uint8_t)(ctx->state[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(ctx->state[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(ctx->state[i] >> 8);
        out[i * 4 + 3] = (uint8_t)ctx->state[i];
    }
}

static void
sha256(const void *data, size_t len, uint8_t out[32])
{
    Sha256 ctx;

    sha256_init(&ctx);
    sha256_update(&ctx, data, len);
    sha256_final(&ctx, out);
}

void
pass_core_sha256(const void *data, size_t len, uint8_t out[32])
{
    sha256(data, len, out);
}

static const int MASTER_EMOJI_CODEPOINTS[] = {
    0x1F436, 0x1F431, 0x1F42D, 0x1F439, 0x1F430, 0x1F98A, 0x1F43B, 0x1F43C,
    0x1F428, 0x1F42F, 0x1F981, 0x1F42E, 0x1F437, 0x1F438, 0x1F435, 0x1F414,
    0x1F427, 0x1F989, 0x1F43A, 0x1F434, 0x1F984, 0x1F41D, 0x1F98B, 0x1F422,
    0x1F34E, 0x1F34A, 0x1F34B, 0x1F349, 0x1F347, 0x1F353, 0x1F352, 0x1F351,
    0x1F951, 0x1F33D, 0x1F355, 0x1F354, 0x1F35F, 0x1F369, 0x1F335, 0x1F332,
    0x1F333, 0x1F334, 0x1F331, 0x1F33B, 0x1F338, 0x1F308, 0x2B50, 0x1F319,
    0x1F525, 0x1F4A7, 0x26C4, 0x1F389, 0x1F3B8, 0x1F3AF, 0x1F3B2, 0x1F381,
    0x1F680, 0x1F697, 0x2693, 0x1F3A8, 0x1F511, 0x1F4A1, 0x1F4DA, 0x1F3A7,
};

const int *
pass_core_master_emoji_codepoints(int *count)
{
    if(count != NULL)
        *count = (int)(sizeof(MASTER_EMOJI_CODEPOINTS) /
                       sizeof(MASTER_EMOJI_CODEPOINTS[0]));
    return MASTER_EMOJI_CODEPOINTS;
}

static size_t
append_utf8(char *out, size_t used, size_t out_size, uint32_t codepoint)
{
    unsigned char bytes[4];
    int len = 0;

    if(out == NULL || out_size == 0)
        return 0;
    if(codepoint <= 0x7Fu) {
        bytes[0] = (unsigned char)codepoint;
        len = 1;
    } else if(codepoint <= 0x7FFu) {
        bytes[0] = (unsigned char)(0xC0u | (codepoint >> 6));
        bytes[1] = (unsigned char)(0x80u | (codepoint & 0x3Fu));
        len = 2;
    } else if(codepoint <= 0xFFFFu) {
        bytes[0] = (unsigned char)(0xE0u | (codepoint >> 12));
        bytes[1] = (unsigned char)(0x80u | ((codepoint >> 6) & 0x3Fu));
        bytes[2] = (unsigned char)(0x80u | (codepoint & 0x3Fu));
        len = 3;
    } else if(codepoint <= 0x10FFFFu) {
        bytes[0] = (unsigned char)(0xF0u | (codepoint >> 18));
        bytes[1] = (unsigned char)(0x80u | ((codepoint >> 12) & 0x3Fu));
        bytes[2] = (unsigned char)(0x80u | ((codepoint >> 6) & 0x3Fu));
        bytes[3] = (unsigned char)(0x80u | (codepoint & 0x3Fu));
        len = 4;
    }

    if(len <= 0 || used + (size_t)len >= out_size) {
        out[used < out_size ? used : out_size - 1] = '\0';
        return used;
    }
    memcpy(out + used, bytes, (size_t)len);
    used += (size_t)len;
    out[used] = '\0';
    return used;
}

void
pass_core_master_emoji(const char *master, char *out, size_t out_size)
{
    uint8_t sum[32];
    size_t used = 0;
    int table_count = 0;
    const int *table = pass_core_master_emoji_codepoints(&table_count);

    if(out == NULL || out_size == 0)
        return;
    out[0] = '\0';
    if(master == NULL)
        master = "";
    pass_core_sha256(master, strlen(master), sum);
    for(int i = 0; i < PASS_MASTER_EMOJI_COUNT && table_count > 0; i++)
        used = append_utf8(out, used, out_size,
                           (uint32_t)table[sum[i] % table_count]);
}

/* ------------------------------------------------------------------ */
/* HMAC-SHA256                                                         */
/* ------------------------------------------------------------------ */

static void
hmac_sha256(const void *key, size_t key_len,
            const void *message, size_t message_len,
            uint8_t out[32])
{
    uint8_t block_key[64];
    uint8_t pad[64];
    uint8_t inner[32];
    Sha256 ctx;
    size_t i;

    memset(block_key, 0, sizeof(block_key));
    if(key_len > 64) {
        sha256(key, key_len, block_key);
    } else {
        memcpy(block_key, key, key_len);
    }

    for(i = 0; i < 64; i++)
        pad[i] = block_key[i] ^ 0x36u;
    sha256_init(&ctx);
    sha256_update(&ctx, pad, 64);
    sha256_update(&ctx, message, message_len);
    sha256_final(&ctx, inner);

    for(i = 0; i < 64; i++)
        pad[i] = block_key[i] ^ 0x5cu;
    sha256_init(&ctx);
    sha256_update(&ctx, pad, 64);
    sha256_update(&ctx, inner, 32);
    sha256_final(&ctx, out);
}

/* ------------------------------------------------------------------ */
/* PBKDF2 (single block, dkLen = 32)                                   */
/* ------------------------------------------------------------------ */

void
pass_core_derive_key(const char *password, size_t password_len,
                     const char *salt, size_t salt_len,
                     uint8_t out[32])
{
    uint8_t block_input[512];
    uint8_t previous[32];
    size_t input_len;
    int round;

    /* salt || INT(32, big-endian) == salt || 00 00 00 01 */
    input_len = salt_len;
    if(input_len > sizeof(block_input) - 4)
        input_len = sizeof(block_input) - 4;
    if(input_len > 0)
        memcpy(block_input, salt, input_len);
    block_input[input_len] = 0;
    block_input[input_len + 1] = 0;
    block_input[input_len + 2] = 0;
    block_input[input_len + 3] = 1;
    input_len += 4;

    hmac_sha256(password, password_len, block_input, input_len, previous);
    memcpy(out, previous, 32);

    for(round = 1; round < PASS_DERIVATION_ROUNDS; round++) {
        hmac_sha256(password, password_len, previous, 32, previous);
        for(int i = 0; i < 32; i++)
            out[i] ^= previous[i];
    }
    memset(block_input, 0, sizeof(block_input));
}

/* ------------------------------------------------------------------ */
/* Entropy draws (big-endian 256-bit value, successive remainders)     */
/* ------------------------------------------------------------------ */

static int
entropy_take_remainder(uint8_t entropy[32], int divisor)
{
    unsigned carry = 0;
    int i;

    for(i = 0; i < 32; i++) {
        unsigned current = carry * 256u + entropy[i];

        entropy[i] = (uint8_t)(current / (unsigned)divisor);
        carry = current % (unsigned)divisor;
    }
    return (int)carry;
}

/* ------------------------------------------------------------------ */
/* Generator                                                           */
/* ------------------------------------------------------------------ */

static int
set_error(char *err, size_t err_size, const char *message)
{
    if(err != NULL && err_size > 0)
        snprintf(err, err_size, "%s", message);
    return 1;
}

static int
class_filtered(const char *characters, const char *exclude, char *out, int out_size)
{
    int count = 0;

    for(const char *c = characters; *c != '\0'; c++) {
        int excluded = 0;

        if(exclude != NULL) {
            for(const char *e = exclude; *e != '\0'; e++) {
                if(*e == *c) {
                    excluded = 1;
                    break;
                }
            }
        }
        if(!excluded && count < out_size)
            out[count++] = *c;
    }
    return count;
}

int
pass_core_generate(const char *site, const char *login, const char *master,
                   const PassOptions *options,
                   char *out, size_t out_size,
                   char *err, size_t err_size)
{
    static const char *class_names[4] = {"lowercase", "uppercase", "digits", "symbols"};
    static const char *class_sets[4] = {
        LOWERCASE_CHARACTERS, UPPERCASE_CHARACTERS, DIGIT_CHARACTERS, SYMBOL_CHARACTERS
    };
    const int class_enabled[4] = {
        options->lowercase != 0, options->uppercase != 0,
        options->digits != 0, options->symbols != 0
    };
    char classes[4][32];
    int class_lengths[4];
    int class_count = 0;
    char alphabet[128];
    int alphabet_length = 0;
    char salt[640];
    size_t salt_len;
    uint8_t entropy[32];
    char password[136];
    int password_length;
    char required[4];
    char message[192];
    int i;

    if(options->length < PASS_MIN_LENGTH || options->length > PASS_MAX_LENGTH) {
        snprintf(message, sizeof(message),
                 "password length must be between %d and %d",
                 PASS_MIN_LENGTH, PASS_MAX_LENGTH);
        return set_error(err, err_size, message);
    }

    for(i = 0; i < 4; i++) {
        if(!class_enabled[i])
            continue;
        class_lengths[class_count] =
            class_filtered(class_sets[i], options->exclude,
                           classes[class_count], (int)sizeof(classes[class_count]));
        if(class_lengths[class_count] == 0) {
            snprintf(message, sizeof(message),
                     "enabled %s character class is empty after exclusions", class_names[i]);
            return set_error(err, err_size, message);
        }
        memcpy(alphabet + alphabet_length, classes[class_count],
               (size_t)class_lengths[class_count]);
        alphabet_length += class_lengths[class_count];
        class_count++;
    }
    if(class_count == 0)
        return set_error(err, err_size, "at least one character class must be enabled");
    if(options->length < class_count) {
        snprintf(message, sizeof(message),
                 "password length %d is smaller than the %d enabled character classes",
                 options->length, class_count);
        return set_error(err, err_size, message);
    }
    if(options->length == class_count)
        return set_error(err, err_size,
                         "password length must leave room for generated characters");
    if((size_t)options->length + 1 > out_size || options->length + 1 > (int)sizeof(password))
        return set_error(err, err_size, "password length exceeds the output buffer");

    salt_len = 0;
    if(site != NULL) {
        size_t site_len = strlen(site);

        if(salt_len + site_len >= sizeof(salt))
            return set_error(err, err_size, "site name is too long");
        memcpy(salt + salt_len, site, site_len);
        salt_len += site_len;
    }
    if(login != NULL) {
        size_t login_len = strlen(login);

        if(salt_len + login_len >= sizeof(salt))
            return set_error(err, err_size, "login is too long");
        memcpy(salt + salt_len, login, login_len);
        salt_len += login_len;
    }
    salt_len += (size_t)snprintf(salt + salt_len, sizeof(salt) - salt_len,
                                 "%llx", (unsigned long long)options->counter);

    pass_core_derive_key(master != NULL ? master : "", master != NULL ? strlen(master) : 0,
                         salt, salt_len, entropy);

    password_length = options->length - class_count;
    for(i = 0; i < password_length; i++)
        password[i] = alphabet[entropy_take_remainder(entropy, alphabet_length)];

    for(i = 0; i < class_count; i++)
        required[i] = classes[i][entropy_take_remainder(entropy, class_lengths[i])];

    for(i = 0; i < class_count; i++) {
        int position = entropy_take_remainder(entropy, password_length);

        memmove(password + position + 1, password + position,
                (size_t)(password_length - position));
        password[position] = required[i];
        password_length++;
    }

    memcpy(out, password, (size_t)password_length);
    out[password_length] = '\0';
    memset(password, 0, sizeof(password));
    memset(required, 0, sizeof(required));
    return 0;
}
