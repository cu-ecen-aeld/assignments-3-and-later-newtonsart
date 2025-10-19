#include <stdio.h>
#include <syslog.h>

int main(int argc, char *argv[]){
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <path/to/file> <string to write>\n", argv[0]);
        syslog(LOG_ERR, "Invalid arguments. Expected file path and string to write.");
        return 1;
    }

    char *filepath = argv[1];
    char *text = argv[2];

    FILE *fp = fopen(filepath, "w");
    if (fp == NULL) {
        syslog(LOG_ERR, "Error opening file: %s", filepath);
        perror("fopen");
        closelog();
        return 1;
    }

    if (fprintf(fp, "%s", text) < 0) {
        syslog(LOG_ERR, "Error writing to file: %s", filepath);
        perror("fprintf");
        fclose(fp);
        return 1;
    }

    fclose(fp);

    syslog(LOG_DEBUG, "Writing %s to %s", text, filepath);

    return 0;
}
