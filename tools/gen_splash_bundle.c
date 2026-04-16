#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <ctype.h>

void to_valid_identifier(char *str) {
    for (int i = 0; str[i]; i++) {
        if (!isalnum(str[i])) {
            str[i] = '_';
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <dir> <output_h>\n", argv[0]);
        return 1;
    }

    const char *dir_path = argv[1];
    const char *out_path = argv[2];

    FILE *out = fopen(out_path, "w");
    if (!out) {
        perror("Failed to open output file");
        return 1;
    }

    fprintf(out, "#ifndef SPLASH_BUNDLE_H\n#define SPLASH_BUNDLE_H\n\n");

    DIR *dir = opendir(dir_path);
    if (!dir) {
        perror("Failed to open directory");
        fclose(out);
        return 1;
    }

    struct dirent *entry;
    char names[512][128];
    int count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (strstr(entry->d_name, ".png")) {
            char full_path[512];
            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, entry->d_name);

            FILE *in = fopen(full_path, "rb");
            if (!in) continue;

            char id[128];
            snprintf(id, sizeof(id), "splash_%s", entry->d_name);
            to_valid_identifier(id);
            strncpy(names[count], id, 127);

            fseek(in, 0, SEEK_END);
            long size = ftell(in);
            fseek(in, 0, SEEK_SET);

            fprintf(out, "static const unsigned char %s[] = {\n    ", id);
            unsigned char buffer[16];
            size_t n;
            long bytes = 0;
            while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0) {
                for (size_t i = 0; i < n; i++) {
                    fprintf(out, "0x%02x%s", buffer[i], (bytes + 1 == size) ? "" : ", ");
                    bytes++;
                    if (bytes % 16 == 0 && bytes != size) fprintf(out, "\n    ");
                }
            }
            fprintf(out, "\n};\nstatic const unsigned int %s_size = %ld;\n\n", id, size);
            fclose(in);
            count++;
            if (count >= 512) break;
        }
    }
    closedir(dir);

    // Generate indexing array
    fprintf(out, "static const unsigned char* splash_frames[] = {\n");
    for (int i = 0; i < count; i++) {
        fprintf(out, "    %s%s\n", names[i], (i + 1 == count) ? "" : ",");
    }
    fprintf(out, "};\n\n");

    fprintf(out, "static const unsigned int splash_frames_size[] = {\n");
    for (int i = 0; i < count; i++) {
        fprintf(out, "    %s_size%s\n", names[i], (i + 1 == count) ? "" : ",");
    }
    fprintf(out, "};\n\n");

    fprintf(out, "#define SPLASH_FRAME_COUNT %d\n\n", count);
    fprintf(out, "#endif\n");

    fclose(out);
    printf("Generated splash bundle with %d frames.\n", count);
    return 0;
}
