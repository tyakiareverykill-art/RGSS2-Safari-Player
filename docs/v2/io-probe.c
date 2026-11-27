#define _FILE_OFFSET_BITS 64

#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>

static unsigned char buffer[65536];

int read_sample(FILE *file, off_t offset, size_t length) {
    if (fseeko(file, offset, SEEK_SET) != 0 ||
        fread(buffer, 1, length, file) != length) {
        return -1;
    }
    return buffer[0] + buffer[length - 1];
}

/* Returns -1 on I/O failure, otherwise a checksum comparable to FileReaderSync. */
int probe_archive(void) {
    FILE *file = fopen("/game/archive.zip", "rb");
    if (!file) return -1;

    int checksum = -1;
    if (fseeko(file, 0, SEEK_END) == 0) {
        const off_t size = ftello(file);
        if (size > 0) {
            const size_t length = size < (off_t)sizeof(buffer) ? (size_t)size : sizeof(buffer);
            checksum = 0;
            for (int i = 0; i < 64; ++i) {
                const off_t offset = (size - length) * i / 63;
                const int sample = read_sample(file, offset, length);
                if (sample < 0) {
                    checksum = -1;
                    break;
                }
                checksum += sample;
            }
        }
    }
    fclose(file);
    return checksum;
}
