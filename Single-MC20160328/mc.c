#include "global.h"

TIMER_DECLARE(1);
TIMER_DECLARE(2);
TIMER_DECLARE(3);
TIMER_DECLARE(4);






 
int main(int argc, char* argv[])
{
	TIMER_BEGIN(1);
	
    if(argc < 2)
    {
        usage(argv[0]);
        return FAILURE;
    } 
	
	jcr = jcr_new();

	default_folder_init();
		
    printf("Argument Parse\n");
	if(!args_parser(jcr, argc, argv))
	{
		jcr_destroy(jcr);
		return FAILURE;
	}

    printf("Handle Job\n");
	handle_job_request(jcr);

	TIMER_END(1, jcr->total_time);
	update_jcr_journal(jcr);	

	jcr_destroy(jcr);	

	fprintf(stderr, "Done\n\n");

    return SUCCESS;
}


void usage(char* program)
{
    fprintf(stderr, "Usage: ./mzip [OPTION...] [FILE]...\n");
    fprintf(stderr, "Examples:\n");
	fprintf(stderr, "  ./mzip sample.tar # compress file named sample.tar, with default gzip compression algorithm \n");
    fprintf(stderr, "  ./mzip -d sample.tar.mgz       # decompress file named sample.tar.mgz \n");
	fprintf(stderr, "  ./mzip --alg bzip2 sample.tar # compress file named sample.tar with bzip2 compression algorithm \n");
    fprintf(stderr, "  -h, --help    give this help list\n\n"); 
}

void default_folder_init()
{
	if(!is_path_exist(compress_path))
		create_directory_recursive(compress_path);

	if(!is_path_exist(decompress_path))
		create_directory_recursive(decompress_path);

	if(!is_path_exist(temp_path))
		create_directory_recursive(temp_path);

	if(!is_path_exist(log_path))
		create_directory_recursive(log_path);
}

bool args_parser(JCR *jcr, int argc, char* argv[])
{
	int opt;
	struct option long_options[] = {
		{"alg", 1, NULL, 'a'},
    	{"help", 0, NULL, 'h'},
    	{NULL, 0, NULL, 0}
	};

	int i = 0, len = 0; 

	for(i = 0; i < argc; ++i)
	{
		sprintf(jcr->cmdline + len, "%s ", argv[i]);
		len = strlen(jcr->cmdline);
	}
	jcr->cmdline[len-1] = '\0';

	while((opt=getopt_long(argc, argv, "a:d:h", long_options, NULL))!=-1){
		switch(opt){ 
			case 'd':
				jcr->decompress = 1;
				jcr_append_path(jcr, optarg);
				break;
				
			case 'a':
				strcpy(jcr->compress_algorithm, optarg);
				break;
				
			case 'h':
				usage(argv[0]);				
				return false;
		 }
	}
	
	if(optind < argc)
	{
		while(optind < argc)
		{
			jcr_append_path(jcr, argv[optind]);
			++optind;
		}		
	}
	
	if(!jcr->decompress && strlen(jcr->compress_algorithm) == 0)
	{
		strcpy(jcr->compress_algorithm, "gzip");
	}

	return true;
}

void handle_job_request(JCR *jcr)
{
	int i = 0;
	bool succ = false;

	//TIMER_BEGIN(2);

	if(!jcr->folder_count)  return;
	
	if(!jcr->decompress)
	{	
		chunk_agl_init();	
		dedup_index_init();
		
		#ifdef _SKIP_ON_
		skip_index_init();
		#endif
	
		#ifndef _SIMHASH_TEST_
		similar_detection_init();	
		fprintf(stderr, "Tip: Super Feature Test\n");
		#else
		simhash_detection_init();
		fprintf(stderr, "Tip: Simhash Test\n");
		#endif
/*		
		workq_init(&file_handler_queue, 5, file_handler);
		workq_init(&chunk_handler_queue, 5, chunk_handler);

		#ifndef _SIMHASH_TEST_
		workq_init(&similar_detection_queue, 5, similar_detection_handler);
		#else
		workq_init(&similar_detection_queue, 5, simhash_detection_handler);
		#endif
*/
		for(i = 0; i < jcr->folder_count; ++i)
		{
			walk_dir(jcr, jcr->folder_path[i]);
		}
/*		
		workq_destroy(&file_handler_queue);
		workq_destroy(&chunk_handler_queue);
		workq_destroy(&similar_detection_queue);
*/
		TIMER_BEGIN(2);
		
		readBuf_init(&read_buf_queue);

		#ifndef _SIMHASH_TEST_
		get_chunk_sequences(jcr); 	
		#else
		get_simhash_sequences(jcr);
		#endif
		
		fprintf(stderr, "Get sequence processed\n");
		//TIMER_END(2, jcr->get_chunk_sequence_time);
		//TIMER_BEGIN(2);

		succ = content_reorganize(jcr);	
		
		readBuf_destroy(&read_buf_queue);
		
		fprintf(stderr, "Content reorganize processed\n");
		TIMER_END(2, jcr->content_reorganize_time);

		if(succ)
		{		
			TIMER_BEGIN(2);
			final_compress(jcr); 
			TIMER_END(2, jcr->compress_time);
			fprintf(stderr, "compress processed\n");
		}		

		dedup_index_destroy();
		
		#ifdef _SKIP_ON_
		skip_index_destroy();
		#endif
		
		#ifndef _SIMHASH_TEST_
		similar_detection_destroy();	
		#else
		simhash_detection_destroy();
		#endif		
	}
	else
	{
		workq_init(&file_handler_queue, max_queue_size, file_decompress_handler);
		
		for(i = 0; i < jcr->folder_count; ++i)
		{
			walk_dir(jcr, jcr->folder_path[i]);
		}

		workq_destroy(&file_handler_queue);
	}		
}

