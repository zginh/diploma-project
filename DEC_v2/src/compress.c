#include "../global.h"

void update_all_file_recipe(JCR *jcr)
{
	workq_t recipe_queue;
	FileInfo *file;
	
	workq_init(&recipe_queue, 1, update_recipe_offset_handler);

	file = jcr->first_file;
	while(file)
	{
		workq_add(&recipe_queue, file->rp, NULL, 0);
		file = file->next;
	}

	workq_destroy(&recipe_queue);	
}

void* update_recipe_offset_handler(void* arg)
{
	size_t read_len;

	Recipe* rp = (Recipe*)arg;

	if(rp == NULL || rp->total_chunks == 0)  return NULL;

	FingerChunk *elem = rp->first; 
	while(elem)
	{
		DedupIndexEntry *entry = dedup_index_lookup(elem->chunk_hash);
		
		if(!entry)
		{
			fprintf(stderr, "%s,%d: Fingerprint_Offset Pair Read fail.\n", __FILE__, __LINE__);
		  	return NULL;
		}
		memcpy(&(elem->offset), &(entry->chunk_offset), sizeof(elem->offset));
		elem = elem->next;
	}

	return NULL;
}

bool content_reorganize(JCR *jcr)
{
	int src_fd, dest_fd;
	uint32_t read_offset = 0;
	int32_t read_bytes = 0;
	uint64_t head_bytes = 0;
	uint64_t write_bytes = 0;
	uint64_t chunk_offset = 0;
	uint32_t chunk_length = 0; 
	uint32_t chunk_counts = 0;
	uint32_t metadata_len = 0;
	uint16_t filename_length = 0;
	char in_buf[MAX_BUF_SIZE];
	char out_buf[MAX_BUF_SIZE];
	char cmdline[256];
	
	FILE *compressed_fp = fopen(get_compresse_filename(jcr), "w");
	
	memcpy(out_buf, &MC_VALID, sizeof(MC_VALID));
	write_bytes += sizeof(MC_VALID);
	memcpy(out_buf + write_bytes, &(jcr->total_file_number), sizeof(jcr->total_file_number));
	write_bytes += sizeof(jcr->total_file_number);
	
    update_all_file_recipe(jcr);

	FileInfo *file;
	FingerChunk* elem;

	int offset_bytes = sizeof(elem->offset);
	int length_bytes = sizeof(elem->chunk_size); 
	
	file = jcr->first_file;
	while(file)
	{
		filename_length = strlen(file->filename);		
		metadata_len = sizeof(filename_length) + filename_length + sizeof(file->total_chunk_size) + sizeof(file->total_chunk_number);

		if(write_bytes + metadata_len > MAX_BUF_SIZE)
		{
			fwrite(out_buf, write_bytes, 1, compressed_fp);
            head_bytes += write_bytes;
 			write_bytes = 0;
		}
		
		memcpy(out_buf + write_bytes, &filename_length, sizeof(filename_length));
		write_bytes += sizeof(filename_length);
		memcpy(out_buf + write_bytes, file->filename, filename_length);
		write_bytes += filename_length;
		memcpy(out_buf + write_bytes, &file->total_chunk_size, sizeof(file->total_chunk_size));
		write_bytes += sizeof(file->total_chunk_size);
		memcpy(out_buf + write_bytes, &file->total_chunk_number, sizeof(file->total_chunk_number));
		write_bytes += sizeof(file->total_chunk_number);
		
		elem = file->rp->first;
		
		while(elem)
		{
			if(write_bytes + offset_bytes + length_bytes > MAX_BUF_SIZE)
			{
				fwrite(out_buf, write_bytes, 1, compressed_fp);
                head_bytes += write_bytes;
				write_bytes = 0;
			}

			memcpy(out_buf + write_bytes, &elem->offset, offset_bytes);
			write_bytes += offset_bytes;
			memcpy(out_buf + write_bytes, &elem->chunk_size, length_bytes);
			write_bytes += length_bytes;
			
	        elem = elem->next;
		}
		
		file = file->next;
	}
	
    head_bytes += write_bytes;
    head_bytes += sizeof(head_bytes);	
	jcr->migrate_head_size = head_bytes;
    jcr->migrate_file_size = head_bytes;
	
	if(write_bytes + sizeof(head_bytes) > MAX_BUF_SIZE)
	{
		fwrite(out_buf, write_bytes, 1, compressed_fp);
 		write_bytes = 0;
	}

	memcpy(out_buf + write_bytes, &head_bytes, sizeof(head_bytes));
	write_bytes += sizeof(head_bytes);

	FILE* read_fp;
	FILE* curr_fp;
	char* compress_ptr = jcr->compress_sequence;
	
	chunk_counts = jcr->deduped_chunk_number;

	if(simlist_number!=chunk_counts)
		fprintf(stderr,"number wrong");
	
	while(chunk_counts)
	{
		memcpy(&chunk_offset, compress_ptr, offset_bytes);
		compress_ptr += offset_bytes;
		memcpy(&chunk_length, compress_ptr, length_bytes);
		compress_ptr += length_bytes;
		memcpy(&read_fp, compress_ptr, sizeof(read_fp));
		compress_ptr += sizeof(read_fp);

		if(write_bytes + chunk_length > MAX_BUF_SIZE)
		{
			fwrite(out_buf, write_bytes, 1, compressed_fp); 
			write_bytes = 0;
		}

		#ifdef _READ_THREAD_
		readBuf_get(&read_buf_queue, out_buf + write_bytes, chunk_offset, chunk_length);
		#else
		if((read_bytes == 0) || (curr_fp != read_fp) || !((chunk_offset >= read_offset) && ((chunk_offset + chunk_length) <= (read_offset + read_bytes))))
		{
			fseek(read_fp, chunk_offset, SEEK_SET);
			read_offset = chunk_offset;
            curr_fp = read_fp;
			read_bytes = fread(in_buf, 1, MAX_BUF_SIZE, curr_fp);
            //jcr->migrate_read_times += 1;
		}

		memcpy(out_buf + write_bytes, in_buf + chunk_offset - read_offset, chunk_length);
		#endif
		write_bytes += chunk_length;

        jcr->migrate_file_size += chunk_length;
		--chunk_counts;
		
	}
	
	if(write_bytes > 0)
    {
		fwrite(out_buf, write_bytes, 1, compressed_fp);
		write_bytes = 0;
    } 
	
	fclose(compressed_fp);
	
	memset(cmdline, 0, 256);
	sprintf(cmdline, "rm -rf %s*", temp_path);
	execLinuxCmd(cmdline);

	return true;
}

