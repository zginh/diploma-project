#include "../global.h"

static DedupIndex *dedep_index = NULL; 

static gboolean g_fingerprint_equal(Fingerprint fp1, Fingerprint fp2)
{
	return !memcmp(fp1, fp2, sizeof(Fingerprint));
}

void dedup_index_init()
{
	dedep_index = (DedupIndex*)malloc(sizeof(DedupIndex));

	if(!dedep_index)
	{
		fprintf(stderr, "%s,%d: dedup_index_init error\n", __FILE__, __LINE__);
		return;
	}
	
 	pthread_mutex_init(&dedep_index->mutex, NULL); 
	
	dedep_index->index_table = g_hash_table_new_full(g_int64_hash, (GEqualFunc)g_fingerprint_equal, NULL, free);
}

DedupIndexEntry* dedup_index_lookup(Fingerprint chunk_hash)
{

	DedupIndexEntry *entry = NULL;
	
	pthread_mutex_lock(&dedep_index->mutex);
	
	entry = (DedupIndexEntry *)g_hash_table_lookup(dedep_index->index_table, (gconstpointer)chunk_hash);

	pthread_mutex_unlock(&dedep_index->mutex);
	
	return entry;
}


DedupIndexEntry *dedup_index_insert(Fingerprint chunk_hash,SimilarChunk *sc)
{ 
	//bool succ = false;
	DedupIndexEntry *entry = NULL;

	//pthread_rwlock_wrlock(&dedup_index->mutex);
	pthread_mutex_lock(&dedep_index->mutex);

	entry = (DedupIndexEntry *)g_hash_table_lookup(dedep_index->index_table, (gconstpointer)chunk_hash);

	if(!entry)
	{
		entry = (DedupIndexEntry *)malloc(sizeof(DedupIndexEntry));
        memset(entry, 0, sizeof(DedupIndexEntry));
        memcpy(entry->chunk_hash, chunk_hash, sizeof(Fingerprint));
		entry->pos = sc;
    	g_hash_table_insert(dedep_index->index_table, chunk_hash, entry);

    }

	pthread_mutex_unlock(&dedep_index->mutex);
	
	return entry;
}

void dedup_index_pos(Fingerprint chunk_hash,SimilarChunk * similar_chunk)
{
	DedupIndexEntry *entry = dedup_index_lookup(chunk_hash);
	if(!entry)
	{
		fprintf(stderr,"%s,%d: dedup_index_pos error\n.",__FILE__, __LINE__);
		return;
	}
	memcpy(entry->pos,similar_chunk,sizeof(SimilarChunk *));
}
void dedup_index_update(Fingerprint chunk_hash, uint64_t chunk_offset)
{	
	DedupIndexEntry* entry = dedup_index_lookup(chunk_hash);

	if(!entry)
	{
		fprintf(stderr, "%s,%d: dedup_index_update error\n", __FILE__, __LINE__);
		return;
	}
	//memcpy(&entry->chunk_offset, &chunk_offset, sizeof(entry->chunk_offset));
	entry->chunk_offset = chunk_offset;

}

void dedup_index_destroy()
{
	if(dedep_index)
	{
		pthread_mutex_destroy(&dedep_index->mutex);
		g_hash_table_destroy(dedep_index->index_table);
		free(dedep_index);
		dedep_index = NULL;		
	}
} 


