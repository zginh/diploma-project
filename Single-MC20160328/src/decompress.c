#include "../global.h"

RestoreFile* restoreFile_new()
{
	RestoreFile* rf = (RestoreFile*)malloc(sizeof(RestoreFile));
	if(!rf)
	{
		return NULL;
	}

	rf->filename = NULL;
	rf->file_size = 0;
	rf->chunk_count = 0;
	rf->offset_size_pairs = NULL;
	rf->next = NULL;

	return rf;
}

void restoreFile_destroy(RestoreFile* rf)
{
	if(rf)
	{
		free(rf->filename);
		free(rf->offset_size_pairs);
		free(rf);
	}
}

RestoreFileList* restoreFileList_new()
{
	RestoreFileList* rflist = (RestoreFileList*)malloc(sizeof(RestoreFileList));
	if(!rflist)
	{
		return NULL;
	}

	rflist->file_count = 0;
	rflist->first = NULL;
	rflist->last = NULL;

    return rflist;
}

void restoreFileList_destroy(RestoreFileList* rflist)
{
	RestoreFile *cur, *next;
	
	if(!rflist)
	{
		cur = rflist->first;
		while(cur)
		{
			next = cur->next;
			restoreFile_destroy(cur);
			cur = next;
		}
		free(rflist);
	}
}

bool append_new_restoreFile(RestoreFileList* rflist, RestoreFile* rf)
{
	if(!rflist || !rf)
		return false;

	if(!rflist->first)
	{
		rflist->first = rf;		
	}
	else
	{
		rflist->last->next = rf;		
	}

	rflist->last = rf;
	rflist->file_count += 1;

	return true;
}

void* file_decompress_handler(void* arg)
{
	TIMER_DECLARE(2);
	char *sfile, *dfile; 
	char cmdline[256];
	uint64_t data_offset;
	RestoreFileList* pList;
	
	if(!arg)
		return NULL;

    sfile = (char *)arg;

	dfile = generate_cmdline(cmdline, 256, sfile);	
	
	if(!dfile)
	{
		return NULL;
	}

	TIMER_BEGIN(2);
	execLinuxCmd(cmdline);
	TIMER_END(2,jcr->decompress_time);

	TIMER_BEGIN(2);
	pList = restoreFileList_new();

	bool ret = parse_metadata(dfile, pList, &data_offset);

	if(ret)
	{
		file_restore(dfile, pList, data_offset);
	}
	else
	{
		fprintf(stderr, "%s,%d: metadata parse error\n", __FILE__, __LINE__);
	}

	TIMER_END(2,jcr->restore_time);
	memset(cmdline, 0, 256);
	sprintf(cmdline, "rm -rf tmp/*");
	execLinuxCmd(cmdline);

	restoreFileList_destroy(pList);
	
	return NULL;
}


char* generate_cmdline(char cmdline[], int len, char* cfile)
{
	FILE* fp;
	char alg_valid;
	char *cptr, *dptr, *retfile;

	assert(cfile != NULL);

	cptr = strrchr(cfile, '/');

	if(cptr)
		++cptr;
	else
		cptr = cfile;
	
	retfile = (char*)malloc(strlen(temp_path) + strlen(cptr) + 1);
	memcpy(retfile, temp_path, strlen(temp_path));
	memcpy(retfile + strlen(temp_path), cptr, strlen(cptr) + 1);

	if(!(fp = fopen(cfile, "r")))
	{
		fprintf(stderr, "%s,%d: %s open error\n", cfile);
		return NULL;
	}

	fread(&alg_valid, 1, 1, fp); 
	fclose(fp);

	dptr = strrchr(retfile, '.');
	if(!memcmp(dptr,".gz", 3))
	{
		if(alg_valid == 0x1f )
		{
			sprintf(cmdline, "cp %s %s", cfile, temp_path);
			execLinuxCmd(cmdline);
			
			memset(cmdline, 0, 256);
			sprintf(cmdline, "gzip -d %s", retfile);

			*dptr = '\0';
		}
		else
		{
			fprintf(stderr, "gzip: %s: not in gzip format\n", cfile);
			return NULL;
		}		
	}
	else if(!memcmp(dptr,".bz2", 4))
	{
		if(alg_valid == 0x42 )
		{
			sprintf(cmdline, "cp %s %s", cfile, temp_path);
			execLinuxCmd(cmdline);

			memset(cmdline, 0, 256);
			sprintf(cmdline, "bzip2 -d %s", retfile);

			*dptr = '\0';
		}
		else
		{
			fprintf(stderr, "bzip2: %s: not in bzip2 format.\n", cfile);
			return NULL;
		}
	}
	else if(!memcmp(dptr,".7z", 3))
	{
		if(alg_valid == 0x37 )
		{
			*dptr = '\0';
			sprintf(cmdline, "7z x %s -o%s", cfile, temp_path);
			
		}
		else
		{
			fprintf(stderr, "7z: %s: not in 7z format.\n", cfile);
			return NULL;
		}
	}
	else if(!memcmp(dptr,".rar", 4))
	{
		if(alg_valid == 0x52 )
		{
			*dptr = '\0';
			sprintf(cmdline, "unrar e %s %s", cfile, temp_path);
		}
		else
		{
			fprintf(stderr, "rar: %s: not in rar format.\n", cfile);
			return NULL;
		}
	} 
	else if(!memcmp(dptr,".lz4", 4))
	{
		if(alg_valid == 0x04 )
		{
			*dptr = '\0';
			sprintf(cmdline, "./lz4 -d %s %s", cfile, retfile);
		}
		else
		{
			fprintf(stderr, "lz4: %s: not in lz4 format.\n", cfile);
			return NULL;
		}
	} 	
	else
	{
		fprintf(stderr, "%s: unknown suffix -- ignored.\n");
		fprintf(stderr, "PS: support suffix: *.gz, *.bz2, *.7z, *.rar, *.lz4\n");
		return NULL;
	} 

	return retfile;
}

