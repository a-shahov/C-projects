#include <string.h>
#include <stdlib.h>
#include <ncurses.h>

#define MAXSIZE 8096
#define DX 3

struct file_struct {
    int size;
    int lines_num;
    char *buf;
};

struct document_state {
    int number_space;
    int vert_offset;
    int horiz_offset;
};

void fill_window(WINDOW *win, struct file_struct *fstruct, struct document_state *docstate);

int main(int argc, char *argv[]) {
    FILE *fp;
    struct file_struct fstruct = {0};
    struct document_state docstate = {0};
    int c;
    WINDOW *win;

    if (argc < 2) {
	fprintf(stderr ,"usage: %s <file>\n", argv[0]);
	exit(1);
    }

    if ((fp = fopen(argv[1], "r")) == NULL) {
	fprintf(stderr, "file %s doesn't exist\n", argv[1]);
	exit(1);
    }

    fseek(fp, 0, SEEK_END);
    if ((fstruct.size = ftell(fp)) > MAXSIZE) {
	fprintf(stderr, "file %s too big\n", argv[1]);
	exit(1);
    }
    fseek(fp, 0, SEEK_SET);

    fstruct.buf = (char*)malloc(fstruct.size * sizeof(char));

    if (fread(fstruct.buf, 1, fstruct.size, fp) != fstruct.size) {
	fprintf(stderr, "Read error\n");
	exit(1);
    }
    fclose(fp);

    char *tmp = strchr(fstruct.buf, '\n');
    while (tmp) {
	fstruct.lines_num++;
	tmp = strchr(tmp+1, '\n');
    }

    initscr();
    noecho();
    cbreak();
    if (has_colors()) {
	start_color();
        init_pair(1, COLOR_RED,     COLOR_BLACK);
        init_pair(2, COLOR_GREEN,   COLOR_BLACK);
        init_pair(3, COLOR_WHITE,   COLOR_BLACK);
    }
    attrset(COLOR_PAIR(1));
    printw("file: %s", argv[1]);
    attrset(COLOR_PAIR(3));
    refresh();


    win = newwin(LINES-2*DX, COLS-2*DX, DX, DX);
    keypad(win, TRUE);
    box(win, 0, 0);
    wmove(win, 1, 0);

    int delim = 1;
    while (fstruct.lines_num / delim != 0) {
	delim *= 10;
        docstate.number_space += 1;
    }

    fill_window(win, &fstruct, &docstate);
    while ((c = wgetch(win)) != 27) {
	switch (c)
	{
	case KEY_UP:
	    if (docstate.vert_offset != 0) {
		docstate.vert_offset--;
	    }
	    break;
	case KEY_DOWN:
	    if (docstate.vert_offset < fstruct.lines_num) {
		docstate.vert_offset++;
	    }
	    break;
	case KEY_RIGHT:
	    docstate.horiz_offset++;
	    break;
	case KEY_LEFT:
	    if (docstate.horiz_offset != 0)
	        docstate.horiz_offset--;
	    break;
	}
        fill_window(win, &fstruct, &docstate);
    }

    endwin();
    return 0;
}

void fill_window(WINDOW *win, struct file_struct *fstruct, struct document_state *docstate) {
    char nstr[10], fstr[20] = "% ";
    int width;

    sprintf(nstr, "%d", docstate->number_space);
    strcat(fstr, (const char*)nstr);
    strcat(fstr, "d ");  // "% Nd "

    width = COLS - 2 * DX - (docstate->number_space + 1);

    wclear(win);
    wmove(win, 1, 0);

    wattrset(win, COLOR_PAIR(2));
    wprintw(win, fstr, 1 + docstate->vert_offset);
    wattrset(win, COLOR_PAIR(3));
    for (int count_lines=1, count_w=0, count_hoffset=0, count_voffset=0, i=0; i<fstruct->size; i++) {
	if (count_voffset - docstate->vert_offset >= LINES - 2 * DX) {
	    break;
	}
	if (count_voffset < docstate->vert_offset) {
	    if (fstruct->buf[i] == '\n') {
		count_lines++;
		count_voffset++;
	    }
	    continue;
	}
	if (count_hoffset++ < docstate->horiz_offset && fstruct->buf[i] != '\n') {
	    continue;
	}
	if (count_w >= width - 3 && fstruct->buf[i] != '\n') {
	    continue;
	}
	waddch(win, fstruct->buf[i]);
	count_w++;
        if (fstruct->buf[i] == '\n') {
            wattrset(win, COLOR_PAIR(2));
	    wprintw(win, fstr, ++count_lines);
            wattrset(win, COLOR_PAIR(3));
	    count_w = 0;
	    count_hoffset = 0;
            count_voffset++;
	}
    }
    box(win, 0, 0);
    wmove(win, 1, 0);
    wrefresh(win);
}

