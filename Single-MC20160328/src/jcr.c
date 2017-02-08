#include "../global.h"

JCR* jcr_new() {
	JCR* psJcr = (JCR*) malloc(sizeof(JCR));

	if (pthread_mutex_init(&psJcr->mutex, NULL) != 0) {
		free(psJcr);
		return NULL;
	}
	
	psJcr->first_file = NULL;
	psJcr->last_file = NULL;
	
	psJcr->total_file_number = 0;
	psJcr->deduped_file_number = 0;
	psJcr->total_file_size = 0;
	
	psJcr->total_chunk_number = 0;
	psJcr->deduped_chunk_number = 0;
	psJcr->deduped_chunk_size = 0;

	psJcr->cur_date = time(0);
	psJcr->decompress = 0; 

    psJcr->migrate_file_size = 0;
	psJcr->migrate_head_size = 0;
	psJcr->compressed_file_size = 0; 
	
	psJcr->total_time = 0;
	psJcr->ae_time= 0;
	psJcr->SHA_time=0;
	psJcr->dedup_time= 0;
	psJcr->SF_time = 0;
	psJcr->content_reorganize_time = 0;	
	psJcr->compress_time = 0; 
	
	psJcr->simhash_calcu_time = 0;	
	psJcr->simhash_detect_time = 0; 
	
	psJcr->decompress_time = 0;
	psJcr->restore_time = 0; 
	
	memset(psJcr->compress_algorithm, 0, 20);
	memset(psJcr->cmdline, 0, 100);
	
	psJcr->folder_capacity = 5;
	psJcr->folder_path = (char**)malloc(sizeof(char*) * psJcr->folder_capacity);
	psJcr->folder_count = 0;

	psJcr->compress_sequence = NULL;
	psJcr->compresse_filename = NULL;
	
	return psJcr;

}

void jcr_destroy(JCR* jcr)
{
	int i;
	FileInfo *file, *next;

	if(jcr)
	{
		file = jcr->first_file;
		while(file)
		{
			next = file->next;
			fileInfo_destroy(file);
			file = next;
		}
	
		if(jcr->folder_path)
		{
			for(i = 0; i < jcr->folder_count; ++i)
			{
				free(*(jcr->folder_path + i));
			}
			
			free(jcr->folder_path);
		}
			
		if(jcr->compress_sequence)
			free(jcr->compress_sequence);
		if(jcr->compresse_filename)
			free(jcr->compresse_filename);
		free(jcr);
	}
}


bool jcr_append_path(JCR* jcr, char* path)
{
	struct stat state;

	if(!path)  return false;

	if(access(path, F_OK) != 0)
    {
        fprintf(stderr, "%s,%d: can not access %s.\n", __FILE__, __LINE__, path);
		return false;
    }

    if(stat(path, &state) != 0 )
    {
        fprintf(stderr, "%s,%d: %s does not exist.\n",  __FILE__, __LINE__, path);
        return false;
    }

	if(jcr->folder_count == jcr->folder_capacity)
	{
		if(!grow_jcr_folder_capacity(jcr))
			return false;
	}

	char** pName = jcr->folder_path + jcr->folder_count;
	
	*pName = (char *)malloc(strlen(path) + 1);
	memset(*pName, 0, strlen(path) + 1);
	strcpy(*pName, path);
	
	jcr->folder_count += 1;
	
	return true;
	
}

bool grow_jcr_folder_capacity(JCR* jcr)
{
	int i;
	char** folder_path = NULL;

	folder_path = (char**)malloc(sizeof(char*) * jcr->folder_capacity * 2);
	if(!folder_path)
	{
		fprintf(stderr, "%s,%d: grow jcr_folder_capacity failed\n", __FILE__, __LINE__);
		return false;
	}

	jcr->folder_capacity = jcr->folder_capacity * 2;
	for(i = 0; i < jcr->folder_count; ++i)
	{
		*(folder_path + i) = *(jcr->folder_path + i);
	}

	free(jcr->folder_path);
	jcr->folder_path = folder_path;

	return true;
}

bool jcr_append_file(JCR* jcr, FileInfo *file)
{
	if(!jcr || !file)
		return false;

	if(!jcr->first_file)
	{
		jcr->first_file = file;
		jcr->last_file = file;
	}
	else
	{
		jcr->last_file->next = file;
		jcr->last_file = file;
	}
	jcr->total_file_number += 1;

	return true;
}

void update_jcr_info(JCR* jcr)
{
	if(jcr)
	{
		FileInfo *file = jcr->first_file;
		while(file)
		{
			jcr->total_chunk_number += file->total_chunk_number;
			jcr->total_file_size += file->total_chunk_size;
			jcr->deduped_chunk_number += file->deduped_chunk_number;
			jcr->deduped_chunk_size += file->deduped_chunk_size;
			
			file = file->next;
		}
	}
}

char* get_compresse_filename(JCR* jcr)
{
	char ch;
	char* ps;
	int len, cnt, newlen;
	bool err = false;
	FILE* fp = NULL;

	if(jcr->folder_count == 0)
	{
		return NULL;
	}
	else if(jcr->folder_count == 1)
	{
		len = strlen(jcr->folder_path[0]);
		cnt = 0;

		ps = jcr->folder_path[0] + len - 1;
		ch = *ps;
		while(*ps == '/' || *ps == '\\')
		{
			*ps = '\0';
			--ps;
			++cnt;
		}

        ps = strrchr(jcr->folder_path[0], '/');
        if(!ps)
        {
            ps = strrchr(jcr->folder_path[0], '\\');
        }

        if(!ps)
            ps = jcr->folder_path[0];
        else
            ++ps;

		if(strlen(ps) == 0)
		{
			fprintf(stderr, "%s,%d: %s -> generate_compress_file_name failed\n", __FILE__, __LINE__, jcr->folder_path[0]);
			err = true;
		}
		else
		{			
			newlen = strlen(ps) + strlen(compress_path) + 1;
			jcr->compresse_filename = (char*)malloc(newlen);
	
			memset(jcr->compresse_filename, 0, newlen);
			memcpy(jcr->compresse_filename, compress_path, strlen(compress_path));
			memcpy(jcr->compresse_filename + strlen(compress_path), ps, strlen(ps));
		}		
		
		ps = jcr->folder_path[0] + len - 1;
		while(cnt > 0)
		{
			*(ps--) = ch;
			--cnt;
		}		
	}
	else
	{
		newlen = 2 + strlen(compress_path) + 1;
		jcr->compresse_filename = (char*)malloc(newlen);

		memset(jcr->compresse_filename, 0, newlen);
		memcpy(jcr->compresse_filename, compress_path, strlen(compress_path));
		memcpy(jcr->compresse_filename + strlen(compress_path), "mc", 2);
	}
	
	return jcr->compresse_filename;
}


