#ifndef _SIMILARITY_H_
#define _SIMILARITY_H_

typedef unsigned char Superfeature[SIMILAR_HASH_LEN];

typedef struct similar_chunk { 
	FingerChunk *finger_chunk;    //切块信息
	Superfeature super_features[SUPER_FEATURE_NUMBER];    //超级特征值	
	struct similar_chunk *next;
}SimilarChunk;

typedef struct super_feature_table_tag
{
	pthread_rwlock_t rwlock;
	GHashTable *index_table;
}SuperFeatureTable;

typedef struct similar_link {
	pthread_mutex_t mutex;
    uint32_t total_chunk_number;
	Superfeature super_feature;	
	SimilarChunk *first;
	SimilarChunk *last;
	struct similar_link *next;
}SimilarLink;

typedef struct similar_list
{	
    pthread_mutex_t mutex;
	SimilarLink *first;
	SimilarLink *last;
}SimilarList; 

void similar_detection_init();
void similar_detection_destroy();
void* similar_detection_handler(void* data);

SimilarChunk* similarChunk_new(FingerChunk *finger_chunk);
void similarChunk_destroy(SimilarChunk *chunk);

SimilarLink* similarLink_new(SimilarChunk *new_chunk);
void similarLink_destroy(SimilarLink *slink);
void similarLink_append(SimilarLink *similar_link, SimilarChunk *new_chunk);

SimilarList *similarList_new();
void similarList_destroy(SimilarList *similarList);
void similarList_append(SimilarLink *similar_link);

void get_chunk_sequences(JCR* jcr);

#endif