#ifndef _COMPRESS_H_
#define _COMPRESS_H_

const uint32_t MC_VALID = 0x2016b509;

int update_fingerprint_offset(FingerChunk *fc);
void update_all_file_recipe(JCR *jcr);
void* update_recipe_offset_handler(void* arg);
bool content_reorganize(JCR* jcr);
bool content_reorganize2(JCR* jcr);
void final_compress(JCR* jcr);

//void DestFileRename(char* tmpfile, JCR* jcr); 

#endif
