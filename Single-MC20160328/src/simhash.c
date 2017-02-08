#include "../global.h"

static SimhashTable* simhash_htables[SIMHASH_HATBLE_NUM];
static SimhashList* simhash_chunk_list = NULL;

TIMER_DECLARE(6);

void simhash_detection_init()
{ 	
	int i;
	for(i = 0; i < SIMHASH_HATBLE_NUM; ++i)
	{
		simhash_htables[i] = (SimhashTable*)malloc(sizeof(SimhashTable));
		
		pthread_rwlock_init(&(simhash_htables[i]->rwlock), NULL);

		simhash_htables[i]->simArray_table = g_hash_table_new_full(g_int_hash, g_int_equal, NULL, (GDestroyNotify)simhashArray_destroy);	
	}
	
	if((simhash_chunk_list = simhashList_new()) == NULL)
	{
		fprintf(stderr, "%s,%d: simhash_chunk_list init fail.\n", __FILE__, __LINE__);
		return;
	}		
}

void* simhash_detection_handler(void* arg)
{
	int i;
	bool found = false;
	DataChunk *data_chunk;
	SimhashChunk* simhash_chunk; 
	SimhashChunk* index_chunk = NULL; 
	SimhashArray* simhashArray;
	SkipIndexEntry* entry = NULL; 
	
	if(arg == NULL) return NULL;
	
	TIMER_BEGIN(6);
    
	data_chunk = (DataChunk*)arg; 
	
	simhash_chunk =  simhashChunk_new();
	simhash_chunk->finger_chunk = data_chunk->fc;

#ifdef _SKIP_ON_
	if(data_chunk->prev_chunk)
	{
		entry = skip_index_lookup(data_chunk->prev_chunk->chunk_hash);
			
		if(entry->next_chunk)
		{
			simhashList_insert((SimhashChunk*)index_chunk, simhash_chunk);
			dataChunk_destroy(data_chunk);
			return NULL;
		}		
	}
#endif
	
	calcu_simhash(data_chunk->data, data_chunk->len, &(simhash_chunk->simhash));
	dataChunk_destroy(data_chunk);
	
	TIMER_END(6, jcr->simhash_calcu_time);
	TIMER_BEGIN(6);

	char *ptr = (char*)&(simhash_chunk->simhash);
	int subSimhash = 0;
	for(i = 0; i < SIMHASH_HATBLE_NUM; ++i)
	{
		if(index_chunk)
		{
			break;
		}
		
		memcpy(&subSimhash, ptr,  sizeof(SubSimHash));
	
		pthread_rwlock_rdlock(&(simhash_htables[i]->rwlock));
		simhashArray =(SimhashArray*)g_hash_table_lookup(simhash_htables[i]->simArray_table, (gconstpointer)&subSimhash);
		pthread_rwlock_unlock(&(simhash_htables[i]->rwlock));
		
		if(simhashArray)
		{			
			index_chunk = simhashArray_search(simhashArray, simhash_chunk->simhash);
		}

		ptr += sizeof(SubSimHash);
	}

	simhashList_insert(index_chunk, simhash_chunk); 
	
	for(i = 0; i < SIMHASH_HATBLE_NUM; ++i)
	{
		memcpy(&subSimhash, ptr,  sizeof(SubSimHash));
	
		pthread_rwlock_rdlock(&(simhash_htables[i]->rwlock));
		simhashArray =(SimhashArray*)g_hash_table_lookup(simhash_htables[i]->simArray_table, (gconstpointer)&subSimhash);
		pthread_rwlock_unlock(&(simhash_htables[i]->rwlock));
		
		if(simhashArray)
		{
			simhashArray_append(simhashArray, simhash_chunk);
		}
		else
		{
			SimhashArray *new_array = simhashArray_new(subSimhash);
			simhashArray_append(new_array, simhash_chunk);
			pthread_rwlock_wrlock(&(simhash_htables[i]->rwlock));
			g_hash_table_insert(simhash_htables[i]->simArray_table, (gpointer)&new_array->subsimhash, new_array);
			pthread_rwlock_unlock(&(simhash_htables[i]->rwlock));
		}

		ptr += sizeof(SubSimHash);
	}

#ifdef _SKIP_ON_
	if(entry)
	{
		if(index_chunk == NULL)
			index_chunk = simhash_chunk;
			
		skip_index_update(entry, index_chunk);
	}		
#endif

	TIMER_END(6, jcr->simhash_detect_time);
	
	return NULL;
}

