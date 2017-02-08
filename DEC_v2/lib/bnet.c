#include "../global.h"

int readn(int fd, char* vptr, int n)
{
    int nleft = n;
    int nread = 0;
    char* ptr = vptr;
    while(nleft > 0)
    {
        nread = read(fd, ptr, nleft);
        if(nread < 0)
        {
            if(errno == EINTR)
                nread = 0;
            else
                return(ERROR);
        }
        else if(nread == 0)
        {
            break;
        }
        ptr += nread;
        nleft -= nread;
    }
    return(n - nleft); 
}

int writen(int fd, char* vptr, int n)
{
    int nleft = n;
    int nwritten = 0;
    char* ptr = vptr;
    while(nleft > 0)
    {
        nwritten = write(fd, ptr, nleft);
        if(nwritten < 0)
        {
            if(errno == EINTR)
                nwritten = 0;
            else
                return(ERROR);
        }
        ptr += nwritten;
        nleft -= nwritten;
    }
    return(n);
}
