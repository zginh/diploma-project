#ifndef _CHUNKING_H_
#define _CHUNKING_H_

#define HASHSIZE 40
#define MAXBUF (128*1024) 
#define SIMILAR_HASH_LEN  8
#define MIN_CHUNK_SIZE 2048     /* 2k */
#define MAX_CHUNK_SIZE 65536    /* 6k */

/* AE分块算法相关参数*/
#define COMP_SIZE 8
#define AVG_CHUNK_SIZE  8 * 1024 
#define WINDOW_SIZE    (AVG_CHUNK_SIZE / 1.7183) 

/* SF相关参数 */
#define SUPER_FEATURE_NUMBER 4 
#define NUMFEATURE  16
#define GSIZE32BIT  0xffffffffLL
#define MSB64 0x8000000000000000LL
#define FINGERPRINT_PT  0xbfe6b8a5bf378d83LL
#define BREAKMARK_VALUE 0x78
 
void chunk_agl_init();
void chunk_finger(unsigned char* buf, uint32_t len, unsigned char hash[]);
int chunk_data(unsigned char* data, uint32_t len);
int ae_chunk_data(unsigned char *p, int n);

void Super_feature(unsigned char* data, uint32_t len, uint64_t SF[], int number);

#endif
