#ifndef _DECOMPRESS_H_
#define _DECOMPRESS_H_

typedef struct restore_file_entity 
{ 
	char* filename; 
	uint64_t file_size;
	uint64_t chunk_count; 	
	char* offset_size_pairs;
	struct restore_file_entity* next;
}RestoreFile;

typedef struct restore_file_list
{
	uint32_t file_count;
	RestoreFile* first;
	RestoreFile* last;
	struct restore_file_list* next;
}RestoreFileList;

RestoreFile* restoreFile_new();
void restoreFile_destroy(RestoreFile* rf);

RestoreFileList* restoreFileList_new();
void restoreFileList_destroy(RestoreFileList* rflist);
bool append_new_restoreFile(RestoreFileList* rflist, RestoreFile* rf);

void* file_decompress_handler(void* arg);
char* generate_cmdline(char cmdline[], int len, char* cfile);
bool parse_metadata(char* dfile, RestoreFileList* pList, uint64_t* data_offset);
bool file_restore(char* dfile, RestoreFileList* pList, uint64_t data_offset);

#endif
