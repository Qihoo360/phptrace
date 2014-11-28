/*
 * @FILE:	definations' functions 
 * @DESC:   
 * @Author: 
 * @Change:
 */

#include "phptrace_protocol.h"

int phptrace_ctrl_init(phptrace_ctrl_t *c)
{
	c->size = MAX_PROCESS_NUMBER;
	c->used = 0;
	c->ctrl_seg = phptrace_mmap_read(MMAP_CTRL_FILENAME);
	return (c->ctrl_seg.shmaddr != MAP_FAILED); 
}
void phptrace_ctrl_clean_all(phptrace_ctrl_t *c)
{
	memset(c->ctrl_seg.shmaddr, 0, c->ctrl_seg.size);
}
void phptrace_ctrl_clean_one(phptrace_ctrl_t *c, int pid)
{
	if (c && c->ctrl_seg.shmaddr)
		phptrace_ctrl_set(c, 0, pid); 
		//*((uint8_t *)(c->ctrl_seg.shmaddr + pid)) = 0;
}
void phptrace_ctrl_free(phptrace_ctrl_t *c, int pid)
{
	if (c && c->ctrl_seg.shmaddr != MAP_FAILED)
	{ 
		phptrace_ctrl_clean_one(c, pid);
		phptrace_unmap(&(c->ctrl_seg));
		free(c);
	}
}
int8_t phptrace_ctrl_heart_beat(phptrace_ctrl_t *c, int pid)
{ 
	int8_t s = -1;
	phptrace_ctrl_get(c, &s, pid);
	if (s != CTRL_STATE_CLOSE)
	{
		if (s == CTRL_STATE_PING) s = CTRL_STATE_PONG;
		else s = CTRL_STATE_PONG;
		phptrace_ctrl_set(c, (uint8_t)s, pid);
	}
	return s;
}

void phptrace_file_init(phptrace_file_t *f)
{
	//f->records = (phptrace_file_record_t *)malloc(sizeof(phptrace_file_record_t) * MIN_RECORDS_NUMBER);
	//if (!f->records)
	//{
	//	printf ("[error] alloc failed!\n");
	//	exit(-1);
	//}
	//f->num_records = 0;
	//f->size = MIN_RECORDS_NUMBER;
	
	f->num_records = 0;
	f->records_base = NULL;
	f->records_top = NULL;
}
void phptrace_file_free(phptrace_file_t *f)
{
	if (f)
	{
	//	if (f->records) free (f->records);
		free (f);
	}
}

void phptrace_file_record_clean_str(phptrace_file_record_t *r)
{
	if (r)
	{
		if (r->func_name) phptrace_str_free((r->func_name));
		if (r->params) phptrace_str_free((r->params));
		if (r->ret_values) phptrace_str_free((r->ret_values));
	}
}
/*
void phptrace_file_shrink_records_queue(phptrace_file_t *f)
{
	int qs, qe;
	if (f)
	{
		for (; f->records_state[f->qe] == 0;
}
*/

/* allocate a record and push it to the stack */
phptrace_file_record_t *phptrace_file_record_new()
{
	phptrace_file_record_t* r = (phptrace_file_record_t *)malloc(sizeof(phptrace_file_record_t));
	if (!r)
	{
		printf ("[error] malloc failed!\n");
		exit(-1);
	}
	return r;
}
void phptrace_file_record_free(phptrace_file_record_t *r)
{
	if (r)
	{
		/* free string members in r if needed */
		free(r);
	}
}

void phptrace_file_record_push(phptrace_file_t *f, phptrace_file_record_t *r)
{
	if (f)
	{
		r->prev = f->records_top;
		f->records_top = r;
		if (f->records_base == NULL)
			f->records_base = f->records_top;
	}
}

phptrace_file_record_t* phptrace_file_record_top(phptrace_file_t *f) 
{
	phptrace_file_record_t* r = NULL;
	if (f)
		r = f->records_top;
	return r;
}

phptrace_file_record_t* phptrace_file_record_pop(phptrace_file_t *f) 
{
	phptrace_file_record_t *r = NULL;
	if (f)
	{
		//phptrace_record_clean_str(&(f->records[f->qs]));
		r = f->records_top;
		if (r)
		{
			f->records_top = r->prev;
			r->prev = NULL;
			if (f->records_top == NULL)
				f->records_base = NULL;
		}
	}
	return r;
}

