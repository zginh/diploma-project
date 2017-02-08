#include "../global.h" 

static SimChunkSet* simchunkset;
static SimilarList* similarList;
static int sim_new_count=0;


void similar_detection_init()
{
	if((similarList = similarList_new()) == NULL)
	{
		fprintf(stderr, "%s,%d: similarity list create failure.\n", __FILE__, __LINE__);
	}
	if((simchunkset = simchunkset_new())==NULL)
	{
		fprintf(stderr, "%s,%d: simchunkset create failure. \n",__FILE__,__LINE__);
	}
}


void similar_detection_destroy()
{  

	similarList_destroy(similarList);
	simchunkset_destory(simchunkset);
}

SimilarChunk *similarChunk_new(FingerChunk *finger_chunk)
{
	SimilarChunk *similar_chunk = NULL;

	if(finger_chunk)
	{
		similar_chunk = (SimilarChunk*)malloc(sizeof(SimilarChunk));

		memset(similar_chunk, 0, sizeof(SimilarChunk));
		similar_chunk->finger_chunk = finger_chunk;
		similar_chunk->next = NULL;
		similar_chunk->pre = NULL;
		
	}
	sim_new_count++;
	return similar_chunk;
}

void similarChunk_destroy(SimilarChunk *similar_chunk)
{
	if(similar_chunk)
	{
		free(similar_chunk);
	}
}

SimilarList* similarList_new()
{
	SimilarList *slist = (SimilarList *)malloc(sizeof(SimilarList));
	if(slist==NULL)
	{
		fprintf(stderr,"%s,%d:similarity list create failure.\n",__FILE__,__LINE__);
		return NULL;
	}
	//pthread_mutex_init(&slist->mutex,0);
	slist->first = NULL;
	slist->last = NULL;
	slist->total_chunk_number = 0;
		
	return slist;
}
void similarList_modify_front(SimilarChunk *dup_pos)
{
	//P(similarList->mutex);
	int i = 0;
	SimilarChunk *temp = similarList->last;
	while(temp->finger_chunk->dup_flag!=1&&i<AWARE_NUMBER)
	{
		i++;
		temp = temp->pre;
	}
	if(dup_pos==similarList->first)
	{
		SimilarChunk *last = similarList->last;
		SimilarChunk *first = temp->next;
		first->pre = NULL;
		last->next = dup_pos;
		dup_pos->pre = last;
		similarList->first = first;
		similarList->last = temp;
		similarList->last->next = NULL;
		return;
	}
	else if(dup_pos==similarList->last)
		return;
	else
	{	
		
		SimilarChunk *last_sim_chunk = similarList->last;
		SimilarChunk *first_sim_chunk = temp->next;
		if(first_sim_chunk==NULL)
			return;
		SimilarChunk *sim_chunk = dup_pos->pre;
		sim_chunk->next = first_sim_chunk;
		first_sim_chunk->pre = sim_chunk;
		last_sim_chunk->next = dup_pos;
		dup_pos->pre = last_sim_chunk;
		similarList->last = temp;
		similarList->last->next = NULL;
		return;
	}
	//V(similarList->mutex);
	
}

void similarList_modify_front_v2(SimilarChunk *dup_pos)
{
	/*插在链表头*/
	if(dup_pos==similarList->first)
	{
		simchunkset->last->next=dup_pos;
		dup_pos->pre = simchunkset->last;
		simchunkset->first->pre = NULL;
		similarList->first = simchunkset->first;
	}
	else
	{
		/*连续不同的重复块之间没有缓冲块*/
		if(simchunkset->first==NULL)
			return;
		else{
			SimilarChunk *temp = simchunkset->first;
			while(dup_pos!=temp&&temp!=NULL)
			{
				temp = temp->next;
			}
			/*当今处理的重复块与缓冲区simchunkset中的块相同*/
			if(temp!=NULL)
			{
				similarList->last->next = simchunkset->first;
				simchunkset->first->pre = similarList->last;
				similarList->last = simchunkset->last;
				similarList->last->next = NULL;
			}
			/*当今处理的重复块与缓冲区simchunkset中的块不同*/
			else{
				SimilarChunk *sim_chunk = dup_pos->pre;
				sim_chunk->next = simchunkset->first;
				if(simchunkset->first->pre!=NULL)
					fprintf(stderr,"%s %d:simchunkset's first error.\n",__FILE__,__LINE__);
				simchunkset->first->pre = sim_chunk;
				simchunkset->last->next = dup_pos;
				dup_pos->pre = simchunkset->last;
			}
		}
	}
	similarList->total_chunk_number+=simchunkset->chunk_numbers;
	simchunkset_clear(simchunkset);
}
void similarList_modify_backward()
{
	
	//P(similarList->mutex);
	DedupIndexEntry *entry = dedup_index_lookup(pre_dedup_hash);
	SimilarChunk *sim_chunk = NULL;
	if(entry->pos->next==NULL)
	{
		entry->pos->next = simchunkset->first;
		simchunkset->first->pre = entry->pos;
		simchunkset->last->next = NULL;
		similarList->last = simchunkset->last;
	}
	else
	{
		sim_chunk = entry->pos->next;
		entry->pos->next = simchunkset->first;
		simchunkset->first->pre = entry->pos;
		simchunkset->last->next = sim_chunk;
		sim_chunk->pre = simchunkset->last;
	}
	similarList->total_chunk_number+=simchunkset->chunk_numbers;
	simchunkset_clear(simchunkset);
	//V(similarList->mutex);
}

