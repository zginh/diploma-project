#include "../global.h"

static DedupIndex *dedep_index = NULL; 
static SkipIndex *skip_index = NULL;

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


bool dedup_index_insert(Fingerprint chunk_hash)
{ 
	bool succ = false;
	DedupIndexEntry *entry = NULL;

	pthread_mutex_lock(&dedep_index->mutex);

	entry = (DedupIndexEntry *)g_hash_table_lookup(dedep_index->index_table, (gconstpointer)chunk_hash);

	if(!entry)
	{
		entry = (DedupIndexEntry *)malloc(sizeof(DedupIndexEntry));
        memset(entry, 0, sizeof(DedupIndexEntry));
        memcpy(entry->chunk_hash, chunk_hash, sizeof(Fingerprint));
		
    	g_hash_table_insert(dedep_index->index_table, chunk_hash, entry);

		succ = true;
    }

	pthread_mutex_unlock(&dedep_index->mutex);
	
	return succ;
}

void dedup_index_update(Fingerprint chunk_hash, uint64_t chunk_offset)
{	
	DedupIndexEntry* entry = dedup_index_lookup(chunk_hash);

	if(!entry)
	{
		fprintf(stderr, "%s,%d: dedup_index_update error\n", __FILE__, __LINE__);
		return;
	}

	memcpy(&entry->chunk_offset, &chunk_offset, sizeof(entry->chunk_offset));
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



void skip_index_init()
{
	skip_index = (SkipIndex*)malloc(sizeof(SkipIndex));

	if(!skip_index)
	{
		fprintf(stderr, "%s,%d: skip_index_init error\n", __FILE__, __LINE__);
		return;
	}
	
 	pthread_mutex_init(&skip_index->mutex, NULL); 
	
	skip_index->index_table = g_hash_table_new_full(g_int64_hash, (GEqualFunc)g_fingerprint_equal, NULL, free);
}

FILE *fpz = fopen("../skip_table.txt", "w");

SkipIndexEntry* skip_index_lookup(Fingerprint chunk_hash)
{
	SkipIndexEntry *entry = NULL;
	
	pthread_mutex_lock(&skip_index->mutex);

	entry = (SkipIndexEntry *)g_hash_table_lookup(skip_index->index_table, (gconstpointer)chunk_hash);
	
	write_fingerprint_to_file(fpz, (char*)chunk_hash, 20);
	
	if(!entry)
	{
		entry = (SkipIndexEntry *)malloc(sizeof(SkipIndexEntry));
		memset(entry, 0, sizeof(SkipIndexEntry));
		memcpy(entry->prev_chunk, chunk_hash, sizeof(Fingerprint));	
		entry->next_chunk = NULL;

		g_hash_table_insert(skip_index->index_table, chunk_hash, entry);

		fprintf(fpz, "\t[NEW]\n");
	}
	else
	{
		fprintf(fpz, "\t[GET]\n");
	}

	pthread_mutex_unlock(&skip_index->mutex);
	
	return entry;
}

bool skip_index_update(SkipIndexEntry *entry, void *similar_chunk)
{
	pthread_mutex_lock(&skip_index->mutex);

	entry->next_chunk = similar_chunk;

	write_fingerprint_to_file(fpz, (char*)entry->prev_chunk, 20);
	fprintf(fpz, "\t[Update]\n");

	pthread_mutex_unlock(&skip_index->mutex);

	return true;
}

void skip_index_destroy()
{
	if(skip_index)
	{		
		pthread_mutex_destroy(&skip_index->mutex);
		g_hash_table_destroy(skip_index->index_table);
		free(skip_index);
		skip_index = NULL; 	
	}
}



