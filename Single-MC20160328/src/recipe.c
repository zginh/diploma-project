#include "../global.h"

const static int32_t magic_key = 0xa1b2c3d4;//bin volume validation

FileInfo* fileInfo_new(char *filename)
{
	if(!filename)  return NULL;

	FileInfo* psFile = (FileInfo*)malloc(sizeof(FileInfo));
	if(!psFile)
	{
		return NULL;
	}
	
	pthread_mutex_init(&psFile->mutex, NULL);
	
	psFile->filename = (char*)malloc(strlen(filename) + 1);
	memset(psFile->filename, 0, strlen(filename) + 1);
	memcpy(psFile->filename, filename, strlen(filename));
	psFile->fp = NULL;
	
	psFile->total_chunk_number= 0;
	psFile->total_chunk_size= 0;
	psFile->deduped_chunk_number= 0;
	psFile->deduped_chunk_size= 0;
	psFile->rp = recipe_new(); 
	psFile->next = NULL;
	
	return psFile;
}

void fileInfo_destroy(FileInfo * finfo)
{
	if(!finfo)
		return;

	pthread_mutex_destroy(&finfo->mutex);
	fclose(finfo->fp);
	free(finfo->filename);
	recipe_destory(finfo->rp);
	free(finfo);	
}

FingerChunk* fingerChunk_new(uint64_t chunk_offset, uint32_t chunk_size)
{
	assert(chunk_size > 0);

    FingerChunk *chunk = (FingerChunk*)malloc(sizeof(FingerChunk));
	
    memset(chunk, 0, sizeof(FingerChunk));
	chunk->offset = chunk_offset;
	chunk->chunk_size = chunk_size;
	
    return chunk;
}

void fingerchunk_destory(FingerChunk *fc)
{
    free(fc);
}

DataChunk* dataChunk_new(FileInfo *file, FingerChunk *fc, unsigned char *buf, uint32_t len)
{
	assert((buf != NULL) && (fc != NULL) && (file != NULL));

	DataChunk *chunk = (DataChunk*)malloc(sizeof(DataChunk));
	assert(chunk != NULL);

	chunk->data = (unsigned char*)malloc(len);
	memcpy(chunk->data, buf, len);
	chunk->file = file;
	chunk->fc = fc;
	chunk->len = len;
	chunk->next = NULL;

	return chunk;
}

void dataChunk_destroy(DataChunk *chunk)
{
	if(chunk)
	{
		free(chunk->data);
		free(chunk);
	}
}


Recipe *recipe_new()
{
    Recipe *rp = (Recipe *)malloc(sizeof(Recipe));
    memset((char* )rp, 0, sizeof(Recipe));
    return rp;
}

void recipe_destory(Recipe *rp)
{
    FingerChunk *fc = rp->first;
    while(fc)
    {
        rp->first = fc->next;
        free(fc);
        fc = rp->first;
    }
    free(rp);
}

bool recipe_append_fingerchunk(Recipe *rp, FingerChunk *fc)
{
    if(!fc)
        return FALSE;
    
    fc->next = NULL;
    if(!rp->first)
        rp->first = fc;
    else
        rp->last->next = fc;
	
    rp->last = fc;
	rp->total_chunks += 1;

    return TRUE;
}

uint32_t write_recipe_to_buf(char* buf, Recipe *rp)
{
	uint32_t len = 0;

	if(rp == NULL || rp->total_chunks == 0)  return 0;

	FingerChunk *elem = rp->first;
	while(elem)
	{
		memcpy(buf + len, (char* )&elem->offset, sizeof(elem->offset));
		len += sizeof(elem->offset);
		memcpy(buf + len, (char* )&elem->chunk_size, sizeof(elem->chunk_size));
		len += sizeof(elem->chunk_size);
        elem = elem->next;
	}

	return len;
}

void write_recipe_to_file(char* pathname, Recipe *rp)
{
    if(rp == NULL || pathname == NULL)
        return;

	FILE *fd = fopen(pathname, "w+");
    if(fd < 0)
    {
        fprintf(stderr, "write recipe to file error.\n");
        return;
    }

    FingerChunk *elem = rp->first;
	while(elem)
	{
        fprintf(fd, "Offset: %d\tSize: %d\n", elem->offset, elem->chunk_size);
        elem = elem->next;
	}
    
    fclose(fd);
} 
