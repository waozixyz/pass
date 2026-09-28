#include <pass_core.h>

#include <string.h>

typedef struct {
    int length;
    unsigned long long counter;
    int lowercase;
    int uppercase;
    int digits;
    int symbols;
    const char *exclude;
} LegacyPassOptions;

static size_t
text_length(const char *text)
{
    return text != NULL ? strlen(text) : 0;
}

static void
copy_error(char *err, size_t err_size, String message)
{
    size_t length;

    if(err == NULL || err_size == 0)
        return;
    length = message.length < err_size - 1 ?
             message.length : err_size - 1;
    if(length > 0)
        memcpy(err, message.data, length);
    err[length] = '\0';
}

void
pass_core_derive_key(const char *password, size_t password_len,
                     const char *salt, size_t salt_len,
                     uint8_t out[32])
{
    pass_core_DeriveKey(out, StringView(password, password_len),
                        StringView(salt, salt_len));
}

void
pass_core_sha256(const void *data, size_t len, uint8_t out[32])
{
    pass_core_Sha256(out, StringView((const char *)data, len));
}

const int *
pass_core_master_emoji_codepoints(int *count)
{
    if(count != NULL)
        *count = 64;
    return master_emoji_codepoints;
}

void
pass_core_master_emoji(const char *master, char *out, size_t out_size)
{
    Slice output;

    if(out == NULL || out_size == 0)
        return;
    output.data = out;
    output.length = (int64_t)out_size;
    pass_core_MasterEmoji(
        StringView(master != NULL ? master : "", text_length(master)),
        output);
}

int
pass_core_generate(const char *site, const char *login, const char *master,
                   const LegacyPassOptions *options,
                   char *out, size_t out_size,
                   char *err, size_t err_size)
{
    PassOptions core_options;
    PassResult result;

    if(err != NULL && err_size > 0)
        err[0] = '\0';
    if(options == NULL)
        return 1;

    memset(&core_options, 0, sizeof(core_options));
    core_options.length = options->length;
    core_options.counter = (uint64_t)options->counter;
    core_options.lowercase = options->lowercase != 0;
    core_options.uppercase = options->uppercase != 0;
    core_options.digits = options->digits != 0;
    core_options.symbols = options->symbols != 0;
    core_options.exclude = StringView(
        options->exclude != NULL ? options->exclude : "",
        text_length(options->exclude));

    result = pass_core_PassGenerate(
        StringView(site != NULL ? site : "", text_length(site)),
        StringView(login != NULL ? login : "", text_length(login)),
        StringView(master != NULL ? master : "", text_length(master)),
        core_options);
    if(!result.valid) {
        copy_error(err, err_size, result.error);
        return 1;
    }
    if((size_t)result.length + 1 > out_size) {
        copy_error(err, err_size,
                   StringLiteral("password length exceeds the output buffer"));
        return 1;
    }

    memcpy(out, result.password, (size_t)result.length);
    out[result.length] = '\0';
    return 0;
}