bool parse_metadata(char* dfile, RestoreFileList* pList, uint64_t* data_offset)
{
	FILE* fp;
	uint16_t filename_length;
	uint32_t metadata_length;
	uint32_t mc_flag;
	uint32_t file_count;
	uint32_t read_offset;
	uint32_t read_bytes;
	uint32_t left_bytes; 
	uint64_t chunk_counts;
	RestoreFile* rf;
	FingerChunk* chunk;
	
	char *in_buf, *out_buf;

	if(!(in_buf = (char*)malloc(MAX_BUF_SIZE)))
	{
		fprintf(stderr, "%s,%d: malloc failed\n", __FILE__, __LINE__);
		return false;
	}
	if(!(out_buf = (char*)malloc(MAX_BUF_SIZE)))
	{
		fprintf(stderr, "%s,%d: malloc failed\n", __FILE__, __LINE__);
		free(in_buf);
		return false;
	}
	if(!(fp = fopen(dfile, "r")))
	{
		fprintf(stderr, "%s,%d: %s open failed\n", __FILE__, __LINE__, dfile);
		free(in_buf);
		free(out_buf);
		return false;
	}

	fread(&mc_flag, sizeof(mc_flag), 1, fp);

	if(mc_flag != MC_VALID)
	{
		fprintf(stderr, "%s,%d: invalid valid bits\n", __FILE__, __LINE__);
		free(in_buf);
		free(out_buf);
		fclose(fp);
		return false;
	}

	fread(&file_count, sizeof(file_count), 1, fp); 
	
	read_offset = 0;
	read_bytes = 0;
	left_bytes = 0;
	while(file_count)
	{
		rf = restoreFile_new();
	
		if(left_bytes < sizeof(filename_length))
		{
			memcpy(in_buf, in_buf + read_offset, left_bytes);
			read_bytes = fread(in_buf + left_bytes, 1, MAX_BUF_SIZE - left_bytes, fp);

			left_bytes += read_bytes;
			read_offset = 0; 
		}

		memcpy(&filename_length, in_buf + read_offset, sizeof(filename_length));
		read_offset += sizeof(filename_length);
		left_bytes -= sizeof(filename_length);

		if(left_bytes < filename_length)
		{
			memcpy(in_buf, in_buf + read_offset, left_bytes);
			read_bytes = fread(in_buf + left_bytes, 1, MAX_BUF_SIZE - left_bytes, fp);

			left_bytes += read_bytes;
			read_offset = 0; 
		}

		rf->filename = (char*)malloc(filename_length + 1);
		memset(rf->filename, 0, filename_length + 1);
		memcpy(rf->filename ,in_buf + read_offset, filename_length);
		read_offset += filename_length;
		left_bytes -= filename_length;

		if(left_bytes < sizeof(rf->file_size))
		{
			memcpy(in_buf, in_buf + read_offset, left_bytes);
			read_bytes = fread(in_buf + left_bytes, 1, MAX_BUF_SIZE - left_bytes, fp);

			left_bytes += read_bytes;
			read_offset = 0; 
		}

		memcpy(&(rf->file_size), in_buf + read_offset, sizeof(rf->file_size));
		read_offset += sizeof(rf->file_size);
		left_bytes -= sizeof(rf->file_size);

        total_bytes += rf->file_size;

		if(left_bytes < sizeof(rf->chunk_count))
		{
			memcpy(in_buf, in_buf + read_offset, left_bytes);
			read_bytes = fread(in_buf + left_bytes, 1, MAX_BUF_SIZE - left_bytes, fp);

			left_bytes += read_bytes;
			read_offset = 0; 
		}

		memcpy(&(rf->chunk_count), in_buf + read_offset, sizeof(rf->chunk_count));
		read_offset += sizeof(rf->chunk_count);
		left_bytes -= sizeof(rf->chunk_count);

		uint64_t copy_bytes = (rf->chunk_count) * (sizeof(chunk->offset)+sizeof(chunk->chunk_size));
		rf->offset_size_pairs = (char*)malloc(copy_bytes);  
		
		uint64_t copy_offset = 0;
		while(copy_bytes)
		{
			if(left_bytes < copy_bytes)
			{
				memcpy(rf->offset_size_pairs + copy_offset, in_buf + read_offset, left_bytes);

				copy_offset += left_bytes;
				copy_bytes -= left_bytes;
				
				read_bytes = fread(in_buf, 1, MAX_BUF_SIZE, fp);

				left_bytes = read_bytes;
				read_offset = 0; 
			}
			else
			{
				memcpy(rf->offset_size_pairs + copy_offset, in_buf + read_offset, copy_bytes);

				read_offset += copy_bytes;
				left_bytes -= copy_bytes;
                copy_bytes = 0;
			}  
		}

		append_new_restoreFile(pList,rf);
		
		--file_count;		
	}
	
	if(left_bytes < sizeof(uint64_t))
	{
		memcpy(in_buf, in_buf + read_offset, left_bytes);
		read_bytes = fread(in_buf + left_bytes, 1, sizeof(uint64_t), fp);
		left_bytes += read_bytes;
		read_offset = 0; 
	}
	memcpy(data_offset, in_buf + read_offset, sizeof(uint64_t));

	free(in_buf);
	free(out_buf);
	fclose(fp);

	return true;
}


