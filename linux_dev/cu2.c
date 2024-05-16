#include <ncurses.h>

#define DX 3

void main() {
    WINDOW *win;
    int c = 0;

    initscr();
    noecho();
    cbreak();
    printw("Window:");
    refresh();

    win = newwin(LINES-2*DX, COLS-2*DX, DX, DX);
    keypad(win, TRUE);
    scrollok(win, TRUE);
    box(win, 1, 0);
    wmove(win, 1, 0);
    while((c = wgetch(win)) != 27) {
        wprintw(win, " Key: %d, Name: %s\n", c, keyname(c));
	box(win, 0, 0);
	wrefresh(win);
    }
    endwin();
}
