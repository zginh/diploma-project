#include "../global.h"

int wait = 0;

ReadUnit* readUnit_new(FILE *fp, uint64_t offset, uint32_t length)
{
	ReadUnit *readUnit = (ReadUnit*)malloc(sizeof(ReadUnit));

	readUnit->fp = fp;
	readUnit->offset = offset;
	readUnit->length = length;
	readUnit->lifetime = 1;

	return readUnit;
}

void readUnit_destroy(ReadUnit *readUnit)
{
	if(readUnit)
		free(readUnit);
}

void readUnit_coalesce(FILE *fp, uint64_t offset, uint32_t length)
{

	if(fp == NULL)
	{
		readBuf_append(&read_buf_queue, readUnit);
		readUnit = NULL;
		return;
	}

	if(readUnit == NULL)
	{
		readUnit = readUnit_new(fp, offset, length);
		return;
	}

	if(readUnit->fp == fp)
	{
		uint64_t left = readUnit->offset;
		uint64_t right = readUnit->offset + readUnit->length;
	
		if(left > offset)
			left = offset;
		if(right < offset + length)
			right = offset + length;

		if(right - left <= MAX_UNIT_SIZE)
		{
			readUnit->offset = left;
			readUnit->length = right - left;
			readUnit->lifetime += 1;
			return;
		}	
	}

	readBuf_append(&read_buf_queue, readUnit);
	readUnit = readUnit_new(fp, offset, length);

	return;
}

BufUnit* bufUnit_new(uint32_t buf_size)
{
	BufUnit *bufUnit = (BufUnit*)malloc(sizeof(BufUnit));
	
	pthread_mutex_init(&bufUnit->mutex, NULL);
	bufUnit->capacity = buf_size > MAX_UNIT_SIZE ? buf_size : MAX_UNIT_SIZE;
	bufUnit->data = (char*)malloc(bufUnit->capacity);
	bufUnit->info = NULL;

	return bufUnit;
}

void bufUnit_resize(BufUnit *bufUnit, uint32_t buf_size)
{
	if(bufUnit)
	{
		free(bufUnit->data);
		bufUnit->capacity = buf_size;
		bufUnit->data = (char*)malloc(bufUnit->capacity);
	}

	return ;
}


void bufUnit_destroy(BufUnit *bufUnit)
{
	if(bufUnit)
	{
		pthread_mutex_destroy(&bufUnit->mutex);
		if(bufUnit->data)
			free(bufUnit->data);
		if(bufUnit->info)
			readUnit_destroy(bufUnit->info);

		free(bufUnit);
	}
}

int readBuf_init(ReadBuf *readBuf)
{
	if(readBuf)
	{
		int stat;
		
		if((stat = pthread_cond_init(&readBuf->fetch_cond, NULL)) != 0)
		{
			return stat;
		}
		if((stat = pthread_cond_init(&readBuf->send_cond, NULL)) != 0)
		{
			pthread_cond_destroy(&readBuf->fetch_cond);
			return stat;
		}
		if((stat = workq_init(&readBuf->worker, 1, readBuf_handler)) != 0)
		{
			pthread_cond_destroy(&readBuf->fetch_cond);
			pthread_cond_destroy(&readBuf->send_cond);
			return stat;
		}

		readBuf->fetch_index= 0;
		readBuf->send_index = 0;
		readBuf->fetch_wait = 0;
		readBuf->send_wait = 0;

		readBuf->buffer_number = MAX_UNIT_NUMBER;
		readBuf->buffer_capacity = MAX_UNIT_SIZE;

		readBuf->units = (BufUnit**)malloc(sizeof(BufUnit*) * MAX_UNIT_NUMBER);
		memset(readBuf->units, 0, sizeof(BufUnit*) * MAX_UNIT_NUMBER);

		readBuf->valid = READBUF_VALID;

		return 0;
	}

	return -1;
}

