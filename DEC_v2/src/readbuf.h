#ifndef _READBUF_H_
#define _READBUF_H_

typedef struct read_unit {
	FILE*	  fp;
	uint64_t  offset;
	uint32_t  length;
	uint16_t  lifetime;
}ReadUnit;

typedef struct buf_unit {
	pthread_mutex_t  mutex;
	uint32_t  capacity;
	ReadUnit  *info;
	char  *data;
}BufUnit;

typedef struct read_buf {
	pthread_cond_t  fetch_cond;	//�ϲ�֪ͨ�ȴ���ReadBuf��ʼ����
	pthread_cond_t  send_cond;	//ReadBuf֪ͨ�ϲ����ȡ������

	workq_t  worker; 			//��������
	BufUnit** units;			//����������
	uint16_t  buffer_number;	//ϵͳӵ�еĻ������ĸ���
	uint16_t  buffer_capacity;	//ÿ��������������
	uint16_t  valid;
	
	uint16_t  fetch_index;		//��һ�����еĻ������ı��
	uint16_t  send_index;		//��һ�����Ա��ϲ���ʵ�Buf���
	uint16_t  fetch_wait;		//ReadBuf�Ƿ����ڵȴ����²���ȡ����
	uint16_t  send_wait;		//�ϲ��Ƿ����ڵ��Ŵ�ReadBuf�ж�����
}ReadBuf;

#define READBUF_VALID  0xb509
#define MAX_UNIT_NUMBER 64 
#define MAX_UNIT_SIZE  16 * 1024

ReadUnit* readUnit_new(FILE *fp, uint64_t offset, uint32_t length);
void readUnit_destroy(ReadUnit *readUnit);
void readUnit_coalesce(FILE *fp, uint64_t offset, uint32_t length);

BufUnit* bufUnit_new(uint32_t buf_size);
void bufUnit_resize(BufUnit *bufUnit, uint32_t buf_size);
void bufUnit_destroy(BufUnit *bufUnit);

int readBuf_init(ReadBuf *readBuf);
int readBuf_destroy(ReadBuf *readBuf);
int readBuf_append(ReadBuf *readBuf, ReadUnit *readUnit);

int readBuf_get(ReadBuf *readBuf, char *data, uint64_t offset, uint32_t length);

void* readBuf_handler(void *arg);


#endif