void simhash_detection_destroy()
{
	int i;
	for(i = 0; i < SIMHASH_HATBLE_NUM; ++i)
	{
		pthread_rwlock_destroy(&(simhash_htables[i]->rwlock));
		g_hash_table_destroy(simhash_htables[i]->simArray_table);
		free(simhash_htables[i]);
	}
	
	simhashList_destroy(simhash_chunk_list);
}

void calcu_simhash(unsigned char *data, uint32_t len, SimHash *simHash)
{
	int i, j;
	int bit[SIMHASH_BIT];
	int hash_vector[SIMHASH_BIT];
	
	Fnv64_t fnv_val;

	if(data && len)
	{
		memset(simHash, 0, sizeof(SimHash));
		memset(hash_vector, 0, sizeof(hash_vector));

		for(i = 0; i <= len - N_GRAM; ++i)
		{
			fnv_val = fnv_64_buf(data + i, N_GRAM, FNV1_64_INIT);

			memset(bit,0,sizeof(bit));
			for(j = SIMHASH_BIT - 1; j >= 0; --j)
			{
				bit[j] = fnv_val & 0x01;

				if(bit[j])
					++hash_vector[j];
				else
					--hash_vector[j];

				fnv_val >>= 1;
			}
		}

		for(i = 0; i < SIMHASH_BIT; ++i)
		{
			(*simHash) <<= 1;
			
			if(hash_vector[i] > 0)
			{
				(*simHash) +=  0x01;
			}				
		}
	}
}

/*
 * 功能：判断两个SimHash是否相似
 * 返回值：true-相似   false-不相似
*/
bool simhash_compare(SimHash lhs, SimHash rhs, uint16_t distance)
{	
	uint16_t counts = 0;	
	
	SimHash tmp = lhs ^ rhs;	
    while(tmp && counts <= distance)
    {
        tmp &= tmp-1;
		
        ++counts;
    }
  
    return (counts <= distance);
}

SimhashChunk* simhashChunk_new()
{
	SimhashChunk *chunk = NULL;

	chunk = (SimhashChunk*)malloc(sizeof(SimhashChunk));

	if(chunk)
	{
		chunk->finger_chunk = NULL;
		chunk->simhash = 0;
		chunk->next = NULL;
	}

	return chunk;
}

void simhashChunk_destroy(SimhashChunk *chunk)
{
	if(chunk)
	{
		free(chunk);
	}
}

SimhashArray* simhashArray_new(int subSimhash)
{
	SimhashArray *sArray = NULL;
	uint32_t array_size = 0;

	sArray = (SimhashArray*)malloc(sizeof(SimhashArray));
	if(sArray)
	{
		pthread_rwlock_init(&sArray->rwlock, NULL);
        sArray->subsimhash = subSimhash;
		sArray->capacity = 256;
		sArray->chunk_num = 0;
		
		array_size = sizeof(SimhashChunk*) * sArray->capacity;	
		sArray->array = (SimhashChunk**)malloc(array_size);
		memset(sArray->array, 0, array_size);
	}

	return sArray;
}

bool simhashArray_append(SimhashArray *sArray, SimhashChunk *simhash_chunk)
{
	if(sArray == NULL)
		return false;

	pthread_rwlock_wrlock(&sArray->rwlock);

	char *ptr = (char*)sArray->array;
	uint32_t length = sizeof(SimhashChunk*) * sArray->chunk_num;

	if(sArray->chunk_num == sArray->capacity)
	{	
		sArray->capacity *= 2;
		SimhashChunk** new_array = (SimhashChunk**)malloc(sizeof(SimhashChunk*) * sArray->capacity);
		memcpy(new_array, ptr, length);
		free(sArray->array);
		sArray->array = new_array;
		ptr = (char*)sArray->array;
	}

	memcpy(ptr + length , &simhash_chunk, sizeof(SimhashChunk*));
	//*(sArray->array + sArray->chunk_num) = simhash_chunk;
	++sArray->chunk_num;

	pthread_rwlock_unlock(&sArray->rwlock);
	
	return true;
}