void *phptrace_mem_write_header(phptrace_file_header_t *header, void *mem){
    void *waitaddr = mem;
    /*skip the waitflag*/
    mem += sizeof(uint64_t);

    *((uint8_t *)mem) = header->version;
    mem += sizeof(uint8_t);

    *((uint8_t *)mem) = header->flag;
    mem += sizeof(uint8_t);

    *((uint64_t *)waitaddr) = header->magic_number;
    return mem; 
}
void *phptrace_mem_write_record(phptrace_file_record_t *record, void *mem){
    void *waitaddr = mem;
    /*skip the waitflag*/
    mem += sizeof(uint64_t);

    *((uint16_t *)mem) = record->level;
    mem += sizeof(uint16_t);

    *((uint64_t *)mem) = record->start_time;
    mem += sizeof(uint64_t);

    if(record->func_name){
        *((uint32_t *)mem) = record->func_name->len;
        mem += sizeof(uint32_t);
        memcpy(mem, record->func_name->data, record->func_name->len);
        mem += record->func_name->len;
    }else{
        *((uint32_t *)mem) = 0;
        mem += sizeof(uint32_t);
    }

    if(record->params){
        *((uint32_t *)mem) = record->params->len;
        mem += sizeof(uint32_t);
        memcpy(mem, record->params->data, record->params->len);
        mem += record->params->len;
    }else{
        *((uint32_t *)mem) = 0;
        mem += sizeof(uint32_t);
    }

    if(record->ret_values){
        *((uint32_t *)mem) = record->ret_values->len;
        memcpy(mem, record->ret_values->data, RET_VALUE_SIZE);
    }else{
        /* zero the reserved chunk because maybe 
         * we never have an opportunity to write 
         * the return value*/
        memset(mem, 0, RET_VALUE_SIZE);
    }
    mem += RET_VALUE_SIZE;

    *((uint64_t *)mem) = record->time_cost;
    mem += sizeof(uint64_t);

    *((uint64_t *)waitaddr) = record->seq;
    return mem;
}

void *phptrace_mem_fix_record(phptrace_file_record_t *record, void *mem){
    memcpy(mem, record->ret_values, 
            record->ret_values->len + sizeof(uint32_t) > RET_VALUE_SIZE 
            ? RET_VALUE_SIZE
            : record->ret_values->len + sizeof(uint32_t));

    mem += RET_VALUE_SIZE;
    *((uint64_t *)mem) = record->time_cost;
}

void *phptrace_mem_write_waitflag(void *mem){
    *((uint64_t *)mem) = (uint64_t)-1;
    mem += sizeof(uint64_t);
    return mem;
}
void *phptrace_mem_write_tailer(phptrace_file_tailer_t *tailer, void *mem){
    void *waitaddr = mem;
    /*skip the waitflag*/
    mem += sizeof(uint64_t);

    *((uint32_t *)mem) = tailer->filename->len;
    mem += sizeof(uint32_t);
    memcpy(mem, tailer->filename->data, tailer->filename->len);
    mem += tailer->filename->len;

    *((uint64_t *)waitaddr) = tailer->magic_number;
    return mem;
}


void *phptrace_mem_read_header(phptrace_file_header_t *header, void *mem)
{
	header->magic_number = *((uint64_t *)mem);
    mem += sizeof(uint64_t);

    header->version = *((uint8_t *)mem);
    mem += sizeof(uint8_t);

    header->flag = *((uint8_t *)mem); 
    mem += sizeof(uint8_t);
    return mem; 
}
void *phptrace_mem_read_record(phptrace_file_record_t *record, void *mem)
{
    record->seq = *((uint64_t *)mem);
    mem += sizeof(uint64_t);

    record->level = *((uint16_t *)mem); 
    mem += sizeof(uint16_t);

    record->start_time = *((uint64_t *)mem);
    mem += sizeof(uint64_t);

    record->func_name = (phptrace_str_t *)mem; 
    mem += sizeof(uint32_t);
    mem += record->func_name->len;

    record->params = (phptrace_str_t *)mem; 
    mem += sizeof(uint32_t);
    mem += record->params->len;

	record->ret_values_ptr = (uint64_t *)mem;
    record->ret_values = (phptrace_str_t *)mem;
    mem += RET_VALUE_SIZE * sizeof(char);

    record->time_cost = *((uint64_t *)mem); 
    mem += sizeof(uint64_t);

    return mem;
}
void *phptrace_mem_update_record(phptrace_file_record_t *record, void *mem)
{
    record->ret_values->len = 0;
    record->ret_values = (phptrace_str_t *)mem;
    mem += RET_VALUE_SIZE * sizeof(char);

    record->time_cost = *((uint64_t *)mem); 
    mem += sizeof(uint64_t);

    return mem;
}

void *phptrace_mem_read_record_level(int16_t *level, void *mem)
{
    mem += sizeof(uint64_t);
    *level = *((uint16_t *)mem); 
    mem += sizeof(uint16_t);
	return mem;
}
void *phptrace_mem_read_64b(uint64_t *number, void *mem)
{
	*number = *((uint64_t *)mem);
	mem += sizeof(uint64_t);
	return mem;
}
void *phptrace_mem_write_8b(uint8_t number, void *mem)
{
    *((uint8_t *)mem) = number;
	mem += sizeof(uint8_t);
	return mem;
}
void *phptrace_mem_read_8b(uint8_t *number, void *mem)
{
	*number = *((uint8_t *)mem);
	mem += sizeof(uint8_t);
	return mem;
}
int phptrace_mem_is_waitflag(void *mem)
{
    return (*((uint64_t *)mem) == (uint64_t)(-1LL));
}
void *phptrace_mem_read_waitflag(uint64_t *waitflag, void *mem)
{
    *waitflag = *((uint64_t *)mem);
    mem += sizeof(uint64_t);
    return mem;
}
void *phptrace_mem_read_tailer(phptrace_file_tailer_t *tailer, void *mem)
{
    tailer->magic_number = *((uint64_t *)mem); 
	tailer->filename = (phptrace_str_t *)mem;
	mem += sizeof(uint32_t);
    mem += tailer->filename->len;
    return mem;
}

