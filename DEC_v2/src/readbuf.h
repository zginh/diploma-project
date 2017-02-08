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
	pthread_cond_t  fetch_cond;	//上层通知等待的ReadBuf开始工作
	pthread_cond_t  send_cond;	//ReadBuf通知上层可以取数据了

	workq_t  worker; 			//工作队列
	BufUnit** units;			//缓冲区数组
	uint16_t  buffer_number;	//系统拥有的缓冲区的个数
	uint16_t  buffer_capacity;	//每个缓冲区的容量
	uint16_t  valid;
	
	uint16_t  fetch_index;		//第一个空闲的缓冲区的编号
	uint16_t  send_index;		//第一个可以被上层访问的Buf编号
	uint16_t  fetch_wait;		//ReadBuf是否正在等待从下层提取数据
	uint16_t  send_wait;		//上层是否正在等着从ReadBuf中读数据
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