void walk_dir(JCR *jcr, char * path)
{
	int lens;
	char* pathname, *ptr;
	DIR* pDir;
	struct stat state;
    struct dirent* ent = NULL;
	
	if(stat(path, &state) != 0 )
    {
        fprintf(stderr, "%s,%d: %s does not exist\n", __FILE__, __LINE__, path);
        return;
    }

	if(S_ISDIR(state.st_mode))
    {
		lens = strlen(path) + 2;
		pDir=opendir(path);	 
		
		while (NULL != (ent=readdir(pDir)))
		{		
			if(!strncmp(ent->d_name, ".", 1))
				continue;
					
			lens += strlen(ent->d_name);
			pathname = (char*)malloc(lens);
			memset(pathname, 0, lens);
			
			if(*(path+strlen(path)-1) == '/')
				sprintf(pathname, "%s%s", path, ent->d_name);
			else
				sprintf(pathname, "%s/%s", path, ent->d_name);

			walk_dir(jcr, pathname);
				
			free(pathname);
		}
		closedir(pDir);
	}
	else
	{
		if(jcr->decompress)
		{
			workq_add(&file_handler_queue, path, NULL, 0);
		}
		else
		{
			FileInfo* finfo = fileInfo_new(path);
			if(!jcr_append_file(jcr,finfo))
			{
				fprintf(stderr, "%s£¬%d: %s insert failure.\n", __FILE__, __LINE__, path);
				return;
			}
			//workq_add(&file_handler_queue, finfo, NULL, 0);
			file_handler(finfo);
		}
	}
}

void* file_handler(void* arg)
{
	FILE* fp;
	FingerChunk* fc; 
	FingerChunk* prev_chunk = NULL;
	DataChunk* data_chunk;
	
	int eof_flag = 0;
	uint64_t offset = 0;
    uint32_t chunk_size = 0;
	uint32_t cnt = 0;
    int32_t srclen = 0, left_bytes = 0, left_offset = 0;

	unsigned char strTemp[MAX_BUF_SIZE + 1];
    unsigned char* ptr = NULL;	

	if(!arg)
		return NULL;

	FileInfo* file_info = (FileInfo*)arg;

    if((fp = fopen(file_info->filename, "r")) == NULL)
    {
    	fprintf(stderr, "%s,%d: %s open error\n", __FILE__, __LINE__, file_info->filename);
        return NULL;
    }	
	file_info->fp = fp;
	
	strTemp[MAX_BUF_SIZE] = '\0';
    unsigned char* srcBuf = (unsigned char* )malloc(MAX_BUF_SIZE + MAX_CHUNK_SIZE);	
    if(!srcBuf)
    {
        fprintf(stderr, "%s,%d memory allocation failed!\n", __FILE__, __LINE__);	
        exit(EXIT_FAILURE);
    }	

	if((srclen = fread(srcBuf, 1, MAX_BUF_SIZE, fp)) != MAX_BUF_SIZE)
	{
		eof_flag = 1;
	}
	
	left_bytes = srclen;	
	while(1)
    {
        if(left_bytes < MAX_CHUNK_SIZE && eof_flag == 0)
        {
			memcpy(srcBuf, srcBuf + left_offset, left_bytes);
            left_offset = 0;
			
            if((srclen = fread(strTemp, 1, MAX_BUF_SIZE, fp)) != MAX_BUF_SIZE)
			{
				eof_flag = 1;
			}
			
			if(srclen != 0)
			{
				memcpy(srcBuf + left_bytes, strTemp, srclen);
				left_bytes += srclen;   
			}           
        }
		
		if(left_bytes == 0)
		{
			assert(eof_flag == 1);
			break;
		}
		TIMER_BEGIN(2);
		
        chunk_size = ae_chunk_data(srcBuf + left_offset, left_bytes); 

		TIMER_END(2,jcr->ae_time);
		fc = fingerChunk_new(offset, chunk_size);
		fc->fp = fp;

		data_chunk = dataChunk_new(file_info, fc, srcBuf + left_offset, chunk_size);
		data_chunk->prev_chunk = prev_chunk;
		prev_chunk = fc;
		
		//workq_add(&chunk_handler_queue, data_chunk, NULL, 0);   
		chunk_handler(data_chunk);		
		
		recipe_append_fingerchunk(file_info->rp, fc); 
		
		file_info->total_chunk_number += 1;
		file_info->total_chunk_size += chunk_size;
		
		offset += chunk_size;
		left_offset += chunk_size;
		left_bytes -= chunk_size;
    }

    free(srcBuf);

	return NULL;	
}