void similarList_destroy(SimilarList *slist)
{
	SimilarChunk *curr_chunk, *next_chunk;
	int count=0;
	if(slist)
	{
		curr_chunk = slist->first;
		while(curr_chunk)
		{
			count++;
			next_chunk = curr_chunk->next;
			similarChunk_destroy(curr_chunk);
			curr_chunk = next_chunk;
		}
		//fprintf(stderr,"%d\n",count);
		//fprintf(stderr,"%d\n",slist->total_chunk_number);
		if(slist->total_chunk_number!=count)
		{
			fprintf(stderr,"%s,%d: similarList count is wrong\n",__FILE__,__LINE__);
		}
		//pthread_mutex_destroy(&slist->mutex);
		free(slist);
	}
}
 
void similarList_append(SimilarChunk *new_chunk)
{	
    //P(similarList->mutex); 
	if(similarList->first==NULL)//链表中第一块
	{
		new_chunk->pre =NULL;
		similarList->first = new_chunk;
		similarList->last = new_chunk;
	}
	else
	{
		new_chunk->pre = similarList->last;
		similarList->last->next= new_chunk;
		similarList->last = new_chunk;
	}
	new_chunk->next = NULL;
    similarList->total_chunk_number += 1;
	//V(similarList->mutex);  
}

SimChunkSet *simchunkset_new()
{
	SimChunkSet *sim_set = (SimChunkSet *)malloc(sizeof(SimChunkSet *));
	if(sim_set==NULL)
	{
		fprintf(stderr, "%s,%d: sim_set creat failure.\n",__FILE__,__LINE__);
		return NULL;
	}
	sim_set->first = NULL;
	sim_set->last = NULL;
	sim_set->chunk_numbers = 0;
	sim_set->max_numbers = AWARE_NUMBER;
	return sim_set;
}

void simchunkset_append(SimilarChunk * sc)
{
	if(simchunkset->chunk_numbers < simchunkset->max_numbers)
	{
		if(simchunkset->chunk_numbers==0)
		{
			simchunkset->first = sc;
			simchunkset->last = sc;
			simchunkset->first->pre = NULL;
			simchunkset->last->next = NULL;
		}
		else
		{
			simchunkset->last->next = sc;
			sc->pre = simchunkset->last;
			simchunkset->last = sc;
			simchunkset->last->next = NULL;
		}
		simchunkset->chunk_numbers++;
	}
	if(simchunkset->chunk_numbers==simchunkset->max_numbers)
	{
		sim_flag = 0;
		similarList_modify_backward();
	}
		
}

void simchunkset_clear(SimChunkSet *sim_chunk_set)
{
	sim_chunk_set->first = NULL;
	sim_chunk_set->last = NULL;
	sim_chunk_set->chunk_numbers = 0;
	sim_chunk_set->max_numbers =AWARE_NUMBER;
}

void simchunkset_destory(SimChunkSet *sim_chunk_set)
{
	if(sim_chunk_set->first!=NULL)
	{
		simchunkset_clear(sim_chunk_set);
	}
	free(sim_chunk_set);
}
void get_chunk_sequences(JCR* jcr)
{ 
	uint64_t offset = 0;
	uint32_t buf_len = 0;
	char* compress_ptr;
	SimilarChunk *chunk;
	FingerChunk *chunk_info;
	//FILE *fcfp;
	uint64_t total_chunks = 0;

	//fprintf(stderr,"The counts of SimilarChunk_new used are %d:",sim_new_count);
	if(similarList == NULL)  
	{
		return; 
	}

	/*if((fcfp=fopen("./similarList.txt","w"))==NULL)
		{
			fprintf(stderr,"%s,%d: fingerchunk.txt open error\n",__FILE__,__LINE__);
			return;
		}
*/
	if(simchunkset->chunk_numbers!=0)
	{
		similarList->last->next = simchunkset->first;
		simchunkset->first->pre = similarList->last;
		similarList->total_chunk_number+=simchunkset->chunk_numbers;
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

	chunk = similarList->first;	
	compress_ptr = jcr->compress_sequence;

	bool flag = true;
	while(chunk)
	{		

		
		//fprintf(fcfp,"%llu\n",chunk->finger_chunk->offset);
		chunk_info= chunk->finger_chunk;
		
		memcpy(compress_ptr, &(chunk_info->offset), offset_bytes);
		compress_ptr += offset_bytes;
			
		memcpy(compress_ptr, &(chunk_info->chunk_size), length_bytes);
		compress_ptr += length_bytes;

		memcpy(compress_ptr, &(chunk_info->fp), fp_bytes);
		compress_ptr += fp_bytes;

		dedup_index_update(chunk_info->chunk_hash, offset);
		
		#ifdef _READ_THREAD_
		readUnit_coalesce(chunk_info->fp, chunk_info->offset, chunk_info->chunk_size);
		#endif
			
		offset += chunk->finger_chunk->chunk_size;			
		chunk = chunk->next; 
		++total_chunks;
	}		

	simlist_number = similarList->total_chunk_number;
	#ifdef _READ_THREAD_
	readUnit_coalesce(NULL, 0, 0);
	#endif
	//fclose(fcfp);
	//fprintf(stderr, "total_chunks = %ld deduped_chunks = %ld similar_chunks = %ld\n", total_chunks, jcr->deduped_chunk_number,similarList->total_chunk_number);
}


