#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

int main()
{
    int fd;
    char buffer[24*4096];

    fd=creat("swap",0640);
    write(fd,buffer,24*4096);
    close(fd);
    exit(0);
}
