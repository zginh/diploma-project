#include "../global.h"

#include <openssl/sha.h>
#include <openssl/md5.h>

enum {size = 48};
uint64_t fingerprint;
static int rabin_init_flag = 0;
static int bufpos;
static uint64_t  U[256];
static uint8_t buf[size];
static int shift;
static uint64_t T[256];
static uint64_t poly;

/*与函数slide8等效，但使用宏替换速度更快*/
#define SLIDE(m) do{       \
	    unsigned char om;   \
	    uint64_t x;	 \
        if (++bufpos >= size)  \
        bufpos = 0;				\
        om = buf[bufpos];		\
        buf[bufpos] = m;		 \
		fingerprint ^= U[om];	 \
		x = fingerprint >> shift;  \
		fingerprint <<= 8;		   \
		fingerprint |= m;		  \
		fingerprint ^= T[x];	 \
}while(0)

#define SLIDE2(m,fingerprint,bufPos,buf) do{	\
	    unsigned char om;   \
	    u_int64_t x;	 \
		if (++bufPos >= size)  \
        bufPos = 0;				\
        om = buf[bufPos];		\
        buf[bufPos] = m;		 \
		fingerprint ^= U[om];	 \
		x = fingerprint >> shift;  \
		fingerprint <<= 8;		   \
		fingerprint |= m;		  \
		fingerprint ^= T[x];	 \
}while(0)

static unsigned long _last_pos;
static unsigned long _cur_pos;
  
static unsigned int _num_chunks;
static const unsigned chunk_size = 0x1FFF; 

double cdtime = 0,nctime = 0;
 
const char bytemsb[0x100] = {
  0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5,
  5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
  6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
  7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
  8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
};

long int MiAi[256][2]=
{
5652377,  45673472, 24423617, 146512310, 16341615,142364781, 
3526234, 1225434201,  654410,324130,  3124312, 359840125, 
35, 15254201,  61,41870,  212, 3340125, 
121, 44122, 551, 8431641, 610, 824311, 
367, 254201,  651,7632410,  319, 7540125, 
129, 115422, 551, 34431741, 633, 424311, 	
11, 31, 17,5, 23, 37,
71,97,37,241,247,257,
137,2391,431,831,2137,4313,
173, 3022467, 523, 4507221, 753, 6750467,
143, 412234, 372, 3174123, 637, 324123, 
821, 25420451, 107,31023,  423, 3512545, 
667, 1541242, 851, 843541741, 453, 3244311, 
964, 223401,  910,3233410,  223, 354012523, 
827,12322, 751, 843178741, 456, 438911, 
};

/******************************the rabin************************/
static  uint8_t  fls32 (uint32_t v)
{
  if (v & 0xffff0000) {
    if (v & 0xff000000)
      return 24 + bytemsb[v>>24];
    else
      return 16 + bytemsb[v>>16];
  }
  if (v & 0x0000ff00)
    return 8 + bytemsb[v>>8];
  else
    return bytemsb[v];
}

static uint8_t fls64 (uint64_t v)
{
  uint32_t h;
  if ((h = v >> 32))
    return 32 + fls32 (h);
  else
    return fls32 ((uint32_t) v);
}

static uint64_t polymod (uint64_t nh, uint64_t nl, uint64_t d)
{

  int k = fls64 (d) - 1;
  int i;
  d <<= (63 - k);
  if (nh) {
    if (nh & MSB64)
      nh ^= d;
    for (i = 62; i >= 0; i--)
      if (nh & ((uint64_t) 1 << i)) {
    nh ^= d >> (63 - i);
    nl ^= d << (i + 1);
      }
  }
  for (i = 63; i >= k; i--)
  {  
    if (nl & ((uint64_t) 1) << i)
      nl ^= d >> (63 - i);
  }
  return nl;
}

static void polymult (uint64_t *php, uint64_t *plp, uint64_t x, uint64_t y)
{

  int i;

  uint64_t ph = 0, pl = 0;
  if (x & 1)
    pl = y;
  for ( i = 1; i < 64; i++)
    if (x & ((int64_t)1 << i)) {
      ph ^= y >> (64 - i);
      pl ^= y << i;
    }
  if (php)
    *php = ph;
  if (plp)
    *plp = pl;
}

static uint64_t append8 (uint64_t p, uint8_t m){ 
     return ((p << 8) | m) ^ T[p >> shift]; 
};

static uint64_t slide8 (uint8_t m) 
{
   unsigned char om;
	uint64_t x;
    if (++bufpos >= size)
        bufpos = 0;
        om = buf[bufpos];
        buf[bufpos] = m;
		fingerprint ^= U[om];
		x = fingerprint >> shift;
		fingerprint <<= 8;
		fingerprint |= m;
		fingerprint ^= T[x];
    return fingerprint;  /*= append8 (fingerprint ^ U[om], m)*/
}

static uint64_t polymmult (uint64_t x, uint64_t y, uint64_t d)
{
    uint64_t h, l;
    polymult (&h, &l, x, y);
    return polymod (h, l, d);
}