int readBuf_destroy(ReadBuf *readBuf)
{
	if(readBuf->valid != READBUF_VALID)
		return EINVAL;

	readBuf->valid = 0;

	if(workq_destroy(&readBuf->worker))
	{
		fprintf(stderr, "%s,%d: workq_destroy() failure in readBuf_destroy()\n");
		return -1;
	}

	int i = 0;
	while(i < readBuf->buffer_number)
	{
		bufUnit_destroy(*(readBuf->units+i));
		++i;
	}
	
	free(readBuf->units);
	pthread_cond_destroy(&readBuf->fetch_cond);
	pthread_cond_destroy(&readBuf->send_cond);

	return 0;
}

int readBuf_append(ReadBuf *readBuf, ReadUnit *readUnit)
{
	int stat = 0;	

	if(readBuf && readUnit)
	{
		stat = workq_add(&readBuf->worker, readUnit, NULL, 0);
	}

	return stat;
}

void* readBuf_handler(void *arg)
{	
	ReadBuf* readBuf = &read_buf_queue;
	ReadUnit* readUnit = (ReadUnit*)arg;

	BufUnit** buffers = readBuf->units;
	uint16_t index = readBuf->fetch_index;

	if(!buffers[index])
		buffers[index] = bufUnit_new(readUnit->length);

	pthread_mutex_lock(&buffers[index]->mutex);

	if(buffers[index]->info && buffers[index]->info->lifetime)
	{
		readBuf->fetch_wait += 1;
		if(pthread_cond_wait(&readBuf->fetch_cond, &buffers[index]->mutex) != 0)
		{
			pthread_mutex_unlock(&buffers[index]->mutex);
			return NULL;
		}
		readBuf->fetch_wait -= 1;
	}

	if(readUnit->length > buffers[index]->capacity)
	{
		bufUnit_resize(buffers[index], readUnit->length);
	}

	fseek(readUnit->fp, readUnit->offset, SEEK_SET);
	fread(buffers[index]->data, readUnit->length, 1, readUnit->fp);
	readBuf->fetch_index = (readBuf->fetch_index + 1) % readBuf->buffer_number;

	free(buffers[index]->info);
	buffers[index]->info = readUnit;

	if(readBuf->send_wait)
	{
		if(pthread_cond_broadcast(&readBuf->send_cond) != 0)
		{
			pthread_mutex_unlock(&buffers[index]->mutex);
			return NULL;
		}
	}

	pthread_mutex_unlock(&buffers[index]->mutex);

	return NULL;	
}

int readBuf_get(ReadBuf *readBuf, char *data, uint64_t offset, uint32_t length)
{
	if(!readBuf || !data)
		return -1;

	int index = readBuf->send_index;
	BufUnit** buffers = readBuf->units;

	pthread_mutex_lock(&buffers[index]->mutex);

	if(readBuf->fetch_wait && readBuf->fetch_index - readBuf->send_index > 0)
	{
		if(pthread_cond_broadcast(&readBuf->fetch_cond) != 0)
		{
			pthread_mutex_unlock(&buffers[index]->mutex);
			return NULL;
		}
	}

	while(0 == buffers[index]->info->lifetime)
	{	
		readBuf->send_wait += 1; 
		if(pthread_cond_wait(&readBuf->send_cond, &buffers[index]->mutex) != 0)
		{
			pthread_mutex_unlock(&buffers[index]->mutex);
			return NULL;
		}
		readBuf->send_wait -= 1;	
	}

	memcpy(data, buffers[index]->data + offset - buffers[index]->info->offset, length);
	buffers[index]->info->lifetime -= 1;

	if(0 == buffers[index]->info->lifetime)
    {
		readBuf->send_index = (readBuf->send_index + 1) % readBuf->buffer_number;
         
	    if(readBuf->fetch_wait)
	        pthread_cond_broadcast(&readBuf->fetch_cond);
    }

	pthread_mutex_unlock(&buffers[index]->mutex);

	return 0;
}
