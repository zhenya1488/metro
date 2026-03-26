#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <locale.h>
#ifdef _WIN32
    #include <Windows.h>
    void set_locale() {
        setlocale(LC_ALL, ".1251");
        SetConsoleCP(1251);
        SetConsoleOutputCP(1251);
    }
#else
    void set_locale() {
        return;
    }
#endif

#define SIG_LEN 32

int main(int argc, char** argv)
{
    set_locale();

    if (argc != 4) {
        fprintf(stderr, "Usage: %s <file> <key> <sigfile>\n", argv[0]);
        return 1;
    }

    const char* filename = argv[1];
    const char* key = argv[2];
    const char* sigfile = argv[3];

    /* Читаем файл */
    FILE* f = fopen(filename, "rb");
    if (!f) return 1;

    fseek(f, 0, SEEK_END); // 0 - offset, SEEK_END - origin (от конца файла)
    long size = ftell(f);
    rewind(f);

    unsigned char* data = malloc(size);
    fread(data, 1, size, f);
    fclose(f);

    /* Считаем HMAC */
    unsigned char hmac[SIG_LEN];
    unsigned int hmac_len = 0;

    HMAC(
        EVP_sha256(),
        key, strlen(key),
        data, size,
        hmac, &hmac_len
    );

    free(data);

    /* Пишем .sig */
    FILE* s = fopen(sigfile, "wb");
    fwrite(hmac, 1, hmac_len, s);
    fclose(s);

    return 0;
}