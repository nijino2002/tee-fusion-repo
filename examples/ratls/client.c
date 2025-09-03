#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
int main(int argc,char** argv){
  if(argc<3){ fprintf(stderr,"Usage: %s <ip> <port>\n", argv[0]); return 1; }
  SSL_library_init(); SSL_load_error_strings(); SSL_CTX* ctx=SSL_CTX_new(TLS_client_method());
  SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
  int fd=socket(AF_INET,SOCK_STREAM,0); struct sockaddr_in a; memset(&a,0,sizeof(a)); a.sin_family=AF_INET; a.sin_port=htons(atoi(argv[2])); a.sin_addr.s_addr=inet_addr(argv[1]); if(connect(fd,(struct sockaddr*)&a,sizeof(a))<0){perror("connect"); return 1;}
  SSL* ssl=SSL_new(ctx); SSL_set_fd(ssl,fd); if(SSL_connect(ssl)<=0){ ERR_print_errors_fp(stderr); return 1; }
  const char* req="GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"; SSL_write(ssl, req, (int)strlen(req));
  char hdr[512]={0}; int hn=SSL_read(ssl,hdr,sizeof(hdr)-1); if(hn<=0){ fprintf(stderr,"hdr fail\n"); return 1; }
  size_t x_pub=0,x_sig=0,cl=0; sscanf(hdr,"%*[^\n]\n%*[^\n]\nX-PubKey-Len: %zu\nX-Sig-Len: %zu\nContent-Length: %zu",&x_pub,&x_sig,&cl);
  uint8_t* pub=(uint8_t*)malloc(x_pub); uint8_t* sig=(uint8_t*)malloc(x_sig); uint8_t* ue=(uint8_t*)malloc(cl);
  int r1=SSL_read(ssl,pub,(int)x_pub); int r2=SSL_read(ssl,sig,(int)x_sig); int r3=SSL_read(ssl,ue,(int)cl);
  printf("pub=%d/%zu sig=%d/%zu ue=%d/%zu\n", r1,x_pub,r2,x_sig,r3,cl);
  free(pub); free(sig); free(ue); SSL_shutdown(ssl); SSL_free(ssl); close(fd); SSL_CTX_free(ctx); return 0;
}