bool content_reorganize2(JCR *jcr)
{
	int src_fd, dest_fd;
	uint32_t read_offset = 0;
	int32_t read_bytes = 0;
	uint64_t head_bytes = 0;
	uint64_t write_bytes = 0;
	uint64_t chunk_offset = 0;
	uint32_t chunk_length = 0; 
	uint32_t chunk_counts = 0;
	uint32_t metadata_len = 0;
	uint16_t filename_length = 0;
	char in_buf[MAX_BUF_SIZE];
	char out_buf[MAX_BUF_SIZE];
	char cmdline[256];
		
	FILE *compressed_fp = fopen("./tmp/GCC_2", "w");
	
	memcpy(out_buf, &MC_VALID, sizeof(MC_VALID));
	write_bytes += sizeof(MC_VALID);
	memcpy(out_buf + write_bytes, &(jcr->total_file_number), sizeof(jcr->total_file_number));
	write_bytes += sizeof(jcr->total_file_number);
	
    update_all_file_recipe(jcr);

	FileInfo *file;
	FingerChunk* elem;

	int offset_bytes = sizeof(elem->offset);
	int length_bytes = sizeof(elem->chunk_size); 
	
	file = jcr->first_file;
	while(file)
	{
		filename_length = strlen(file->filename);		
		metadata_len = sizeof(filename_length) + filename_length + sizeof(file->total_chunk_size) + sizeof(file->total_chunk_number);

		if(write_bytes + metadata_len > MAX_BUF_SIZE)
		{
			fwrite(out_buf, write_bytes, 1, compressed_fp);
            head_bytes += write_bytes;
 			write_bytes = 0;
		}
		
		memcpy(out_buf + write_bytes, &filename_length, sizeof(filename_length));
		write_bytes += sizeof(filename_length);
		memcpy(out_buf + write_bytes, file->filename, filename_length);
		write_bytes += filename_length;
		memcpy(out_buf + write_bytes, &file->total_chunk_size, sizeof(file->total_chunk_size));
		write_bytes += sizeof(file->total_chunk_size);
		memcpy(out_buf + write_bytes, &file->total_chunk_number, sizeof(file->total_chunk_number));
		write_bytes += sizeof(file->total_chunk_number);
		
		elem = file->rp->first;
		
		while(elem)
		{
			if(write_bytes + offset_bytes + length_bytes > MAX_BUF_SIZE)
			{
				fwrite(out_buf, write_bytes, 1, compressed_fp);
                head_bytes += write_bytes;
				write_bytes = 0;
			}

			memcpy(out_buf + write_bytes, &elem->offset, offset_bytes);
			write_bytes += offset_bytes;
			memcpy(out_buf + write_bytes, &elem->chunk_size, length_bytes);
			write_bytes += length_bytes;
			
	        elem = elem->next;
		}
		
		file = file->next;
	}
	
    head_bytes += write_bytes;
    head_bytes += sizeof(head_bytes);	
	jcr->migrate_head_size = head_bytes;
    jcr->migrate_file_size = head_bytes;
	
	if(write_bytes + sizeof(head_bytes) > MAX_BUF_SIZE)
	{
		fwrite(out_buf, write_bytes, 1, compressed_fp);
 		write_bytes = 0;
	}

	memcpy(out_buf + write_bytes, &head_bytes, sizeof(head_bytes));
	write_bytes += sizeof(head_bytes);

	FILE* read_fp;
	FILE* curr_fp;
	char* compress_ptr = jcr->compress_sequence;
	
	chunk_counts = jcr->deduped_chunk_number;
	
	while(chunk_counts)
	{
		memcpy(&chunk_offset, compress_ptr, offset_bytes);
		compress_ptr += offset_bytes;
		memcpy(&chunk_length, compress_ptr, length_bytes);
		compress_ptr += length_bytes;
		memcpy(&read_fp, compress_ptr, sizeof(read_fp));
		compress_ptr += sizeof(read_fp);

		if(write_bytes + chunk_length > MAX_BUF_SIZE)
		{
			fwrite(out_buf, write_bytes, 1, compressed_fp); 
			write_bytes = 0;
		}
		
		if((read_bytes == 0) || (curr_fp != read_fp) || !((chunk_offset >= read_offset) && ((chunk_offset + chunk_length) <= (read_offset + read_bytes))))
		{
			fseek(read_fp, chunk_offset, SEEK_SET);
			read_offset = chunk_offset;
            curr_fp = read_fp;
			read_bytes = fread(in_buf, 1, MAX_BUF_SIZE, curr_fp);
		}

		memcpy(out_buf + write_bytes, in_buf + chunk_offset - read_offset, chunk_length);
		write_bytes += chunk_length;

		--chunk_counts;
	}

	if(write_bytes > 0)
    {
		fwrite(out_buf, write_bytes, 1, compressed_fp);
		write_bytes = 0;
    } 
	
	fclose(compressed_fp);
	
	memset(cmdline, 0, 256);
	sprintf(cmdline, "rm -rf %s*", temp_path);
	execLinuxCmd(cmdline);

	return true;
}

