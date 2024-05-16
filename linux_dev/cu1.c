#include <curses.h>

int main(int argc, char *argv[]) {
    initscr();
    move(LINES / 2 - 1, COLS / 2 - 20);
    addstr("Hello!");
    getch();
    //refresh();
    endwin();
    return 0;
}