void* chunk_handler(void* arg)
{
	FileInfo *file;
	DataChunk *data_chunk;
	FingerChunk *finger_chunk;

	if(!arg)  return NULL;

	data_chunk = (DataChunk*)arg;
	file = data_chunk->file;
	finger_chunk = data_chunk->fc;	
	TIMER_BEGIN(2);
	chunk_finger(data_chunk->data, data_chunk->len, finger_chunk->chunk_hash);  
	TIMER_END(2,jcr->SHA_time);
	TIMER_BEGIN(3);
	if(dedup_index_insert(finger_chunk->chunk_hash))
	{ 
		P(file->mutex);
		file->deduped_chunk_number += 1;
		file->deduped_chunk_size += finger_chunk->chunk_size;
		V(file->mutex);
		
		//workq_add(&similar_detection_queue,(void *)data_chunk,NULL,0);
		TIMER_END(3,jcr->dedup_time);
		#ifndef _SIMHASH_TEST_
		TIMER_BEGIN(4);
		similar_detection_handler(data_chunk);
		TIMER_END(4,jcr->SF_time);
		#else
		simhash_detection_handler(data_chunk);
		#endif

	} 
	else
	{
		TIMER_END(3,jcr->dedup_time);
		dataChunk_destroy(data_chunk);
	}
	return NULL;
}

