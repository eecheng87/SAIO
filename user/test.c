#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main(){
    int count = 777777777;
    char *mesg = "hahahaa\n";
    int fd = open("test.txt", O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);
    //write(fd, "hahaha\n", 7);
    //close(fd);

#if 1
    printf("fd = %d, &buf[0] = %lu\n", fd, (unsigned long)mesg);
    //printf("pid = %ld\n", syscall(39));
    syscall(400, fd, mesg);
    while(count--);
    //syscall(401);
#endif
    close(fd);
    printf("hello\n");
    return 0;
}