void final_compress(JCR* jcr)
{ 	
	int status;
    char cmdline[256];
 
    memset(cmdline, 0, 256);

	int len = strlen(jcr->compresse_filename);
	char* filename = (char*)malloc(len + 6);	
	memset(filename, 0, len + 6);

	if(memcmp(jcr->compress_algorithm, "7z", 2) == 0)
	{
		sprintf(cmdline, "%s a %s.%s %s > /dev/null", jcr->compress_algorithm, jcr->compresse_filename, jcr->compress_algorithm, jcr->compresse_filename);
		execLinuxCmd(cmdline);

		memset(cmdline, 0, 256);
		sprintf(cmdline, "rm %s", jcr->compresse_filename);
		execLinuxCmd(cmdline);

		sprintf(filename, "%s.7z", jcr->compresse_filename);	
	}
	else if(memcmp(jcr->compress_algorithm, "lha", 3) == 0)
	{
		sprintf(cmdline, "%s a %s.%s %s > /dev/null", jcr->compress_algorithm, jcr->compresse_filename, jcr->compress_algorithm, jcr->compresse_filename);
		execLinuxCmd(cmdline);

		memset(cmdline, 0, 256);
		sprintf(cmdline, "rm %s", jcr->compresse_filename);
		execLinuxCmd(cmdline);

		sprintf(filename, "%s.lha", jcr->compresse_filename);
	}
	else if(memcmp(jcr->compress_algorithm, "rar", 3) == 0)
	{
		sprintf(cmdline, "%s a %s.%s %s > /dev/null", jcr->compress_algorithm, jcr->compresse_filename, jcr->compress_algorithm, jcr->compresse_filename);
		execLinuxCmd(cmdline);

		memset(cmdline, 0, 256);
		sprintf(cmdline, "rm %s", jcr->compresse_filename);
		execLinuxCmd(cmdline);

		sprintf(filename, "%s.rar", jcr->compresse_filename);
	}
	else if(memcmp(jcr->compress_algorithm, "zip", 3) == 0)
	{
		sprintf(cmdline, "%s %s.zip %s > /dev/null", jcr->compress_algorithm, jcr->compresse_filename, jcr->compresse_filename);
		execLinuxCmd(cmdline);

		memset(cmdline, 0, 256);
		sprintf(cmdline, "rm %s", jcr->compresse_filename);
		execLinuxCmd(cmdline);

		sprintf(filename, "%s.zip", jcr->compresse_filename);
	}
	else if(memcmp(jcr->compress_algorithm, "lz4", 3) == 0)
	{
		sprintf(cmdline, "./%s %s", jcr->compress_algorithm, jcr->compresse_filename);
		execLinuxCmd(cmdline);

		memset(cmdline, 0, 256);
		sprintf(cmdline, "rm %s", jcr->compresse_filename);
		execLinuxCmd(cmdline);

		sprintf(filename, "%s.lz4", jcr->compresse_filename);
	}
	else
	{
		sprintf(cmdline, "%s %s > /dev/null", jcr->compress_algorithm, jcr->compresse_filename);
		execLinuxCmd(cmdline);

		if(memcmp(jcr->compress_algorithm, "gzip", 4) == 0)
		{
			sprintf(filename, "%s.gz", jcr->compresse_filename);
		}
		else if(memcmp(jcr->compress_algorithm, "bzip2", 5) == 0)
		{
			sprintf(filename, "%s.bz2", jcr->compresse_filename);
		}
	}

	jcr->compressed_file_size = get_file_size(filename);
	free(filename);

}

void DestFileRename(char* tmpfile, JCR* jcr)
{
    int status;	
	char cmdline[256];
	char srcFile[FILE_NAME_LEN];
	char destFile[FILE_NAME_LEN];
	
    int tmpLen = strlen(tmpfile);
	if(tmpfile == NULL || tmpLen == 0)
	{
		fprintf(stderr, "%s,%d: invalid tmpfile.\n", __FILE__, __LINE__); 
	}

	memset(srcFile, 0, FILE_NAME_LEN);
	memset(destFile, 0, FILE_NAME_LEN);
    sprintf(srcFile, "%s.gz", tmpfile);
    sprintf(destFile, "./%s.mgz", tmpfile + 6); 	
    
    memset(cmdline, 0, 256);
    sprintf(cmdline, "mv %s %s", srcFile, destFile);

    execLinuxCmd(cmdline);
	
}
 
