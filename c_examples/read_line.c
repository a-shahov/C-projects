#include <errno.h>
#include <unistd.h>
#include <tgmath.h>
#include <fcntl.h>
#include <string.h>
#include "read_line.h"

#include <stdio.h>

#define true 1
#define false 0

#define BUF_SIZE 256

struct rlb_struct {
    int fd;
    size_t len;
    char *next_char;
    char *end;
    char buffer[BUF_SIZE];
};

static int read_line_buf_init(int fd, struct rlb_struct *rlbuf);

ssize_t read_line(int fd, void *buffer, size_t buf_length)
{
    static struct rlb_struct rlbuf = {0}; 
    char *buf = NULL;
    ssize_t numWrite = 0;
    
    if (buf_length <= 1 || buffer == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    buf = (char*)buffer;
    
    while (true) {
        if (rlbuf.next_char == rlbuf.end) {
            if (read_line_buf_init(fd, &rlbuf) != 0) {
                errno = ENOMEM;
                return -1;
            } else if (rlbuf.next_char == rlbuf.end) {
                *buf = '\0';
                return numWrite;
            }
        }
        
        while (rlbuf.next_char != rlbuf.end) {
            if (*rlbuf.next_char == '\n') {
                *buf++ = *rlbuf.next_char++;
                *buf = '\0';
                return ++numWrite;
            } else if ((size_t)numWrite < buf_length - 2) {
                *buf++ = *rlbuf.next_char++;
                ++numWrite;
            } else {
                rlbuf.next_char++;
            }
        }
    }
}

static int read_line_buf_init(int fd, struct rlb_struct *rlbuf)
{
    ssize_t count_read;
    
    if (fd <= -1 || rlbuf == NULL) {
        errno = EINVAL;
        return -1;
    }
    
    memset(rlbuf, 0, sizeof(struct rlb_struct));
    
    if ((count_read = read(fd, rlbuf->buffer, BUF_SIZE - 1)) == -1) { //добавить проверку на EINTR!
        return -1;
    }
    rlbuf->fd = fd;
    rlbuf->len = count_read;
    rlbuf->next_char = rlbuf->buffer;
    rlbuf->end = rlbuf->buffer + count_read;
    
    return 0;
}
