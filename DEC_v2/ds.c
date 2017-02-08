#include "global.h"

TIMER_DECLARE(1);
TIMER_DECLARE(2);
 

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
/*****************************************************************************
 * �� �� ��  : handle_job_request
 * �� �� ��  : �ı�·��
 * ��������  : 2016��4��29��
 * ��������  : ����job��ѹ���ͽ�ѹ����
 * �������  : JCR *jcr  job control record
 * �������  : ��
 * �� �� ֵ  : 
 * ���ù�ϵ  : 
 * ��    ��  : 

*****************************************************************************/
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
		
		similar_detection_init();	
		//fprintf(stderr, "Tip: Dedup Similarity Test\n");
		
		#ifndef _SINGLE_THREAD_
		workq_init(&file_handler_queue, 5, file_handler);
		workq_init(&chunk_handler_queue, 5, chunk_handler);
		workq_init(&similar_detection_queue, 5, similar_detection_handler);
		#endif
		
		for(i = 0; i < jcr->folder_count; ++i)
		{
			walk_dir(jcr, jcr->folder_path[i]);
		}

		#ifndef _SINGLE_THREAD_
		workq_destroy(&file_handler_queue);
		workq_destroy(&chunk_handler_queue);
		workq_destroy(&similar_detection_queue);
		#endif
		
		TIMER_BEGIN(2);
		#ifdef _READ_THREAD_
		//fprintf(stderr,"Get read_buf\n");
		readBuf_init(&read_buf_queue);
		#endif
		
		//fprintf(stderr,"Ready to get chunk sequneces\n");
		/*��ȡ�����ļ�˳��*/
		get_chunk_sequences(jcr); 	
		//TIMER_END(2,jcr->DEC_time);
		
		//fprintf(stderr, "Get sequence processed\n");

		/*�ļ�����*/
		//TIMER_BEGIN(2);
		succ = content_reorganize(jcr);

		#ifdef _READ_THREAD_
		readBuf_destroy(&read_buf_queue);
		#endif
		
		fprintf(stderr, "Content reorganize processed\n");
		TIMER_END(2, jcr->content_reorganize_time);

		if(succ)
		{		
			TIMER_BEGIN(2);
			/*ѹ���ļ�*/
			final_compress(jcr); 
			TIMER_END(2, jcr->compress_time);
			fprintf(stderr, "compress processed\n");
		}		

		dedup_index_destroy();
	
		similar_detection_destroy();
				
	}
	/*��ѹ����*/
	else
	{
		#ifdef _DECOMPRESS_THREAD_
		workq_init(&file_handler_queue, max_queue_size, file_decompress_handler);
		#endif
		
		for(i = 0; i < jcr->folder_count; ++i)
		{
			walk_dir(jcr, jcr->folder_path[i]);
		}

		#ifdef _DECOMPRESS_THREAD_
		workq_destroy(&file_handler_queue);
		#endif _DECOMPRESS_THREAD_
	}		
}
/*****************************************************************************
 * �� �� ��  : walk_dir
 * �� �� ��  : �ı�·��
 * ��������  : 2016��4��29��
 * ��������  : �ݹ�Ŀ¼�õ��ļ���
 * �������  : JCR *jcr     job control record
               char * path  �ļ�·��
 * �������  : ��
 * �� �� ֵ  : 
 * ���ù�ϵ  : ��handler_job_request����
 * ��    ��  : 

*****************************************************************************/
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
			#ifdef _DECOMPRESS_THREAD_
			workq_add(&file_handler_queue, path, NULL, 0);
			#else
			file_decompress_handler(path);
			#endif
		}
		else
		{
			FileInfo* finfo = fileInfo_new(path);
			if(!jcr_append_file(jcr,finfo))
			{
				fprintf(stderr, "%s��%d: %s insert failure.\n", __FILE__, __LINE__, path);
				return;
			}
			#ifndef _SINGLE_THREAD_
			workq_add(&file_handler_queue, finfo, NULL, 0);
			#else
			file_handler(finfo);
			#endif
		}
	}
}
/*****************************************************************************
 * �� �� ��  : file_handler
 * �� �� ��  : �ı�·��
 * ��������  : 2016��4��29��
 * ��������  : �����ļ���������ȡ�ļ������棬ae�ֿ飬�鼶��������recipe
 * �������  : void* arg  �ļ�������
 * �������  : ��
 * �� �� ֵ  : 
 * ���ù�ϵ  : ��walk_dir����
 * ��    ��  : 

*****************************************************************************/
void* file_handler(void* arg)
{
	//TIMER_DECLARE(3);
	FILE* fp;
	FingerChunk* fc; 
	FingerChunk* prev_chunk = NULL;
	DataChunk* data_chunk;
	//FILE* scfp;
	//char *fcpath="/home/hzj/fingerchunk.txt";
	
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

	#ifdef _TEST_
	if((scfp = fopen("./SimilarChunk","w+"))==NULL)
	{
		fprintf(stderr,"%s,%d:SimilarChunk open error\n",__FILE__,__LINE__);
		return NULL;
	}
	#endif

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
	//TIMER_DECLARE(2);
	while(1)
    {
    	//srcBuf��������ʣ�����ݲ���һ��chunk
        if(left_bytes < MAX_CHUNK_SIZE && eof_flag == 0)
        {
			memcpy(srcBuf, srcBuf + left_offset, left_bytes);
            left_offset = 0;
			//�Ӵ����ж����ݣ������ȡ��СС��MAX_BUF_SIZE��˵�������ļ�β
            if((srclen = fread(strTemp, 1, MAX_BUF_SIZE, fp)) != MAX_BUF_SIZE)
			{
				eof_flag = 1;
			}
			//���ݿ�����srcBuf��������
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
		/*ae�ֿ鲿��*/
        chunk_size = ae_chunk_data(srcBuf + left_offset, left_bytes); 
		TIMER_END(2,jcr->ae_chunk_time);
		fc = fingerChunk_new(offset, chunk_size);
		fc->fp = fp;
		
		data_chunk = dataChunk_new(file_info, fc, srcBuf + left_offset, chunk_size);

		#ifndef _SINGLE_THREAD_
		workq_add(&chunk_handler_queue, data_chunk, NULL, 0); 
		#else
		/*�鼶����*/
		chunk_handler(data_chunk);
		#endif
		/*�����ļ���*/
		recipe_append_fingerchunk(file_info->rp, fc); 

		//fprintf(fcfp,"fingerprint=%x offset=%I64u chunk_size=%I32u\n",fc->chunk_hash,fc->offset,fc->chunk_size);
		
		file_info->total_chunk_number += 1;
		file_info->total_chunk_size += chunk_size;
		
		offset += chunk_size;
		left_offset += chunk_size;
		left_bytes -= chunk_size;
    }
	//fclose(scfp);
    free(srcBuf);
	return NULL;	
}
/*****************************************************************************
 * �� �� ��  : chunk_handler
 * �� �� ��  : �ı�·��
 * ��������  : 2016��4��29��
 * ��������  : �鼶������������ָ�ƣ�ȥ�أ�ȥ�ظ�֪
 * �������  : void* arg  ���ݿ����Ϣ
 * �������  : ��
 * �� �� ֵ  : 
 * ���ù�ϵ  : ��file_handler����
 * ��    ��  : 

*****************************************************************************/
void* chunk_handler(void* arg)
{
	static int counter = 0;
	FileInfo *file;
	DataChunk *data_chunk;
	FingerChunk *finger_chunk;
	SimilarChunk *sc;
	DedupIndexEntry *entry;
	//TIMER_DECLARE(2);
	if(!arg)  return NULL;

	data_chunk = (DataChunk*)arg;
	file = data_chunk->file;
	finger_chunk = data_chunk->fc;	
	TIMER_BEGIN(2);
	/*����ָ��*/
	chunk_finger(data_chunk->data, data_chunk->len, finger_chunk->chunk_hash); 
	entry=dedup_index_lookup(finger_chunk->chunk_hash);
	TIMER_END(2,jcr->SHA_time);
	/*ȥ���Լ�ȥ�ظ�֪���֣���֪�ظ�������AWARENESS�ķ�Χ������ֱ����֮ǰ�ظ�����������ĺ���
	����һ��similarList���洢��������߼�˳��*/
	TIMER_BEGIN(2);
	if(!entry)//���ظ���
	{ 
		counter++;
		//fprintf(scfp,"%llu\n",finger_chunk->offset);
		/*��������������˫���ϵ*/
		sc = similarChunk_new(finger_chunk);
		entry=dedup_index_insert(finger_chunk->chunk_hash,sc);
		/*���ȸ�֪��ǰ����*/
		if(!sim_flag)
			similarList_append(sc);
		else
			simchunkset_append(sc);
		P(file->mutex);
		file->deduped_chunk_number += 1;
		file->deduped_chunk_size += finger_chunk->chunk_size;
		V(file->mutex);
		
	} 
	else
	{
		entry->pos->finger_chunk->dup_flag = 1;
		if(sim_flag)
			similarList_modify_front_v2(entry->pos);
		else
			similarList_modify_front(entry->pos);
		memcpy(pre_dedup_hash,finger_chunk->chunk_hash,sizeof(Fingerprint));
		sim_flag=1;
	}
	TIMER_END(2,jcr->DEC_time);
	dataChunk_destroy(data_chunk);
	return NULL;
}
/*****************************************************************************
 * �� �� ��  : update_jcr_journal
 * �� �� ��  : �ı�·��
 * ��������  : 2016��4��29��
 * ��������  : �������������log
 * �������  : JCR *jcr  job control record
 * �������  : ��
 * �� �� ֵ  : 
 * ���ù�ϵ  : 
 * ��    ��  : 

*****************************************************************************/
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
		fprintf(logfile, "restore time = %.3fs\n", jcr->restore_time/1000000);
		fprintf(logfile, "through_output = %.4f MB/s\n", 1.00 * total_bytes / 1024.0 / 1024.0 / (jcr->total_time/1000000));
		fprintf(logfile, "\n");

		fclose(logfile);		
	}
	else
	{
		logfile = fopen("./log/compress.txt", "a+");

		fprintf(logfile, "cmdline: %s\n", jcr->cmdline);
		fprintf(logfile, "\n"); 

		fprintf(logfile,"total_chunk_number = %llu\n",jcr->total_chunk_number);
		fprintf(logfile,"dedupe_chunk_number = %llu\n",jcr->total_chunk_number-jcr->deduped_chunk_number);
		fprintf(logfile,"deduped_chunk_number = %llu\n",jcr->deduped_chunk_number);

		fprintf(logfile, "total_size = %.4f MB\n", 1.00 * jcr->total_file_size / 1024.0 / 1024.0);
		fprintf(logfile, "dedup_size = %.4f MB\n", 1.00 * jcr->deduped_chunk_size / 1024.0 / 1024.0);
		fprintf(logfile, "deduped_ratio = %.2f%%\n", 100.00 * (jcr->total_file_size - jcr->deduped_chunk_size) / jcr->total_file_size);
		fprintf(logfile, "avg_chunk_size = %.4f\n",1.00 * jcr->total_file_size/1024.0/jcr->total_chunk_number);
		fprintf(logfile, "\n");

		fprintf(logfile, "migrate_file_size = %.4f MB\n", 1.00 * jcr->migrate_file_size / 1024.0 / 1024.0);
		fprintf(logfile, "migrate_head_size = %.4f MB\n", 1.00 * jcr->migrate_head_size / 1024.0 / 1024.0);
		fprintf(logfile, "compressed_file_size = %.4f MB\n", 1.00 * jcr->compressed_file_size / 1024.0 / 1024.0);
		fprintf(logfile, "compress_ratio = %.2f\n",(1.00 * jcr->total_file_size / 1024.0 / 1024.0) / (1.00 * jcr->compressed_file_size / 1024.0 / 1024.0));
		fprintf(logfile, "\n");

		fprintf(logfile, "total time = %.3fs\n", jcr->total_time/1000000);
		fprintf(logfile, "AE time = %.3fs\n", jcr->ae_chunk_time/1000000);
		fprintf(logfile, "SHA-1 time = %.3fs\n", jcr->SHA_time/1000000);
		fprintf(logfile, "DEC time = %.3fs\n", jcr->DEC_time/1000000);
		fprintf(logfile, "content reorganize time = %.3fs\n", jcr->content_reorganize_time/1000000);
		fprintf(logfile, "compress time = %.3fs\n", jcr->compress_time/1000000);
		fprintf(logfile, "\n"); 
		
		//fprintf(logfile, "simhash_calcu_time = %.3fs\n", jcr->simhash_calcu_time);
		//fprintf(logfile, "simhash_detect_time = %.3fs\n", jcr->simhash_detect_time);
		//fprintf(logfile, "\n"); 

		fprintf(logfile, "total_filesize_output = %.4f MB/s\n", 1.00 * jcr->total_file_size / 1024.0 / 1024.0 / (jcr->total_time/1000000));
		fprintf(logfile, "\n");
		
		fprintf(logfile, "data_reduction_output2 = %.4f MB/s\n", 1.00 * (jcr->total_file_size - jcr->compressed_file_size) / 1024.0 / 1024.0 / (jcr->total_time/1000000));
		fprintf(logfile, "\n");
		
		fclose(logfile);
		
	}	
}

