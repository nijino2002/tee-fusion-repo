#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "../../include/tee_fusion.h"
static int open_listen(const char* ip,int port){ int fd=socket(AF_INET,SOCK_STREAM,0); int yes=1; setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes)); struct sockaddr_in a; memset(&a,0,sizeof(a)); a.sin_family=AF_INET; a.sin_port=htons(port); a.sin_addr.s_addr=inet_addr(ip); if(bind(fd,(struct sockaddr*)&a,sizeof(a))<0){perror("bind"); exit(1);} if(listen(fd,10)<0){perror("listen"); exit(1);} return fd; }
static int exporter(SSL* s,uint8_t* o,size_t l){ return SSL_export_keying_material(s,o,l,"EXPORTER-TEEFUSION",19,NULL,0,0); }
int main(int argc,char** argv){
  if(argc<5){ fprintf(stderr,"Usage: %s <ip> <port> <cert> <key>\n",argv[0]); return 1; }
  SSL_library_init(); SSL_load_error_strings();
  SSL_CTX* ctx=SSL_CTX_new(TLS_server_method());
  SSL_CTX_use_certificate_file(ctx, argv[3], SSL_FILETYPE_PEM);
  SSL_CTX_use_PrivateKey_file(ctx, argv[4], SSL_FILETYPE_PEM);
  tee_info_t info; tee_init(&(tee_init_opt_t){.app_id="demo"}, &info);
  int lfd=open_listen(argv[1],atoi(argv[2]));
  for(;;){
    int cfd=accept(lfd,NULL,NULL); if(cfd<0) continue;
    SSL* ssl=SSL_new(ctx); SSL_set_fd(ssl,cfd);
    if(SSL_accept(ssl)<=0){ ERR_print_errors_fp(stderr); SSL_free(ssl); close(cfd); continue; }
    uint8_t exp[32]; exporter(ssl,exp,sizeof(exp));
    tee_attested_key_t ak; tee_key_generate(TEE_EC_P256,&ak);
    tee_key_bind_into_evidence(&ak, exp, sizeof(exp));
    uint8_t sig[256]; size_t sl=sizeof(sig); tee_key_sign(&ak, exp, sizeof(exp), sig, &sl);
    tee_buf_t ue={0}; tee_get_u_evidence(&ue);
    char hdr[256]; int n=snprintf(hdr,sizeof(hdr),"HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nX-PubKey-Len: %zu\r\nX-Sig-Len: %zu\r\nContent-Length: %zu\r\n\r\n", ak.pubkey_len, sl, ue.len);
    SSL_write(ssl,hdr,n); SSL_write(ssl,ak.pubkey,(int)ak.pubkey_len); SSL_write(ssl,sig,(int)sl); if(ue.ptr) SSL_write(ssl,ue.ptr,(int)ue.len);
    if(ue.ptr) free(ue.ptr); SSL_shutdown(ssl); SSL_free(ssl); close(cfd);
  }
  return 0;
}
