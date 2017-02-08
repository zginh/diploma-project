#include "global.h"

JCR *jcr;

uint64_t total_bytes = 0;

char compress_path[] = "./compress/";
char decompress_path[] = "./decompress/";
char store_path[] = "./data/";
char temp_path[] = "./tmp/";
char log_path[] = "./log/";

char tmp_file0[] = "./tmp/zsp0";
char tmp_file1[] = "./tmp/zsp1";

int max_queue_size = 5;
workq_t file_handler_queue;
workq_t chunk_handler_queue;
workq_t similar_detection_queue;

ReadBuf read_buf_queue;
ReadUnit *readUnit = NULL;

void changeFilePath(char* srcFile, char* destFile, int destLen, char* path)
{
	int start = 0, end = 0;
	int srcLen = strlen(srcFile);

	if(srcFile == NULL || srcLen == 0)
	{
		fprintf(stderr, "%s,%d: invalid source filename.\n", __FILE__, __LINE__);
	}

	start = srcLen - 1;
	end = start;
	for( ; start >= 0; --start)
	{
		char c = *(srcFile + start);

		if(c == '/' || c == '\\')
			break;

		if(c == '.' && end == srcLen - 1)
			end = start;
	}

	if(end < 0 )  end = 0;	
	start += 1;

	
	memset(destFile, 0, destLen);
	strcpy(destFile, path);

	int endLen = srcLen - end;
	int prefix_len = strlen(destFile);
	if(endLen > destLen - prefix_len)
	{
		memcpy(destFile + destLen - 4, srcFile + end, 4);
		destLen -= (4 + prefix_len);
	}
	else
	{
		memcpy(destFile + destLen - endLen, srcFile + end, endLen);
		destLen -= (endLen + prefix_len);		
	}
	memcpy(destFile + prefix_len, srcFile + start, destLen);	 

}


bool execLinuxCmd(char* cmdString)
{
	if(cmdString == NULL || strlen(cmdString)== 0)
		return false;

	int status = system(cmdString);
    if(status < 0)
    {
        fprintf(stderr, "cmd: %s\t error:%s\n", cmdString, strerror(errno));
        return false;
    }

	return true;
}

bool is_path_exist(char* file_path)
{
	if(file_path == NULL)
		return false;
	if(access(file_path, F_OK) ==0)
		return true;

	return false;
}

bool overwrite_file(char* filename)
{
	char ch;
	
	if(is_path_exist(filename))
	{
		while(1)
		{
			fprintf(stderr, "file: %s already exists,overwrite it ? [Y / N]" ,filename);

			ch = getchar();

			if(ch == 'y' || ch == 'Y')
			{
				break;
			}				
			else if(ch == 'n' || ch == 'N')
			{
				return false;
			} 
			else 
			{
				setbuf(stdin, NULL); 
			} 
		}
	}
	else
	{
		create_directory_recursive(filename);
	}
	return true;
}

bool write_fingerprint_to_file(FILE* fp, char* str, int len)
{
	int i = 0;
	for(i = 0; i < len; ++i)
		fprintf(fp, "%02x", (str[i] & 0xff));
	fprintf(fp, "\n");
}

bool write_chunk_to_file(FILE* fp, SimilarChunk* chunk)
{
	int i = 0;
	
	char* str = (char*)chunk->finger_chunk->fp;
	for(i = 0; i < sizeof(FILE*); ++i)
	{
		fprintf(fp, "%02x", (str[i] & 0xff));
	}
	
	fprintf(fp, "\t%ld\t%ld\n", chunk->finger_chunk->offset, chunk->finger_chunk->chunk_size);
}

bool create_directory_recursive(char *path)
{
	int len; 
	char* ptr;
	DIR *pDir;
	
	if(!path || !(len = strlen(path)))
		return true;

	ptr = strchr(path, '/');
	while(ptr)
	{ 
		*ptr = '\0';

		pDir = opendir(path);
		if(pDir == NULL)
		{
			mkdir(path, 0777);
		}
		closedir(pDir);

		*ptr = '/';
		ptr = strchr(ptr + 1, '/');
	}
}

uint64_t get_file_size(char* filename)
{
	struct stat buf;
	
	if(stat(filename, &buf) < 0)
	{
		return 0;
	}

	return (uint64_t)buf.st_size;
}


