#include <stdlib.h>
#include <string.h>
#include "phptrace_type.h"

#define LEN_SDS(s) \
    (sizeof(uint32_t) + (s == NULL ? 0 : sdslen(s)))

#define PACK(buf, type, ele) \
    *(type *) buf = ele; buf += sizeof(type)

#define UNPACK(buf, type, ele) \
    ele = *(type *) buf; buf += sizeof(type)

#define PACK_SDS(buf, ele) if (ele == NULL) { \
    PACK(buf, uint32_t, 0); \
} else { \
    PACK(buf, uint32_t, sdslen(ele)); \
    memcpy(buf, ele, sdslen(ele)); \
    buf += sdslen(ele); \
}

#define UNPACK_SDS(buf, ele) UNPACK(buf, uint32_t, len); \
    if (len) { ele = sdsnewlen(buf, len); buf += len; }

size_t phptrace_type_len_frame(phptrace_frame *frame)
{
    int i;
    size_t size = 0;

    size += sizeof(uint8_t);                                  /* type */
    size += sizeof(uint8_t);                                  /* functype */
    size += sizeof(uint32_t);                                 /* lineno */

    size += LEN_SDS(frame->filename);                         /* filename */
    size += LEN_SDS(frame->class);                            /* class */
    size += LEN_SDS(frame->function);                         /* function */

    size += sizeof(uint32_t);                                 /* level */

    size += sizeof(uint32_t);                                 /* arg_count */
    for (i = 0; i < frame->arg_count; i++) {
        size += LEN_SDS(frame->args[i]);                      /* args */
    }
    size += LEN_SDS(frame->retval);                           /* retval */

    size += sizeof(uint64_t);                                 /* wall_time */
    size += sizeof(uint64_t);                                 /* cpu_time */
    size += sizeof(int64_t);                                  /* mem */
    size += sizeof(int64_t);                                  /* mempeak */

    size += sizeof(uint64_t);                                 /* wall_time */
    size += sizeof(uint64_t);                                 /* cpu_time */
    size += sizeof(int64_t);                                  /* mem */
    size += sizeof(int64_t);                                  /* mempeak */

    return size;
}

size_t phptrace_type_pack_frame(phptrace_frame *frame, char *buf)
{
    int i;
    char *ori = buf;

    PACK(buf, uint8_t, frame->type);
    PACK(buf, uint8_t, frame->functype);
    PACK(buf, uint32_t, frame->lineno);

    PACK_SDS(buf, frame->filename);
    PACK_SDS(buf, frame->class);
    PACK_SDS(buf, frame->function);

    PACK(buf, uint32_t, frame->level);

    PACK(buf, uint32_t, frame->arg_count);
    for (i = 0; i < frame->arg_count; i++) {
        PACK_SDS(buf, frame->args[i]);
    }
    PACK_SDS(buf, frame->retval);

    PACK(buf, uint64_t, frame->entry.wall_time);
    PACK(buf, uint64_t, frame->entry.cpu_time);
    PACK(buf, int64_t,  frame->entry.mem);
    PACK(buf, int64_t,  frame->entry.mempeak);

    PACK(buf, uint64_t, frame->exit.wall_time);
    PACK(buf, uint64_t, frame->exit.cpu_time);
    PACK(buf, int64_t,  frame->exit.mem);
    PACK(buf, int64_t,  frame->exit.mempeak);

    return buf - ori;
}

phptrace_frame *phptrace_type_unpack_frame(phptrace_frame *frame, char *buf)
{
    int i, len;

    UNPACK(buf, uint8_t, frame->type);
    UNPACK(buf, uint8_t, frame->functype);
    UNPACK(buf, uint32_t, frame->lineno);

    UNPACK_SDS(buf, frame->filename);
    UNPACK_SDS(buf, frame->class);
    UNPACK_SDS(buf, frame->function);

    UNPACK(buf, uint32_t, frame->level);

    UNPACK(buf, uint32_t, frame->arg_count);
    frame->args = calloc(frame->arg_count, sizeof(sds));
    for (i = 0; i < frame->arg_count; i++) {
        UNPACK_SDS(buf, frame->args[i]);
    }
    UNPACK_SDS(buf, frame->retval);

    UNPACK(buf, uint64_t, frame->entry.wall_time);
    UNPACK(buf, uint64_t, frame->entry.cpu_time);
    UNPACK(buf, int64_t,  frame->entry.mem);
    UNPACK(buf, int64_t,  frame->entry.mempeak);

    UNPACK(buf, uint64_t, frame->exit.wall_time);
    UNPACK(buf, uint64_t, frame->exit.cpu_time);
    UNPACK(buf, int64_t,  frame->exit.mem);
    UNPACK(buf, int64_t,  frame->exit.mempeak);

    return frame;
}

#if 0
#include <stdio.h>
int main(void)
{
    char buf[4096];
    size_t len;
    phptrace_frame frame;

    /* init */
    memset(&frame, 0, sizeof(frame));
    frame.filename = sdsnew("/tmp");
    frame.class = sdsnew("YourHome");
    frame.function = sdsnew("move");
    frame.arg_count = 3;
    frame.args = calloc(frame.arg_count, sizeof(sds));
    frame.args[0] = sdsnew("hello");
    frame.args[1] = sdsnew("world");
    frame.args[2] = sdsnew("true");
    /* frame.retval = sdsnew("false"); */
    frame.entry.mem = 654321;
    frame.exit.wall_time = 654321;
    printf("frame->arg_count: %d\n", frame.arg_count);

    /* len */
    len = phptrace_type_len_frame(&frame);
    printf("len: %ld\n", len);

    /* pack */
    len = phptrace_type_pack_frame(&frame, buf);
    printf("len: %ld\n", len);

    memset(&frame, 0, sizeof(frame));
    printf("frame->arg_count: %d\n", frame.arg_count);

    /* unpack */
    phptrace_type_unpack_frame(&frame, buf);
    printf("frame->arg_count: %d\n", frame.arg_count);
    printf("frame->args[0]: %s\n", frame.args[0]);
    printf("frame->args[1]: %s\n", frame.args[1]);
    printf("frame->args[2]: %s\n", frame.args[2]);
    printf("frame->entry.mem: %ld\n", frame.entry.mem);
    printf("frame->exit.wall_time: %ld\n", frame.exit.wall_time);
}
#endif
