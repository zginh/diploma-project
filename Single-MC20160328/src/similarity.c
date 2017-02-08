#include "../global.h" 

static SuperFeatureTable* sf_tbl[SUPER_FEATURE_NUMBER];
static SimilarList* similarList; 

static gboolean g_superfeature_equal(Superfeature sf1, Superfeature sf2)
{
	return !memcmp(sf1, sf2, sizeof(Superfeature));
}

void similar_detection_init()
{
	int i;
	for(i = 0; i < SUPER_FEATURE_NUMBER; ++i)
	{
		sf_tbl[i] = (SuperFeatureTable*)malloc(sizeof(SuperFeatureTable));
		
		pthread_rwlock_init(&sf_tbl[i]->rwlock, NULL);

		sf_tbl[i]->index_table = g_hash_table_new_full(g_int_hash, (GEqualFunc)g_superfeature_equal, NULL, (GDestroyNotify)similarLink_destroy);
	}

	if((similarList = similarList_new()) == NULL)
	{
		fprintf(stderr, "%s,%d: similarity list create failure.\n", __FILE__, __LINE__);
	}
}

void* similar_detection_handler(void* arg)
{
	int i;
	bool found = false;
	DataChunk *data_chunk;
	SimilarChunk* new_chunk; 	
	SimilarLink* similar_link = NULL;	
	SkipIndexEntry* entry = NULL; 
	
	if(arg == NULL) return NULL;

	data_chunk = (DataChunk*)arg; 	
	new_chunk =  similarChunk_new(data_chunk->fc);

#ifdef _SKIP_ON_
	if(data_chunk->prev_chunk)
	{
		entry = skip_index_lookup(data_chunk->prev_chunk->chunk_hash);

		if(entry->next_chunk)
		{
			similarLink_append((SimilarLink*)entry->next_chunk, new_chunk);
			dataChunk_destroy(data_chunk);
			return NULL;
		}		
	}
#endif
	
	Super_feature(data_chunk->data, data_chunk->len, (uint64_t*)new_chunk->super_features, SUPER_FEATURE_NUMBER); 
	dataChunk_destroy(data_chunk);

	for(i = 0; (i < SUPER_FEATURE_NUMBER) && (similar_link == NULL); ++i)
	{
		pthread_rwlock_rdlock(&(sf_tbl[i]->rwlock));
		similar_link =(SimilarLink*)g_hash_table_lookup(sf_tbl[i]->index_table, (gconstpointer)new_chunk->super_features[i]);
		pthread_rwlock_unlock(&(sf_tbl[i]->rwlock));
	}

	if(similar_link)
	{
		similarLink_append(similar_link, new_chunk);
	}
	else
	{
		similar_link = similarLink_new(new_chunk);
		similarList_append(similar_link);
		
		for(i = 0; i < SUPER_FEATURE_NUMBER; ++i)
		{
			pthread_rwlock_wrlock(&(sf_tbl[i]->rwlock));
			g_hash_table_insert(sf_tbl[i]->index_table, new_chunk->super_features[i], similar_link);
			pthread_rwlock_unlock(&(sf_tbl[i]->rwlock));
		}
	}
	
#ifdef _SKIP_ON_
	if(data_chunk->prev_chunk)
		skip_index_update(entry, similar_link);
#endif
	
	return NULL;
}


void similar_detection_destroy()
{  
	int i = 0;
	bool flag = true;	
	
	for(i = 0; i < SUPER_FEATURE_NUMBER; ++i)
    {
		if(sf_tbl[i] == NULL)
			continue;
	
		if(flag)
		{
			g_hash_table_destroy(sf_tbl[i]->index_table);
			flag = false;
		}
	
        pthread_rwlock_destroy(&(sf_tbl[i]->rwlock));		
		free(sf_tbl[i]);
    }	

	similarList_destroy(similarList);
}

SimilarChunk* similarChunk_new(FingerChunk *finger_chunk)
{
	SimilarChunk *similar_chunk = NULL;

	if(finger_chunk)
	{
		similar_chunk = (SimilarChunk*)malloc(sizeof(SimilarChunk));

		memset(similar_chunk, 0, sizeof(SimilarChunk));
		similar_chunk->finger_chunk = finger_chunk;
		
	}
	return similar_chunk;
}

