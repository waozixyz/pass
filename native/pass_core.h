#ifndef PASS_CORE_H
#define PASS_CORE_H

#include <stddef.h>
#include <stdint.h>

#define PASS_DERIVATION_ROUNDS 100000
#define PASS_MASTER_EMOJI_COUNT 4
#define PASS_MIN_LENGTH 5
#define PASS_MAX_LENGTH 35

typedef struct {
    int length;
    uint64_t counter;
    int lowercase;
    int uppercase;
    int digits;
    int symbols;
    const char *exclude; /* NUL-terminated; may be NULL or empty */
} PassOptions;

/* Derives the 32-byte PBKDF2-HMAC-SHA256 key (one block, 100000 rounds),
 * the byte-exact counterpart of the Go package's deriveKey. */
void pass_core_derive_key(const char *password, size_t password_len,
                          const char *salt, size_t salt_len,
                          uint8_t out[32]);

/* Computes SHA-256. Shared with UI code that must match the Go package's
 * master-password fingerprint derivation. */
void pass_core_sha256(const void *data, size_t len, uint8_t out[32]);

const int *pass_core_master_emoji_codepoints(int *count);
void pass_core_master_emoji(const char *master, char *out, size_t out_size);

/* Generates a LessPass-compatible password. Returns 0 on success and writes
 * the NUL-terminated password into out. Returns nonzero on invalid options
 * and writes the reason into err (English, matches the Go error strings). */
int pass_core_generate(const char *site, const char *login, const char *master,
                       const PassOptions *options,
                       char *out, size_t out_size,
                       char *err, size_t err_size);

#endif
