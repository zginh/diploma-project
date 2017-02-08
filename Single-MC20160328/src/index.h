#ifndef INDEX_H_
#define INDEX_H_

typedef struct dedup_index_entry
{
	Fingerprint chunk_hash; 
	uint64_t chunk_offset;
}DedupIndexEntry;

typedef struct dedup_index
{
	pthread_mutex_t mutex;    
	
	GHashTable *index_table;	//全局分块指纹的哈希表
}DedupIndex;

 	
void dedup_index_init();

DedupIndexEntry* dedup_index_lookup(Fingerprint chunk_hash);

bool dedup_index_insert(Fingerprint chunk_hash);

void dedup_index_update(Fingerprint chunk_hash, uint64_t chunk_offset);

void dedup_index_destroy();



typedef struct skip_index_entry
{
	Fingerprint prev_chunk; 
	void* next_chunk;
}SkipIndexEntry;

typedef struct skip_index
{
	pthread_mutex_t mutex; 
	
	GHashTable *index_table;
}SkipIndex;

void skip_index_init();

SkipIndexEntry* skip_index_lookup(Fingerprint chunk_hash);

bool skip_index_update(SkipIndexEntry *entry, void *similar_chunk); 

void skip_index_destroy();





#endif