void update_jcr_journal(JCR *jcr)
{
	FILE *logfile, *logfile1, *logfile2, *logfile3;

	if(jcr->decompress)
	{
		logfile = fopen("./log/decompress.txt", "a+");
		
		fprintf(logfile, "cmdline: %s\n", jcr->cmdline);
		fprintf(logfile, "\n"); 

		fprintf(logfile, "total_size = %.4f MB\n", 1.00 * total_bytes / 1024.0 / 1024.0);
		fprintf(logfile, "total time = %.3fs\n", jcr->total_time/1000000);
		fprintf(logfile, "decompress time = %.3fs\n", jcr->decompress_time/1000000);
        fprintf(logfile, "restore time = %.3fs\n",jcr->restore_time/1000000);
		fprintf(logfile, "through_output = %.4f MB/s\n", 1.00 * total_bytes / 1024.0 / 1024.0 / (jcr->total_time/1000000));
		fprintf(logfile, "\n");

		fclose(logfile);		
	}
	else
	{
		logfile = fopen("./log/compress.txt", "a+");

		fprintf(logfile, "cmdline: %s\n", jcr->cmdline);
		fprintf(logfile, "\n"); 

		fprintf(logfile, "total_size = %.4f MB\n", 1.00 * jcr->total_file_size / 1024.0 / 1024.0);
		fprintf(logfile, "dedup_size = %.4f MB\n", 1.00 * jcr->deduped_chunk_size / 1024.0 / 1024.0);
		fprintf(logfile, "deduped_ratio = %.2f%%\n", 100.00 * (jcr->total_file_size - jcr->deduped_chunk_size) / jcr->total_file_size);
		fprintf(logfile, "\n");

		fprintf(logfile, "migrate_file_size = %.4f MB\n", 1.00 * jcr->migrate_file_size / 1024.0 / 1024.0);
		fprintf(logfile, "migrate_head_size = %.4f MB\n", 1.00 * jcr->migrate_head_size / 1024.0 / 1024.0);
		fprintf(logfile, "compressed_file_size = %.4f MB\n", 1.00 * jcr->compressed_file_size / 1024.0 / 1024.0);
		fprintf(logfile, "compress_ratio = %.2f\n", (1.00 * jcr->total_file_size / 1024.0 / 1024.0) / (1.00 * jcr->compressed_file_size / 1024.0 / 1024.0));
		fprintf(logfile, "\n");

		fprintf(logfile, "total time = %.3fs\n", jcr->total_time/1000000);
		fprintf(logfile, "AE time = %.3fs\n", jcr->ae_time/1000000);
		fprintf(logfile, "SHA-1 time = %.3fs\n", jcr->SHA_time/1000000);
		fprintf(logfile, "dedup time = %.3fs\n", jcr->dedup_time/1000000);
		fprintf(logfile, "Super-Feature time = %.3fs\n", jcr->SF_time/1000000);
		fprintf(logfile, "content reorganize time = %.3fs\n", jcr->content_reorganize_time/1000000);
		fprintf(logfile, "compressing time = %.3fs\n", jcr->compress_time/1000000);
		fprintf(logfile, "\n"); 
		
		fprintf(logfile, "simhash_calcu_time = %.3fs\n", jcr->simhash_calcu_time);
		fprintf(logfile, "simhash_detect_time = %.3fs\n", jcr->simhash_detect_time);
		fprintf(logfile, "\n"); 

		fprintf(logfile, "through_output = %.4f MB/s\n", 1.00 * jcr->total_file_size / 1024.0 / 1024.0 / (jcr->total_time/1000000));
		fprintf(logfile, "\n");
		
		fprintf(logfile, "through_output2 = %.4f MB/s\n", 1.00 * (jcr->total_file_size - jcr->compressed_file_size) / 1024.0 / 1024.0 / (jcr->total_time/1000000));
		fprintf(logfile, "\n");
		
		fclose(logfile);
		
	}	
}
/*	{
		logfile1 = fopen("./log/ratio.txt", "a+");
		logfile2 = fopen("./log/time.txt", "a+");
		logfile3 = fopen("./log/output.txt", "a+");

		fprintf(logfile1, "cmdline: %s\n", jcr->cmdline);
		//fprintf(logfile, "\n"); 
		
		fprintf(logfile2, "cmdline: %s\n", jcr->cmdline);
		//fprintf(logfile, "\n"); 
		
		fprintf(logfile3, "cmdline: %s\n", jcr->cmdline);
		//fprintf(logfile, "\n"); 

		//fprintf(logfile, "total_size = %.4f MB\n", 1.00 * jcr->total_file_size / 1024.0 / 1024.0);
		//fprintf(logfile, "dedup_size = %.4f MB\n", 1.00 * jcr->deduped_chunk_size / 1024.0 / 1024.0);
		//fprintf(logfile, "deduped_ratio = %.2f%%\n", 100.00 * (jcr->total_file_size - jcr->deduped_chunk_size) / jcr->total_file_size);
		//fprintf(logfile, "\n");

		//fprintf(logfile, "migrate_file_size = %.4f MB\n", 1.00 * jcr->migrate_file_size / 1024.0 / 1024.0);
		//fprintf(logfile, "migrate_head_size = %.4f MB\n", 1.00 * jcr->migrate_head_size / 1024.0 / 1024.0);
		//fprintf(logfile, "compressed_file_size = %.4f MB\n", 1.00 * jcr->compressed_file_size / 1024.0 / 1024.0);
		fprintf(logfile1, "compress_ratio = %.2f%%\n", 100.00 * jcr->compressed_file_size / jcr->total_file_size);
		fprintf(logfile1, "\n");

		fprintf(logfile2, "total time = %.3fs\n", jcr->total_time);
		fprintf(logfile2, "phase1 time = %.3fs\n", jcr->chunk_dedup_and_similar_detect_time);
		//fprintf(logfile, "phase2 time = %.3fs\n", jcr->get_chunk_sequence_time);
		fprintf(logfile2, "phase3 time = %.3fs\n", jcr->content_reorganize_time);
		fprintf(logfile2, "phase4 time = %.3fs\n", jcr->compress_time);
		fprintf(logfile2, "\n"); 

		fprintf(logfile3, "through_output = %.4f MB/s\n", 1.00 * jcr->total_file_size / 1024.0 / 1024.0 / jcr->total_time);
		//fprintf(logfile, "\n");
		
		fprintf(logfile3, "through_output2 = %.4f MB/s\n", 1.00 * (jcr->total_file_size - jcr->compressed_file_size) / 1024.0 / 1024.0 / jcr->total_time);
		fprintf(logfile3, "\n");
		
		fclose(logfile1);
		fclose(logfile2);
		fclose(logfile3);
		
	}	
}*/
