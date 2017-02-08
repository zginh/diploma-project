#ifndef JCR_H_
#define JCR_H_

typedef struct jcr { 
	pthread_mutex_t  mutex;    		//JCR结构体的互斥锁

	FileInfo* first_file;
	FileInfo* last_file;

	uint32_t total_file_number;		//总的文件个数
    uint32_t deduped_file_number;	//非重复文件的总数
	uint64_t total_file_size;		//总的文件大小
	
	uint64_t total_chunk_number;	//总的分块个数
	uint64_t deduped_chunk_number;	//非重复分块的总数	
	uint64_t deduped_chunk_size;	//非重复分块的大小
	
	time_t cur_date;				//工作时间
	uint16_t decompress; 			//作业类型: 0-压缩  1-解压

    uint64_t migrate_file_size;		//重组文件的大小
	uint32_t migrate_head_size;		//重组文件的元数据部分的大小
	uint64_t compressed_file_size;	//作业完成后产生的压缩文件的大小

	double total_time;								//总的作业时间
	double ae_time;		//分块时间
	double SHA_time;	//SHA-1时间
	double dedup_time;	//去重时间
	double SF_time; 				//SF相似性检测时间
	double content_reorganize_time;					//生成重组文件的时间
	double compress_time; 							//重组文件进行压缩的时间
	
	double simhash_calcu_time;					//生成重组文件的时间
	double simhash_detect_time; 							//重组文件进行压缩的时间

	double decompress_time; 		//数据解压时间
	double restore_time;   			//文件恢复时间
	
	char compress_algorithm[20];	//所用的压缩算法的名称，默认值为gzip
	char cmdline[100];
	
	char** folder_path;				//待压缩的文件名的数组
	uint32_t folder_capacity;		//数组的最多可容纳的文件名个数
	uint32_t folder_count;			//数组中有效的文件名个数

	char* compress_sequence;		//分块重组序列
	char* compresse_filename;		//生成的压缩文件的文件名
}JCR;

JCR* jcr_new();
void jcr_destroy(JCR* jcr);
bool jcr_append_path(JCR* jcr, char* path);
bool grow_jcr_folder_capacity(JCR* jcr);
bool jcr_append_file(JCR* jcr, FileInfo *file);
char* get_compresse_filename(JCR* jcr);
void update_jcr_info(JCR* jcr);		//更新jcr的分块统计信息

#endif /* JCR_H_ */ 
