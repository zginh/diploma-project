#ifndef _SIMILARITY_H_
#define _SIMILARITY_H_

typedef struct similar_chunk { 
	FingerChunk *finger_chunk;
	struct similar_chunk *next;
	struct similar_chunk *pre;
}SimilarChunk;

typedef struct sim_chunk_set {
	SimilarChunk *first;
	SimilarChunk *last;
	int chunk_numbers;
	int max_numbers;
}SimChunkSet;

typedef struct similar_list {
	//pthread_mutex_t mutex;
    uint32_t total_chunk_number;	
	SimilarChunk *first;
	SimilarChunk *last;
}SimilarList;

#define AWARE_NUMBER 5
void similar_detection_init();
void similar_detection_destroy();
void* similar_detection_handler(void* data);

SimilarChunk* similarChunk_new(FingerChunk *finger_chunk);
void similarChunk_destroy(SimilarChunk *chunk);

SimilarList* similarList_new();
void similarList_modify_front(SimilarChunk *dup_pos);
void similarList_modify_front_v2(SimilarChunk *dup_pos);
void similarList_modify_backward();
void similarList_destroy(SimilarList *slist);
void similarList_append(SimilarChunk *new_chunk);
SimChunkSet* simchunkset_new();
void simchunkset_clear(SimChunkSet *sim_chunk_set);
void simchunkset_append(SimilarChunk *sc);
void simchunkset_destory(SimChunkSet *sim_chunk_set);
void get_chunk_sequences(JCR* jcr);

#endif