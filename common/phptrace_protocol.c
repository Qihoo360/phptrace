/*
 * @FILE:    definations' functions 
 */
#include "phptrace_protocol.h"

void *phptrace_mem_write_header(phptrace_file_header_t *header, void *mem)
{
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

void *phptrace_mem_write_record(phptrace_file_record_t *record, void *mem)
{
    uint32_t len;
    void *waitaddr = mem;
    /*skip the waitflag*/
    mem += sizeof(uint64_t);

    *((uint8_t *)mem) = record->flag;
    mem += sizeof(uint8_t);

    *((uint16_t *)mem) = record->level;
    mem += sizeof(uint16_t);

    if (record->function_name) {
        len = sdslen(record->function_name);
        *((uint32_t *)mem) = len;
        mem += sizeof(uint32_t);
        memcpy(mem, record->function_name, len);
        mem += len;
    } else {
        *((uint32_t *)mem) = 0;
        mem += sizeof(uint32_t);
    }

    *((uint64_t *)mem) = record->start_time;
    mem += sizeof(uint64_t);

    if (record->flag == RECORD_FLAG_ENTRY) {
        if (RECORD_ENTRY(record, params)) {
            len = sdslen(RECORD_ENTRY(record, params));
            *((uint32_t *)mem) = len;
            mem += sizeof(uint32_t);
            memcpy(mem, RECORD_ENTRY(record, params), len);
            mem += len;
        } else {
            *((uint32_t *)mem) = 0;
            mem += sizeof(uint32_t);
        }

        if (RECORD_ENTRY(record, filename)) {
            len = sdslen(RECORD_ENTRY(record, filename));
            *((uint32_t *)mem) = len;
            mem += sizeof(uint32_t);
            memcpy(mem, RECORD_ENTRY(record, filename), len);
            mem += len;
        } else {
            *((uint32_t *)mem) = 0;
            mem += sizeof(uint32_t);
        }

        *((uint32_t *)mem) = RECORD_ENTRY(record, lineno);
        mem += sizeof(uint32_t);
    } else {
        if (RECORD_EXIT(record, return_value)) {
            len = sdslen(RECORD_EXIT(record, return_value));
            *((uint32_t *)mem) = len;
            mem += sizeof(uint32_t);
            memcpy(mem, RECORD_EXIT(record, return_value), len);
            mem += len;
        } else {
            *((uint32_t *)mem) = 0;
            mem += sizeof(uint32_t);
        }

        *((uint64_t *)mem) = RECORD_EXIT(record, cost_time);
        mem += sizeof(uint64_t);

        *((uint64_t *)mem) = RECORD_EXIT(record, cpu_time);
        mem += sizeof(uint64_t);

        *((int64_t *)mem) = RECORD_EXIT(record, memory_usage);
        mem += sizeof(int64_t);

        *((int64_t *)mem) = RECORD_EXIT(record, memory_peak_usage);
        mem += sizeof(int64_t);
    }

    *((uint64_t *)waitaddr) = record->seq;
    return mem;
}

void *phptrace_mem_write_waitflag(void *mem)
{
    *((uint64_t *)mem) = (uint64_t)-1;
    mem += sizeof(uint64_t);
    return mem;
}
void *phptrace_mem_write_tailer(phptrace_file_tailer_t *tailer, void *mem)
{
    uint32_t len;
    void *waitaddr = mem;
    /*skip the waitflag*/
    mem += sizeof(uint64_t);

    if (tailer->filename) {
        len = sdslen(tailer->filename);
        *((uint32_t *)mem) = len;
        mem += sizeof(uint32_t);
        memcpy(mem, tailer->filename, len);
        mem += len;
    } else {
        *((uint32_t *)mem) = 0;
        mem += sizeof(uint32_t);
    }

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

size_t phptrace_record_rawsize(phptrace_file_record_t *record)
{
    size_t raw_size = 0;

    raw_size += sizeof(uint64_t);                       /* seq */
    raw_size += sizeof(uint8_t);                        /* flag */
    raw_size += sizeof(uint16_t);                       /* level */

    raw_size += sizeof(uint32_t);                       /* function name */
    raw_size += sdslen(record->function_name);

    raw_size += sizeof(uint64_t);                       /* start time */

    if (record->flag == RECORD_FLAG_ENTRY) {            /* entry : */
        raw_size += sizeof(uint32_t);
        raw_size += sdslen(RECORD_ENTRY(record, params));        /* params */

        raw_size += sizeof(uint32_t);                            /* filename */
        raw_size += sdslen(RECORD_ENTRY(record, filename));

        raw_size += sizeof(uint32_t);                            /* lineno */
    } else {                                            /* exit : */
        raw_size += sizeof(uint32_t);                            /* return value */
        raw_size += sdslen(RECORD_EXIT(record, return_value));

        raw_size += sizeof(uint64_t);                            /* cost time */
        raw_size += sizeof(uint64_t);                            /* cpu time */
        raw_size += sizeof(int64_t);                             /* memory usage */
        raw_size += sizeof(int64_t);                             /* memory peak usage */
    }
    return raw_size;
}

void *phptrace_mem_read_record(phptrace_file_record_t *record, void *mem, uint64_t seq)
{
    uint32_t len;
    void *seqaddr = mem;
    if (*((uint64_t *)mem)!=seq) {
        return NULL;
    }
    record->seq = *((uint64_t *)mem);
    mem += sizeof(uint64_t);

    if (*((uint64_t *)seqaddr) != seq) {
        return NULL;
    }
    record->flag = *((uint8_t *)mem); 
    mem += sizeof(uint8_t);

    if (*((uint64_t *)seqaddr)!=seq) {
        return NULL;
    }
    record->level = *((uint16_t *)mem); 
    mem += sizeof(uint16_t);

    if (*((uint64_t *)seqaddr) != seq) {
        return NULL;
    }
    len = *(uint32_t *)mem;
    mem += sizeof(uint32_t);
    record->function_name = sdsnewlen(mem, len);
    mem += len;

    if (*((uint64_t *)seqaddr)!=seq) {
        return NULL;
    }
    record->start_time = *((uint64_t *)mem);
    mem += sizeof(uint64_t);

    if (record->flag == RECORD_FLAG_ENTRY) {
        if (*((uint64_t *)seqaddr)!=seq) {
            return NULL;
        }
        len = *(uint32_t *)mem;
        mem += sizeof(uint32_t);
        RECORD_ENTRY(record, params) = sdsnewlen(mem, len); 
        mem += len;

        if (*((uint64_t *)seqaddr)!=seq) {
            return NULL;
        }
        len = *(uint32_t *)mem;
        mem += sizeof(uint32_t);
        RECORD_ENTRY(record, filename) = sdsnewlen(mem, len); 
        mem += len;
        
        if (*((uint32_t *)seqaddr)!=seq) {
            return NULL;
        }
        RECORD_ENTRY(record, lineno) = *((uint32_t *)mem);
        mem += sizeof(uint32_t);
    } else {
        if (*((uint64_t *)seqaddr)!=seq) {
            return NULL;
        }
        len = *(uint32_t *)mem;
        mem += sizeof(uint32_t);
        RECORD_EXIT(record, return_value) = sdsnewlen(mem, len);
        mem += len;

        if (*((uint64_t *)seqaddr)!=seq) {
            return NULL;
        }
        RECORD_EXIT(record, cost_time) = *((uint64_t *)mem); 
        mem += sizeof(uint64_t);

        RECORD_EXIT(record, cpu_time) = *((uint64_t *)mem);
        mem += sizeof(uint64_t);

        RECORD_EXIT(record, memory_usage) = *((int64_t *)mem);
        mem += sizeof(int64_t);

        RECORD_EXIT(record, memory_peak_usage) = *((int64_t *)mem);
        mem += sizeof(int64_t);
    }
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
    uint32_t len;
    tailer->magic_number = *((uint64_t *)mem); 
    mem += sizeof(uint64_t);
    len = *(uint32_t *)mem;
    mem += sizeof(uint32_t);
    if (len > 0) {
        tailer->filename = sdsnewlen(mem, len);
    } else {
        tailer->filename = NULL;
    }
    mem += len;
    return mem;
}
