#include "core/hid/hid_mapper.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void trim_xml_tag(char *str) {
    char *start = strchr(str, '>');
    if (!start) return;
    start++;
    char *end = strchr(start, '<');
    if (!end) return;
    *end = '\0';
    memmove(str, start, strlen(start) + 1);
}

bool HID_LoadMapping(HidMapping *map, const char *path) {
    memset(map, 0, sizeof(HidMapping));
    FILE *f = fopen(path, "r");
    if (!f) return false;

    char line[512];
    bool inInfo = false;
    bool inScripts = false;

    while (fgets(line, sizeof(line), f)) {
        char *p;
        
        if (strstr(line, "<info>")) inInfo = true;
        else if (strstr(line, "</info>")) inInfo = false;
        
        if (inInfo) {
            if ((p = strstr(line, "<name>"))) {
                char temp[128]; strcpy(temp, p); trim_xml_tag(temp);
                strncpy(map->name, temp, 127);
            } else if ((p = strstr(line, "<product "))) {
                // Parse vendor_id and product_id
                char *vid = strstr(p, "vendor_id=\"");
                char *pid = strstr(p, "product_id=\"");
                if (vid) sscanf(vid + 11, "0x%hx", &map->vendorId);
                if (pid) sscanf(pid + 12, "0x%hx", &map->productId);
            }
        }

        if (strstr(line, "<scriptfiles>")) inScripts = true;
        else if (strstr(line, "</scriptfiles>")) inScripts = false;

        if (inScripts) {
            if ((p = strstr(line, "filename=\""))) {
                char *start = p + 10;
                char *end = strchr(start, '\"');
                if (end && map->scriptCount < 8) {
                    int len = end - start;
                    strncpy(map->scriptFiles[map->scriptCount], start, len);
                    map->scriptFiles[map->scriptCount][len] = '\0';
                    map->scriptCount++;
                }
            }
        }
    }

    fclose(f);
    return (map->vendorId != 0);
}