static void calcT (uint64_t poly)
{

  int j;
  uint64_t T1;

  int xshift = fls64 (poly) - 1;
  shift = xshift - 8;
  T1 = polymod (0, ((int64_t) 1<< xshift), poly);
  for (j = 0; j < 256; j++)
  {
    T[j] = polymmult (j, T1, poly) | ((uint64_t) j << xshift);
  }
}

static void rabinpoly_init (uint64_t  p)
{
  poly=p;
  calcT (poly);
}

static void windows_init (uint64_t poly)
{
    int i;
    uint64_t sizeshift;

    rabinpoly_init(poly);
    fingerprint = 0;
    bufpos = -1;
    sizeshift = 1;
    for (i = 1; i < size; i++)
        sizeshift = append8 (sizeshift, 0);
    for (i = 0; i < 256; i++)
        U[i] = polymmult (i, sizeshift, poly);
    memset ((char*) buf,0, sizeof (buf));
}

static void windows_reset () { 
    fingerprint = 0; 
    memset((char*) buf,0,sizeof (buf));
}

void chunk_agl_init()
{
    if(rabin_init_flag == 0)
    {
        windows_init(FINGERPRINT_PT);
        rabin_init_flag = 1;        
    }
	_last_pos = 0;
    _cur_pos = 0;
    windows_reset();
    _num_chunks = 0;
}


int chunk_data(unsigned char* data, uint32_t len)
{
    int i = 1;
    windows_reset();
    while(i <= len)
    {
        SLIDE(data[i-1]);
        if(((fingerprint & chunk_size) == BREAKMARK_VALUE && i >= MIN_CHUNK_SIZE) || i >= MAX_CHUNK_SIZE || i == len)
        {
            break;
        }
        else i++;
    }
    return i;
}

/* 
 * return how many bytes skipped */
inline int my_memcmp(unsigned char* x, unsigned char* y) 
{
    uint64_t __a = __builtin_bswap64(*((uint64_t *) x));
    uint64_t __b = __builtin_bswap64(*((uint64_t *) y));
    uint64_t __c = __b;
        while(__a <= __b){
            __a = (__a << 8) + 0xff;
            --__b;
        }
    return __c - __b;
}

int ae_chunk_data(unsigned char *p, int n)
{
    unsigned char *p_curr, *max_str = p;
   int comp_res = 0, i, end, max_pos = 0;
   if(n < WINDOW_SIZE - COMP_SIZE)
  		return n;
   end = n - COMP_SIZE;
   i = 1;
   p_curr = p + 1;
   while (i < end){
      int end_pos = max_pos + WINDOW_SIZE;
	  if (end_pos > end)
			end_pos = end;
	  while(*((uint64_t *)p_curr) <= *((uint64_t *)max_str) && i < end_pos){
		  p_curr++;
		  i++;
	  }
	  uint64_t _cur = *((uint64_t *) p_curr);
	  uint64_t _max = *((uint64_t *) max_str);
	  if (_cur > _max) {
		 max_str = p_curr;
		 max_pos = i;
	  }
	  else
		 return end_pos;
	  p_curr++;
	  i++;
   }
   return n;
}

void chunk_finger(unsigned char* buf, uint32_t len,unsigned char hash[])
{
   	SHA_CTX context;
   	unsigned char digest[20];
	SHA1_Init(&context);
	SHA1_Update(&context, buf, (int)len);
	SHA1_Final(digest,&context);
	memcpy(hash,digest,20);	
}

void Super_feature(unsigned char* data, uint32_t len, uint64_t SF[], int number)
{
	uint64_t SFeature, Feature[124];
	uint64_t fingerprint = 0;
	int i = 48, bufPos = -1, jj, ii;

	unsigned char om, Fbuf[256], *Tempbuf;   		
	unsigned char buf[128];
	memset ((char*) buf,0, 128);
	memset((char*)Feature, 0, sizeof(Feature));

	for(int j = 48; j >= 2; --j){
		SLIDE2(data[i-j], fingerprint, bufPos, buf);
	}

	while(i <= len)
	{
		SLIDE2(data[i-1], fingerprint, bufPos, buf);
		for(jj = 0; jj < number*NUMFEATURE; ++jj)
		{
			SFeature = (MiAi[jj][0]*fingerprint + MiAi[jj][1]) & GSIZE32BIT; 
			if(SFeature > Feature[jj]){
				Feature[jj] = SFeature;
 			}
		}
		i++;
 	}

	for(jj = 0; jj < number; ++jj)
	{	
		Tempbuf = (unsigned char*)(Feature + NUMFEATURE*jj);
		fingerprint =0; 
		bufPos = -1;
		memset((char*)buf, 0, 128);
		SF[jj] =SpookyHash::Hash64(Tempbuf, NUMFEATURE * SIMILAR_HASH_LEN, 0x7ff); 
	}	
	return;
 }
 

