#ifndef _RECIPE_H_
#define _RECIPE_H_

/* 描述文件信息的结构体 */
typedef struct file_info {
	char* filename;
	FILE* fp;
	pthread_mutex_t mutex;
	
	uint64_t total_chunk_number;
	uint64_t total_chunk_size;
	uint64_t deduped_chunk_number;
	uint64_t deduped_chunk_size; 

	struct recipe_tag *rp;
	struct file_info *next;
}FileInfo;

/* 描述分块指纹信息的结构体 */
typedef struct finger_chunk { 
	Fingerprint chunk_hash;	
	FILE* fp;	
    uint64_t offset;
    uint32_t chunk_size;	
    struct finger_chunk *next;
}FingerChunk;

/* 描述分块数据信息的结构体 */
typedef struct data_chunk {
	FileInfo *file;
	FingerChunk *prev_chunk;
	FingerChunk *fc;
	unsigned char *data;
	uint32_t len;	
	struct data_chunk *next;
}DataChunk;


typedef struct recipe_tag {
    char filename[FILE_NAME_LEN];
    uint64_t total_chunks;
    FingerChunk *first;
    FingerChunk *last;
    struct recipe_tag *next;
}Recipe;  

FileInfo* fileInfo_new(char *filename);
void fileInfo_destroy(FileInfo* finfo);

FingerChunk* fingerChunk_new(uint64_t chunk_offset, uint32_t chunk_size);
void fingerChunk_destroy(FingerChunk *fc);

DataChunk* dataChunk_new(FileInfo *file, FingerChunk *fc, unsigned char *buf, uint32_t len);
void dataChunk_destroy(DataChunk *chunk);

Recipe *recipe_new();
void recipe_destory(Recipe *rp);
bool recipe_append_fingerchunk(Recipe *rp, FingerChunk *fc);
uint32_t write_recipe_to_buf(char* buf, Recipe *rp);
void write_recipe_to_file(char* pathname, Recipe *rp);
 
#endif
