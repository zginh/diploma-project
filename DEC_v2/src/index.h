#ifndef INDEX_H_
#define INDEX_H_

typedef struct dedup_index_entry
{
	Fingerprint chunk_hash; 
	uint64_t chunk_offset;
	SimilarChunk *pos;
}DedupIndexEntry;

typedef struct dedup_index
{
	pthread_mutex_t mutex;    
	
	GHashTable *index_table;	//全局分块指纹的哈希表
}DedupIndex;

 	
void dedup_index_init();

DedupIndexEntry* dedup_index_lookup(Fingerprint chunk_hash);

DedupIndexEntry *dedup_index_insert(Fingerprint chunk_hash,SimilarChunk *sc);

void dedup_index_pos(Fingerprint chunk_hash, SimilarChunk *similar_chunk);

void dedup_index_update(Fingerprint chunk_hash, uint64_t chunk_offset);

void dedup_index_destroy();


#endif
