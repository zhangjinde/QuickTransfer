#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
// #include <sys/sendfile.h>
#include <time.h>
#include "PrefixHead.h"

#ifdef YI_OS_MAC
    #define basename(name) (name)
#endif
 
int main( int argc, char* argv[] )
{
    if( argc <= 3 )
    {
        printf( "usage: %s ip_address port_number filename\n", basename( argv[0] ) );
        return 1;
    }
    long num=0,sum=0;
    static char buf[1024];
    memset(buf,'\0',sizeof(buf));
    const char* ip = argv[1];
    int port = atoi( argv[2] );
    const char* file_name = argv[3];
 
    int filefd = open( file_name, O_RDONLY );
    assert( filefd > 0 );
    struct stat stat_buf;
    fstat( filefd, &stat_buf );
		
	FILE *fp=fdopen(filefd,"r");
		
    struct sockaddr_in address;
    bzero( &address, sizeof( address ) );
    address.sin_family = AF_INET;
    inet_pton( AF_INET, ip, &address.sin_addr );
    address.sin_port = htons( port );
 
    int sock = socket( PF_INET, SOCK_STREAM, 0 );
    assert( sock >= 0 );
 
    int ret = bind( sock, ( struct sockaddr* )&address, sizeof( address ) );
    assert( ret != -1 );
 
    ret = listen( sock, 5 );
    assert( ret != -1 );
 
    struct sockaddr_in client;
    socklen_t client_addrlength = sizeof( client );
    int connfd = accept( sock, ( struct sockaddr* )&client, &client_addrlength );
    if ( connfd < 0 )
    {
        printf( "errno is: %d\n", errno );
    }
    else
    {
        time_t begintime=time(NULL);
        
        while((fgets(buf,1024,fp))!=NULL){
        	num=send(connfd,buf,sizeof(buf),0);
        	sum+=num;
        	memset(buf,'\0',sizeof(buf));
        }
        
//        sendfile( connfd, filefd, NULL, stat_buf.st_size );
        time_t endtime=time(NULL);
        printf("sum:%ld\n",sum);
        printf("need time:%d\n",endtime-begintime);
        close( connfd );
    }
 
    close( sock );
    return 0;
}
