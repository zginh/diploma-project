#ifndef _GLOBAL_H
#define _GLOBAL_H

//#define _SIMHASH_TEST_	//默认simhash
//#define _SKIP_ON_		//基于重复块的相似性检测

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <dirent.h>
#include <getopt.h>
#include <pthread.h>
#include <inttypes.h>
#include <openssl/sha.h>
#include <glib.h>


#define FINGER_LENGTH 40
#define FILE_NAME_LEN 256
#define CHUNK_ADDRESS_LENGTH 64
#define MAX_BUF_SIZE  1024 * 1024
#define SF_COUNTS  4


/* compute the total time and average time*/
#define TIMER_DECLARE(n) struct timeval b##n,e##n
#define TIMER_BEGIN(n) gettimeofday(&b##n, NULL)
#define TIMER_END(n,t) gettimeofday(&e##n, NULL); \
    (t)+=(e##n.tv_usec-b##n.tv_usec) +(e##n.tv_sec-b##n.tv_sec)*1000000

#define P(mutex) pthread_mutex_lock(&mutex)
#define V(mutex) pthread_mutex_unlock(&mutex)

#define TRUE 1
#define FALSE 0
#define FAILURE 1
#define SUCCESS 0
#define ERROR -1

typedef unsigned char Fingerprint[FINGER_LENGTH];
typedef unsigned char Chunkaddress[CHUNK_ADDRESS_LENGTH]; 

#include "lib/bnet.h"
#include "lib/threadq.h"
#include "lib/Spooky.h" 
#include "lib/fnv.h"
#include "src/recipe.h"
#include "src/jcr.h"
#include "src/index.h"
#include "src/chunking.h"
#include "src/readbuf.h"
#include "src/simhash.h"
#include "src/similarity.h"
#include "src/compress.h"
#include "src/decompress.h"
#include "mc.h"

extern JCR *jcr;

extern uint64_t total_bytes;

extern char compress_path[];
extern char decompress_path[];
extern char temp_path[];
extern char log_path[];
extern char tmp_file0[];
extern char tmp_file1[];

extern int max_queue_size;
extern workq_t file_handler_queue;
extern workq_t chunk_handler_queue;
extern workq_t similar_detection_queue;

extern ReadBuf read_buf_queue;
extern ReadUnit *readUnit;

void changeFilePath(char* srcFile, char* destFile, int destLen, char* path);

bool execLinuxCmd(char* cmdString);

bool is_path_exist(char* file_path);

bool overwrite_file(char* filename);

bool write_fingerprint_to_file(FILE* fp, char* str, int len);

bool write_chunk_to_file(FILE* fp, SimilarChunk* chunk);

bool create_directory_recursive(char *path);

uint64_t get_file_size(char* file_path);



#endif
