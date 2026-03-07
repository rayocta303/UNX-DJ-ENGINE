#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void to_valid_identifier(char *str) {
    for (int i = 0; str[i]; i++) {
        if (!isalnum(str[i])) {
            str[i] = '_';
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <input_file> <output_file> <array_name>\n", argv[0]);
        return 1;
    }

    FILE *in = fopen(argv[1], "rb");
    if (!in) {
        perror("Failed to open input file");
        return 1;
    }

    FILE *out = fopen(argv[2], "a"); // Append mode
    if (!out) {
        perror("Failed to open output file");
        fclose(in);
        return 1;
    }

    char array_name[256];
    strncpy(array_name, argv[3], 255);
    to_valid_identifier(array_name);

    fseek(in, 0, SEEK_END);
    long size = ftell(in);
    fseek(in, 0, SEEK_SET);

    fprintf(out, "static const unsigned char %s[] = {\n    ", array_name);

    unsigned char buffer[16];
    size_t n;
    long count = 0;
    while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0) {
        for (size_t i = 0; i < n; i++) {
            fprintf(out, "0x%02x%s", buffer[i], (count + 1 == size) ? "" : ", ");
            count++;
            if (count % 16 == 0 && count != size) {
                fprintf(out, "\n    ");
            }
        }
    }

    fprintf(out, "\n};\n");
    fprintf(out, "static const unsigned int %s_size = %ld;\n\n", array_name, size);

    fclose(in);
    fclose(out);
    return 0;
}
