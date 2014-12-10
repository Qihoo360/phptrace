#include<stdio.h>
#include<stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>
#include "../util/phptrace_mmap.h"
#include "../util/phptrace_protocol.h"

enum protocol_st{
    P_START,
    P_HEADER,
    P_RECORD,
    P_TAILER
};

void sigctrlc(sig){
    printf("\33[?25h");
    exit(0);
}
void repr_binary_data(char *bin, int len){
    if(len > 1024){
        len = 1024;
        bin[len] = '\0';
    }
    while(--len>0){
        if(!isprint(bin[len])){
            bin[len] = '.';
        }
    }
}
int main(int argc, char *argv[]){
    uint64_t seq, last;
    phptrace_segment_t tracelog;
    enum protocol_st st;
    void *addr;
    char out[1024*1024]; //FIXME this maybe not enogth as well

    phptrace_file_record_t record;
    phptrace_file_header_t header;
    phptrace_file_tailer_t tailer;

    if(argc != 2){
        printf("%s tracelog\n", argv[0]);
        return 1;
    }

    signal(SIGINT, sigctrlc);
    signal(SIGTERM, sigctrlc);
    signal(SIGSTOP, sigctrlc);

    tracelog = phptrace_mmap_read(argv[1]);
    if(tracelog.shmaddr == MAP_FAILED){
        perror("phptrace_mmap_read");
        return 1;
    }

    addr = tracelog.shmaddr;
    
    st = P_HEADER;
    last = 0;
    for(;;){
        seq = *(uint64_t *)addr;
        if(seq == (uint64_t)-1){
            if(!isatty(1)){
                break;
            }
            printf("\33[?25lwaiting \\\r");
            usleep(1000*2);
            printf("waiting -\r");
            usleep(1000*2);
            printf("waiting /\r");
            usleep(1000*2);
            printf("waiting |\r");
            usleep(1000*2);
            continue;
        }
        //printf("progress %lld/%lld\n", addr - tracelog.shmaddr, tracelog.size);
        if(seq == MAGIC_NUMBER_TAILER){
            st = P_TAILER;
        }

        if(st == P_HEADER){
            addr = phptrace_mem_read_header(&header, addr);
            printf("magic 0X%lX, version %d, flag %d\n", header.magic_number, header.version, header.flag);
            st = P_RECORD;
            continue;
        }
        if(st == P_RECORD){
            seq = *(uint64_t *)addr;
            if(last != 0 && seq != last + 1){
                printf("overlapped seq(0X%lX)\n", seq);
                break;
            }
            last = seq;
            addr = phptrace_mem_read_record(&record, addr, seq);
            if(!addr){
                printf("overlapped\n");
                break;
            }
            snprintf(out, record.func_name->len+1, "%s",record.func_name->data);
            printf("seq %lu level %d %s", record.seq,record.level, out);

            snprintf(out, record.params->len+1, "%s", record.params->data);
            repr_binary_data(out, strlen(out));
            printf("(%s)", out);

            snprintf(out, record.ret_values->len+1, "%s", record.ret_values->data);
            repr_binary_data(out, strlen(out));
            printf(" => %s", out);

            sprintf(out, "%f", record.time_cost/1000000.0);
            printf(" %s\n", out);

            continue;
        }
        if(st == P_TAILER){
            addr = phptrace_mem_read_tailer(&tailer, addr);
            snprintf(out, tailer.filename->len+1, "%s", tailer.filename->data);
            printf("magic 0X%lX, rotate %s\n", tailer.magic_number, out);
            st = P_HEADER;
            addr = tracelog.shmaddr;
            continue;
        }
    }
    return 0;
}
