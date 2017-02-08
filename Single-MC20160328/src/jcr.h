#ifndef JCR_H_
#define JCR_H_

typedef struct jcr { 
	pthread_mutex_t  mutex;    		//JCR�ṹ��Ļ�����

	FileInfo* first_file;
	FileInfo* last_file;

	uint32_t total_file_number;		//�ܵ��ļ�����
    uint32_t deduped_file_number;	//���ظ��ļ�������
	uint64_t total_file_size;		//�ܵ��ļ���С
	
	uint64_t total_chunk_number;	//�ܵķֿ����
	uint64_t deduped_chunk_number;	//���ظ��ֿ������	
	uint64_t deduped_chunk_size;	//���ظ��ֿ�Ĵ�С
	
	time_t cur_date;				//����ʱ��
	uint16_t decompress; 			//��ҵ����: 0-ѹ��  1-��ѹ

    uint64_t migrate_file_size;		//�����ļ��Ĵ�С
	uint32_t migrate_head_size;		//�����ļ���Ԫ���ݲ��ֵĴ�С
	uint64_t compressed_file_size;	//��ҵ��ɺ������ѹ���ļ��Ĵ�С

	double total_time;								//�ܵ���ҵʱ��
	double ae_time;		//�ֿ�ʱ��
	double SHA_time;	//SHA-1ʱ��
	double dedup_time;	//ȥ��ʱ��
	double SF_time; 				//SF�����Լ��ʱ��
	double content_reorganize_time;					//���������ļ���ʱ��
	double compress_time; 							//�����ļ�����ѹ����ʱ��
	
	double simhash_calcu_time;					//���������ļ���ʱ��
	double simhash_detect_time; 							//�����ļ�����ѹ����ʱ��

	double decompress_time; 		//���ݽ�ѹʱ��
	double restore_time;   			//�ļ��ָ�ʱ��
	
	char compress_algorithm[20];	//���õ�ѹ���㷨�����ƣ�Ĭ��ֵΪgzip
	char cmdline[100];
	
	char** folder_path;				//��ѹ�����ļ���������
	uint32_t folder_capacity;		//������������ɵ��ļ�������
	uint32_t folder_count;			//��������Ч���ļ�������

	char* compress_sequence;		//�ֿ���������
	char* compresse_filename;		//���ɵ�ѹ���ļ����ļ���
}JCR;

JCR* jcr_new();
void jcr_destroy(JCR* jcr);
bool jcr_append_path(JCR* jcr, char* path);
bool grow_jcr_folder_capacity(JCR* jcr);
bool jcr_append_file(JCR* jcr, FileInfo *file);
char* get_compresse_filename(JCR* jcr);
void update_jcr_info(JCR* jcr);		//����jcr�ķֿ�ͳ����Ϣ

#endif /* JCR_H_ */ 
