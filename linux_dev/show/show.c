#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>

#define MAXSIZE 8096

int main(int argc, char *argv[]) {
    FILE *fp;
    char *buf = NULL;
    int size;

    if (argc < 2) {
	fprintf(stderr ,"usage: %s <file>\n", *argv);
	exit(1);
    }

    if ((fp = fopen(*++argv, "r")) == NULL) {
	fprintf(stderr, "file %s doesn't exist\n", *argv);
	exit(1);
    }

    fseek(fp, 0, SEEK_END);
    if ((size = ftell(fp)) > MAXSIZE) {
	fprintf(stderr, "file %s too big\n", *argv);
	exit(1);
    }
    fseek(fp, 0, SEEK_SET);

    buf = (char*)malloc(size * sizeof(char));

    if (fread(buf, 1, size, fp) != size) {
	fprintf(stderr, "Read error\n");
	exit(1);
    }

    printf("%s", buf);

	
    fclose(fp);
    return 0;
}