SimhashChunk* simhashArray_search(SimhashArray *sArray, SimHash simhash)
{
	int i;
	SimhashChunk *ret_chunk = NULL;
	SimhashChunk *chunk = NULL;

	if(sArray && sArray->chunk_num)
	{	
		pthread_rwlock_rdlock(&sArray->rwlock);

		char *ptr = (char*)sArray->array;
		for(i = 0; i < sArray->chunk_num; ++i)
		{
			memcpy(&chunk, ptr, sizeof(SimhashChunk*));
			if(simhash_compare(simhash, chunk->simhash, 3) == true)
			{
				ret_chunk = chunk;
				break;
			}
			ptr += sizeof(sizeof(SimhashChunk*));
		}

		pthread_rwlock_unlock(&sArray->rwlock);
	}
	
	return ret_chunk;
}

void simhashArray_destroy(SimhashArray *sArray)
{
	if(sArray)
	{
		sArray->capacity = 0;
		pthread_rwlock_destroy(&sArray->rwlock);
		free(sArray->array);
		free(sArray);
	}
}

SimhashList* simhashList_new()
{
	SimhashList *slink = NULL;

	slink = (SimhashList*)malloc(sizeof(SimhashList));

	if(slink)
	{
		memset(slink, 0, sizeof(SimhashList));
		pthread_mutex_init(&slink->mutex, NULL);
	}

	return slink;
}

bool simhashList_insert(SimhashChunk *first_chunk, SimhashChunk *next_chunk)
{
	if(next_chunk == NULL)
		return false;

	pthread_mutex_lock(&simhash_chunk_list->mutex);

	if(first_chunk)
	{		
		next_chunk->next = first_chunk->next;
		first_chunk->next = next_chunk;
		++simhash_chunk_list->similar_chunks;
	}
	else
	{
		if(simhash_chunk_list->first)
			simhash_chunk_list->last->next = next_chunk;
		else
			simhash_chunk_list->first = next_chunk;
			
		simhash_chunk_list->last = next_chunk;
	}
	
	++simhash_chunk_list->total_chunks;	

	pthread_mutex_unlock(&simhash_chunk_list->mutex);

	return true;
}

void simhashList_destroy(SimhashList *slink)
{
	SimhashChunk *curr_chunk, *next_chunk;

	if(slink)
	{
		curr_chunk = slink->first;

		while(curr_chunk)
		{
			next_chunk = curr_chunk->next;
			simhashChunk_destroy(curr_chunk);
			curr_chunk = next_chunk;
		}

		pthread_mutex_destroy(&slink->mutex);
		free(slink);
	}
}

void get_simhash_sequences(JCR* jcr)
{ 	
	uint64_t offset = 0;
	uint32_t buf_len = 0;
	char* compress_ptr;
	SimhashChunk *simhash_chunk;
	FingerChunk *chunk_info;

	uint64_t total_chunks = 0;
	
	if(simhash_chunk_list == NULL)  return; 

	int offset_bytes = sizeof(chunk_info->offset);
	int length_bytes = sizeof(chunk_info->chunk_size);
	int fp_bytes = sizeof(chunk_info->fp);

	update_jcr_info(jcr);
	
	buf_len = simhash_chunk_list->total_chunks * (offset_bytes + length_bytes + fp_bytes);
		
	jcr->compress_sequence = (char*)malloc(buf_len); 
	memset(jcr->compress_sequence, 0, buf_len); 
	
	uint64_t chunk_offset = 0;
	uint32_t chunk_length = 0;
	uint16_t chunk_times = 0;

	simhash_chunk = simhash_chunk_list->first;	
	compress_ptr = jcr->compress_sequence;

	bool flag = true;

	while(simhash_chunk)
	{
		chunk_info = simhash_chunk->finger_chunk;
		
		memcpy(compress_ptr, &(chunk_info->offset), offset_bytes);
		compress_ptr += offset_bytes;
		
		memcpy(compress_ptr, &(chunk_info->chunk_size), length_bytes);
		compress_ptr += length_bytes;

		memcpy(compress_ptr, &(chunk_info->fp), fp_bytes);
		compress_ptr += fp_bytes;
		
		dedup_index_update(chunk_info->chunk_hash, offset);

		readUnit_coalesce(chunk_info->fp, chunk_info->offset, chunk_info->chunk_size);
		
		offset += chunk_info->chunk_size;			
		simhash_chunk = simhash_chunk->next; 
		++total_chunks;
	}

	readUnit_coalesce(NULL, 0, 0);
	
	fprintf(stderr, "total_chunks = %ld deduped_chunks = %ld\n", total_chunks, jcr->deduped_chunk_number);
}