bool file_restore(char* dfile, RestoreFileList* pList, uint64_t data_offset)
{
	uint32_t file_count;
	uint64_t chunk_count; 
	uint64_t chunk_offset;
	uint32_t chunk_length;
	uint64_t read_offset;
	uint32_t read_bytes;
	uint32_t write_bytes;
	bool overwrite_flag = false;
	char* recipe_ptr;
	char* filename;
	FILE *isfp, *osfp;
	RestoreFile* rf;

	if((file_count = pList->file_count) == 0)
	{
		return false;
	}

	char *in_buf, *out_buf;

	if(!(in_buf = (char*)malloc(MAX_BUF_SIZE)))
	{
		fprintf(stderr, "%s,%d: malloc failed\n", __FILE__, __LINE__);
		return false;
	}
	if(!(out_buf = (char*)malloc(MAX_BUF_SIZE)))
	{
		fprintf(stderr, "%s,%d: malloc failed\n", __FILE__, __LINE__);
		free(in_buf);
		return false;
	}
	if(!(isfp = fopen(dfile, "r")))
	{
		fprintf(stderr, "%s,%d: %s open failed\n", __FILE__, __LINE__, dfile);
		free(in_buf);
		free(out_buf);
		return false;
	}

	read_offset = 0;
	read_bytes = 0;
	write_bytes = 0;

	rf = pList->first;
	while(1)
	{
		if(!rf)
		{
			assert(file_count == 0);
			break;
		}

		char* ps = rf->filename;
		while(*ps == '.' || *ps == '/')
		{
			++ps;
		}

		filename = (char*)malloc(strlen(decompress_path) + strlen(ps) + 1);
		
		memcpy(filename, decompress_path, strlen(decompress_path));		
		
		memcpy(filename + strlen(decompress_path), ps, strlen(ps) + 1);

		if(!overwrite_flag)
			overwrite_file(filename);

		if(!(osfp = fopen(filename, "w+")))
		{
			fprintf(stderr, "%s,%d: %s create failed\n", __FILE__, __LINE__, rf->filename);
			free(in_buf);
			free(out_buf);
			return false;
		}

		recipe_ptr = rf->offset_size_pairs;
		chunk_count = rf->chunk_count;
		while(chunk_count)
		{
			memcpy(&chunk_offset, recipe_ptr, sizeof(chunk_offset));
	        recipe_ptr += sizeof(chunk_offset);

	        memcpy(&chunk_length, recipe_ptr, sizeof(chunk_length));
	        recipe_ptr += sizeof(chunk_length);

			if(write_bytes + chunk_length > MAX_BUF_SIZE)
			{
				fwrite(out_buf, write_bytes, 1, osfp);
				write_bytes = 0;
			}
			
			if(!((chunk_offset >= read_offset) && ((chunk_offset + chunk_length) <= (read_offset + read_bytes))))
			{
				fseek(isfp, data_offset + chunk_offset, SEEK_SET);
				read_offset = chunk_offset;
				read_bytes = fread(in_buf, 1, MAX_BUF_SIZE, isfp);
			}

			memcpy(out_buf + write_bytes, in_buf + chunk_offset - read_offset, chunk_length);
			write_bytes += chunk_length;
			
			chunk_count -= 1;
		}
		
		if(write_bytes > 0)
	    {
	    	fwrite(out_buf, write_bytes, 1, osfp);
			write_bytes = 0;
	    } 

		fclose(osfp);
		rf = rf->next;		
		--file_count;
	}

	free(in_buf);
	free(out_buf);
	fclose(isfp); 
	
	return true;
} 

