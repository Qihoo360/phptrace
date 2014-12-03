#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "../util/phptrace_mmap.h"
#define PID_MAX 32768
int main(int argc, char *argv[]){
    int pid, i;
    uint8_t *flags, flag;
    char *cmd;
    if(argc != 2 && argc !=3){
        printf("Usage: %s pid [flag]\n", argv[0]);
        return 1;
    }
    phptrace_segment_t ctrl = phptrace_mmap_read("/tmp/phptrace.ctrl");
    flags = ctrl.shmaddr;

    cmd = argv[1];
    if(strncmp("scan", cmd, sizeof("scan")-1) == 0){
        for(i = 0; i < PID_MAX; ++i){
            flag = flags[i];
            if(flag!=0){
                printf("%d %d\n", i, flag);
            }
        } 
        return 0;
    }
    if(strncmp("clear", cmd, sizeof("clear")-1) == 0){
        for(i = 0; i < PID_MAX; ++i){
            if(flags[i]!=0){
                printf("set %d %d\n", i, 0);
            }
            flags[i] = 0;
        }
        return 0;
    }

    pid = atoi(argv[1]);

    if(argc==3){
        flag = atoi(argv[2]);
        flags[pid] = flag;
        printf("set %d %d\n", pid, flag);
    }else{
        flag = flags[pid];
        printf("get %d %d\n", pid, flag&0x00FF);
    }


    return 0;
}