/* utils
 */

int string2uint(const char *str)
{       
	char *error;
	long value; 

	if (!*str)
		return -1; 
	errno = 0;
	value = strtol(str, &error, 10);
	if (errno || *error || value < 0 || (long)(int)value != value)
		return -1;
	return (int)value;
}   

#ifdef DEBUG

//#define dump_header(h, fp) \
//	fprintf (fp, "%lld%c%c", (h)->magic_number, (h)->version, (h)->flag);
//#define dump_tailer(t, fp) \
//	fprintf (fp, "%lld%u%p", (t)->magic_number, (t)->filename_len, (t)->file_ptr);
#define dump_header(h, fp) \
	fwrite(&((h)->magic_number), sizeof(uint64_t), 1, (fp));	\
	fwrite(&((h)->version), sizeof(uint8_t), 1, (fp));	\
	fwrite(&((h)->flag), sizeof(uint8_t), 1, (fp));	
#define dump_tailer(t, fp) \
	fwrite(&((t)->magic_number), sizeof(uint64_t), 1, (fp));	\
	fwrite(&((t)->filename->len), sizeof(uint32_t), 1, (fp));	\
	fwrite((t)->filename->data, sizeof(char), (t)->filename->len, (fp));	
void dump_record(phptrace_file_record_t *r, FILE *fp)
{
	int rm = 32;
	char ch = '\0';
	fwrite(&((r)->seq), sizeof(uint64_t), 1, (fp));	
	fwrite(&((r)->level), sizeof(uint16_t), 1, (fp));	
	fwrite(&((r)->start_time), sizeof(uint64_t), 1, (fp));
	fwrite(&((r)->func_name->len), sizeof(uint32_t), 1, (fp));
	fwrite((r)->func_name->data, sizeof(char), (r)->func_name->len, (fp));
	fwrite(&((r)->params->len), sizeof(uint32_t), 1, (fp));	
	fwrite((r)->params->data, sizeof(char), (r)->params->len, (fp));	
	fwrite(&((r)->ret_values->len), sizeof(uint32_t), 1, (fp));	
	fwrite((r)->ret_values->data, sizeof(char), (r)->ret_values->len, (fp));
	rm = rm - sizeof(uint32_t) - (r)->ret_values->len;	
	fwrite(&ch, sizeof(char), rm, (fp));
	fwrite(&((r)->time_cost), sizeof(uint64_t), 1, (fp));	
}

void dump2file(phptrace_file_t *f, phptrace_file_record_t *r, char *filename)
{
	int i;
	FILE *fp;
	fp = fopen(filename, "wb");
	if (!fp)
	{
		printf ("failed to open file");
		return;
	}
	
	//fwrite(&(f->header.magic_number), sizeof(uint64_t), 1, (fp));
	//fwrite(&(f->header.version), sizeof(uint8_t), 1, (fp));	
	dump_header(&(f->header), fp);
	
	//fwrite(&(f->records[0].func_name->len), sizeof(uint32_t), 1, (fp));	
	//fwrite(&((f->records[0].func_name)->len), sizeof(uint32_t), 1, (fp));	
	for (i = 0; i < f->num_records; i++)
	{
	//	dump_record(&(f->records[i]), fp);
		dump_record(r + i, fp);
	}
	dump_tailer(&(f->tailer), fp);
	/*
*/
	fclose(fp);
}

void pr_header(phptrace_file_header_t *h)
{
	printf ("[magic_header:0x%llx][version:%d][flag:%d]\n", h->magic_number, h->version, h->flag);
}
void pr_tailer(phptrace_file_tailer_t *t)
{
	printf ("[magic_tailer:0x%llx][file_len:%d][file_ptr:%d]\n", t->magic_number, t->filename->len,
			t->filename->data);
}
void pr_record(phptrace_file_record_t *r)
{
	r->ret_values->data[r->ret_values->len] = '\0';
	printf ("[seq:%lld][level:%d][time:%lld] [(%s) = %s(%s)]\n", 
			r->seq, r->level, r->start_time, (char *)r->ret_values->data, (char *)r->func_name->data, (char *)r->params->data);
}
void pr_file(phptrace_file_t *f, phptrace_file_record_t *r)
{
	int i;
	pr_header(&(f->header));	
	for (i = 0; i < f->num_records; i++)
	{
		printf ("------------\nRecord %d\n", i);
		pr_record(r + i);
		printf ("------------\n\n", i);
	}
	pr_tailer(&(f->tailer));
}
#endif
