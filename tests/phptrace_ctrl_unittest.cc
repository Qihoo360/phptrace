#include<iostream>
#include "gtest/gtest.h"
extern "C"{
#include "phptrace_ctrl.h"
#include "sds/sds.h"
}

class PhptraceCtrl :  public testing::Test{
protected:
    virtual void SetUp(){
        ctrl.ctrl_seg.shmaddr = _buf;
        ctrl.ctrl_seg.size = PID_MAX;
    }
    virtual void TearDown(){
    }
    
    phptrace_ctrl_t ctrl;
    uint8_t _buf[PID_MAX];
};
TEST_F(PhptraceCtrl, clean_one)
{
    int pid = 1235;
    ((uint8_t*)ctrl.ctrl_seg.shmaddr)[pid] = 1;
    phptrace_ctrl_clean_one(&ctrl, pid);
    ASSERT_EQ(((uint8_t*)ctrl.ctrl_seg.shmaddr)[pid], 0);
}
TEST_F(PhptraceCtrl, clean_all)
{
    memset(ctrl.ctrl_seg.shmaddr, 1, PID_MAX);
    phptrace_ctrl_clean_all(&ctrl);

    for (int i = 0; i < PID_MAX; ++i) {
        ASSERT_EQ(((uint8_t*)ctrl.ctrl_seg.shmaddr)[i], 0);
    }
}

TEST_F(PhptraceCtrl, heartbeat_ping)
{
    int pid = 1235;
    ((uint8_t*)ctrl.ctrl_seg.shmaddr)[pid] = 1;
    phptrace_ctrl_heartbeat_ping(&ctrl, pid);
    ASSERT_EQ(((uint8_t*)ctrl.ctrl_seg.shmaddr)[pid], (1<<7) + 1);

    ((uint8_t*)ctrl.ctrl_seg.shmaddr)[pid] = 2;
    phptrace_ctrl_heartbeat_ping(&ctrl, pid);
    ASSERT_EQ(((uint8_t*)ctrl.ctrl_seg.shmaddr)[pid], (1<<7) + 2);
}

TEST_F(PhptraceCtrl, heartbeat_pong)
{
    int pid = 1235;
    ((uint8_t*)ctrl.ctrl_seg.shmaddr)[pid] = 1;
    phptrace_ctrl_heartbeat_pong(&ctrl, pid);
    ASSERT_EQ(((uint8_t*)ctrl.ctrl_seg.shmaddr)[pid], 1);

    ((uint8_t*)ctrl.ctrl_seg.shmaddr)[pid] = 129;
    phptrace_ctrl_heartbeat_pong(&ctrl, pid);
    ASSERT_EQ(((uint8_t*)ctrl.ctrl_seg.shmaddr)[pid], 1);
}

