#ifndef _SIMHASH_H_
#define _SIMHASH_H_


/* SimHash相关参数 */
typedef uint16_t SubSimHash;
typedef uint64_t SimHash;

#define N_GRAM 4
#define SIMHASH_BIT 64
#define SIMHASH_HATBLE_NUM  sizeof(SimHash)/sizeof(SubSimHash)

typedef struct simhash_chunk { 
	FingerChunk *finger_chunk;  //切块信息
	SimHash simhash;    		//SimHash值	
	struct simhash_chunk *next;
}SimhashChunk;

typedef struct simhash_List {	//SimHash检测结果
	pthread_mutex_t mutex;
	uint64_t total_chunks;
	uint64_t similar_chunks;
	
	SimhashChunk *first;
	SimhashChunk *last;
}SimhashList;

typedef struct simhash_array {
	pthread_rwlock_t rwlock;	//互斥访问的读写锁
	int	subsimhash;		//key
	SimhashChunk **array; 		//value，一个存储SimhashChunk*的动态数组
	uint32_t capacity;			//动态数组的长度
	uint32_t chunk_num;			//动态数组中元素的个数
}SimhashArray;

typedef struct simhash_htable_tag
{
	pthread_rwlock_t rwlock;
	GHashTable *simArray_table;
}SimhashTable;

void simhash_detection_init();
void* simhash_detection_handler(void* data);
void simhash_detection_destroy();

void calcu_simhash(unsigned char *data, uint32_t len, SimHash *simHash);
bool simhash_compare(SimHash lhs, SimHash rhs, uint16_t distance);

SimhashChunk* simhashChunk_new();
void simhashChunk_destroy(SimhashChunk *chunk);

SimhashArray* simhashArray_new(int subSimhash);
bool simhashArray_append(SimhashArray *sArray, SimhashChunk *simhash_chunk);
SimhashChunk* simhashArray_search(SimhashArray *sArray, SimHash simhash);
void simhashArray_destroy(SimhashArray *sArray);

SimhashList* simhashList_new();
bool simhashList_insert(SimhashChunk *first_chunk, SimhashChunk *next_chunk);
void simhashList_destroy(SimhashList *slink);

void get_simhash_sequences(JCR* jcr);

#endif

