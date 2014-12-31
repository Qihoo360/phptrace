#include "phptrace_ctrl.h"

int phptrace_ctrl_init(phptrace_ctrl_t *c)
{
    c->ctrl_seg = phptrace_mmap_write(PHPTRACE_LOG_DIR "/" PHPTRACE_CTRL_FILENAME);
    return (c->ctrl_seg.shmaddr != MAP_FAILED); 
}
void phptrace_ctrl_clean_all(phptrace_ctrl_t *c)
{
    if (c && c->ctrl_seg.shmaddr && c->ctrl_seg.shmaddr != MAP_FAILED) {
        memset(c->ctrl_seg.shmaddr, 0, c->ctrl_seg.size);
    }
}
void phptrace_ctrl_clean_one(phptrace_ctrl_t *c, int pid)
{
    if (c && c->ctrl_seg.shmaddr && c->ctrl_seg.shmaddr != MAP_FAILED) {
        phptrace_ctrl_set(c, 0, pid); 
    }
}
void phptrace_ctrl_destroy(phptrace_ctrl_t *c)
{
    if (c && c->ctrl_seg.shmaddr && c->ctrl_seg.shmaddr != MAP_FAILED) {
        phptrace_unmap(&(c->ctrl_seg));
    }
}
int8_t phptrace_ctrl_heartbeat_ping(phptrace_ctrl_t *c, int pid)
{ 
    int8_t s = -1;
    phptrace_ctrl_get(c, &s, pid);
    s = s | (1 << 7);
    phptrace_ctrl_set(c, (int8_t)s, pid);
    return s;
}
/* TODO c */
int8_t phptrace_ctrl_heartbeat_pong(phptrace_ctrl_t *c, int pid)
{
    int8_t s = -1;
    phptrace_ctrl_get(c, &s, pid);
    s &= ~(1 << 7) & 0x00FF;
    phptrace_ctrl_set(c, (int8_t)s, pid);
    return s;
}