void similarChunk_destroy(SimilarChunk *similar_chunk)
{
	if(similar_chunk)
	{
		free(similar_chunk);
	}
}

SimilarLink* similarLink_new(SimilarChunk *new_chunk)
{
	SimilarLink *slink = NULL;

	if(new_chunk)
	{
		slink = (SimilarLink *)malloc(sizeof(SimilarLink));

		pthread_mutex_init(&slink->mutex, NULL);
		slink->total_chunk_number = 1;
		slink->first = new_chunk;
		slink->last = new_chunk;
		slink->next = NULL;
	}

	return slink;
}

void similarLink_destroy(SimilarLink *slink)
{
	SimilarChunk *curr_chunk, *next_chunk;

	if(slink)
	{
		curr_chunk = slink->first;
		while(curr_chunk)
		{
			next_chunk = curr_chunk->next;
			similarChunk_destroy(curr_chunk);
			curr_chunk = next_chunk;
		}
		pthread_mutex_destroy(&slink->mutex);
		free(slink);
	}
}
 
void similarLink_append(SimilarLink *similar_link, SimilarChunk *new_chunk)
{	
    P(similar_link->mutex); 
	
	similar_link->last->next = new_chunk;
	similar_link->last = new_chunk;
    similar_link->total_chunk_number += 1;

	V(similar_link->mutex);  
}
 
 SimilarList *similarList_new()
{
	SimilarList *similarList = (SimilarList *)malloc(sizeof(SimilarList));
	if(similarList == NULL)
	{
		fprintf(stderr, "%s,%d: similarity list create failure.\n", __FILE__, __LINE__);
		return NULL;
	}

	pthread_mutex_init(&similarList->mutex, 0);
	similarList->first = NULL;
    similarList->last = NULL;

	return similarList;
}

void similarList_destroy(SimilarList *similarList)
{
	if(similarList)
	{
		pthread_mutex_destroy(&similarList->mutex);
		free(similarList);
	}
}

void similarList_append(SimilarLink *similar_link)
{	
    P(similarList->mutex);
	
	if(similarList->first == NULL)
		similarList->first = similar_link;
	else
        similarList->last->next = similar_link; 

	similarList->last = similar_link; 
	
    V(similarList->mutex);
	
} 

void get_chunk_sequences(JCR* jcr)
{ 
	uint64_t offset = 0;
	uint32_t buf_len = 0;
	char* compress_ptr;
	SimilarChunk *chunk;
	SimilarLink *pLink;
	FingerChunk *chunk_info;

	uint64_t total_chunks = 0;
	
	if(similarList == NULL)  
	{
		return; 
	}

	int offset_bytes = sizeof(chunk_info->offset);
	int length_bytes = sizeof(chunk_info->chunk_size);
	int fp_bytes = sizeof(chunk_info->fp);

	update_jcr_info(jcr);
	
	buf_len = jcr->deduped_chunk_number * (offset_bytes + length_bytes + fp_bytes);
		
	jcr->compress_sequence = (char*)malloc(buf_len); 
	memset(jcr->compress_sequence, 0, buf_len); 
	
	uint64_t chunk_offset = 0;
	uint32_t chunk_length = 0;
	uint16_t chunk_times = 0;

	pLink = similarList->first;	
	compress_ptr = jcr->compress_sequence;

	bool flag = true;

	while(pLink)
	{
		chunk = pLink->first;

		flag = true;
		
		while(chunk)
		{			
			chunk_info= chunk->finger_chunk;
		
			memcpy(compress_ptr, &(chunk_info->offset), offset_bytes);
			compress_ptr += offset_bytes;
			
			memcpy(compress_ptr, &(chunk_info->chunk_size), length_bytes);
			compress_ptr += length_bytes;

			memcpy(compress_ptr, &(chunk_info->fp), fp_bytes);
			compress_ptr += fp_bytes;
			
			dedup_index_update(chunk_info->chunk_hash, offset);

			readUnit_coalesce(chunk_info->fp, chunk_info->offset, chunk_info->chunk_size);
			
			offset += chunk->finger_chunk->chunk_size;			
			chunk = chunk->next; 
			++total_chunks;
		}		
		pLink = pLink->next;
	}

	readUnit_coalesce(NULL, 0, 0);
	
	//fprintf(stderr, "total_chunks = %ld deduped_chunks = %ld\n", total_chunks, jcr->deduped_chunk_number);
}


