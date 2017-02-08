#ifndef _CLIENT_H
#define _CLIENT_H

void usage(char* program);
void default_folder_init();
bool args_parser(JCR *jcr, int argc, char* argv[]);
void handle_job_request(JCR *jcr);
void walk_dir(JCR *jcr, char * path); 
void* file_handler(void* arg);
void* chunk_handler(void* arg);
void update_jcr_journal(JCR *jcr);

#endif