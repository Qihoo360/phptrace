#include<iostream>
#include "gtest/gtest.h"
extern "C"{
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "phptrace_mmap.h"
#include "sds/sds.h"
}

class PhptraceMmap : public testing::Test{
protected:
    virtual void SetUp(){
        filename = NULL;
    }
    virtual void TearDown(){
        if (filename) {
            unlink(filename);
            sdsfree(filename);
        }
    }
    sds filename;
};
class PhptraceMmapRead   : public PhptraceMmap{};
class PhptraceMmapWrite  : public PhptraceMmap{};
class PhptraceMmapCreate : public PhptraceMmap{};

TEST_F(PhptraceMmapWrite, file_not_exist)
{
    filename = sdsnew("file_that_not_exist");
    phptrace_segment_t seg;

    seg = phptrace_mmap_write(filename);

    ASSERT_EQ(ENOENT, errno);
    ASSERT_EQ(MAP_FAILED, seg.shmaddr);
    ASSERT_EQ(0, seg.size);
}
TEST_F(PhptraceMmapWrite, file_permission_denied)
{
    filename = sdsnew("file_that_permisson_denied");
    phptrace_segment_t seg;
    int fd;

    /*creat file*/
    fd = open(filename, O_CREAT, 0);
    ASSERT_NE(-1, fd);

    seg = phptrace_mmap_write(filename);
    ASSERT_EQ(EACCES, errno);
    ASSERT_EQ(MAP_FAILED, seg.shmaddr)<< strerror(errno);
    ASSERT_EQ(0, seg.size);
    unlink(filename);
}
TEST_F(PhptraceMmapWrite, file_normal)
{
    int fd;
    phptrace_segment_t seg;
    filename = sdsnew("file_that_is_normal");

    /*creat file*/
    fd = open(filename, O_CREAT|O_RDWR, 0666);
    ASSERT_TRUE(fd > 0);
    ASSERT_EQ(sdslen(filename), write(fd, filename, sdslen(filename)))<< strerror(errno)
        << " (fd is:" << fd <<")";

    seg = phptrace_mmap_write(filename);
    ASSERT_NE(MAP_FAILED, seg.shmaddr) << strerror(errno);
    ASSERT_NE((void*)0, seg.shmaddr) << strerror(errno);
    ASSERT_EQ(sdslen(filename), seg.size);

    ((char *)seg.shmaddr)[0] = 't';
    ASSERT_EQ('t', ((char *)seg.shmaddr)[0]);

    unlink(filename);
}
TEST_F(PhptraceMmapRead, file_not_exist)
{
    phptrace_segment_t seg;
    filename = sdsnew("file_that_not_exist");

    seg = phptrace_mmap_read(filename);

    ASSERT_EQ(ENOENT, errno);
    ASSERT_EQ(MAP_FAILED, seg.shmaddr);
    ASSERT_EQ(0, seg.size);
}
TEST_F(PhptraceMmapRead, file_permission_denied)
{
    filename = sdsnew("file_that_permisson_denied");
    phptrace_segment_t seg;
    int fd;

    /*creat file*/
    fd = open(filename, O_CREAT, 0);
    ASSERT_NE(-1, fd);

    seg = phptrace_mmap_write(filename);
    ASSERT_EQ(EACCES, errno);
    ASSERT_EQ(MAP_FAILED, seg.shmaddr)<< strerror(errno);
    ASSERT_EQ(0, seg.size);
    unlink(filename);
}
TEST_F(PhptraceMmapRead, file_normal)
{
    int fd;
    phptrace_segment_t seg;
    filename = sdsnew("file_that_is_normal");

    /*creat file*/
    fd = open(filename, O_CREAT|O_RDWR, 0444);
    ASSERT_TRUE(fd > 0);
    ASSERT_EQ(sdslen(filename), write(fd, filename, sdslen(filename)))<< strerror(errno)
        << " (fd is:" << fd <<")";

    seg = phptrace_mmap_read(filename);
    ASSERT_NE(MAP_FAILED, seg.shmaddr) << strerror(errno);
    ASSERT_NE((void*)0, seg.shmaddr) << strerror(errno);
    ASSERT_EQ(sdslen(filename), seg.size);
    ASSERT_EQ('f', ((char *)seg.shmaddr)[0]);
    unlink(filename);
}

TEST_F(PhptraceMmapCreate, file_not_exist)
{
    phptrace_segment_t seg;
    filename = sdsnew("file_that_not_exist");

    seg = phptrace_mmap_create(filename, sdslen(filename));

    ASSERT_NE(MAP_FAILED, seg.shmaddr) << strerror(errno);
    ASSERT_NE((void*)0, seg.shmaddr) << strerror(errno);

    ((char *)seg.shmaddr)[0] = 't';
    ASSERT_EQ('t', ((char *)seg.shmaddr)[0]);
    unlink(filename);
}
TEST_F(PhptraceMmapCreate, file_already_exist)
{
    int fd;
    phptrace_segment_t seg;
    filename = sdsnew("file_that_is_exist");

    /*creat file*/
    fd = open(filename, O_CREAT|O_RDWR, 0666);
    ASSERT_TRUE(fd > 0);
    ASSERT_EQ(sdslen(filename), write(fd, filename, sdslen(filename)))<< strerror(errno)
        << " (fd is:" << fd <<")";

    seg = phptrace_mmap_create(filename, sdslen(filename));

    ASSERT_NE(MAP_FAILED, seg.shmaddr) << strerror(errno);
    ASSERT_NE((void*)0, seg.shmaddr) << strerror(errno)<<
        "(seg.shmaddr is "<< seg.shmaddr << ")";

    ASSERT_EQ(sdslen(filename), seg.size);
    ASSERT_EQ('f', ((char *)seg.shmaddr)[0]);

    ((char *)seg.shmaddr)[0] = 't';
    ASSERT_EQ('t', ((char *)seg.shmaddr)[0]);
    unlink(filename);
}
TEST_F(PhptraceMmapCreate, file_permission_denied){
    filename = sdsnew("file_that_permisson_denied");
    phptrace_segment_t seg;
    int fd;

    /*creat file*/
    fd = open(filename, O_CREAT|O_RDWR, 0444);
    ASSERT_TRUE(fd > 0);
    ASSERT_EQ(sdslen(filename), write(fd, filename, sdslen(filename)))<< strerror(errno)
        << " (fd is:" << fd <<")";

    seg = phptrace_mmap_create(filename, 1024);
    ASSERT_EQ(EACCES, errno);
    ASSERT_EQ(MAP_FAILED, seg.shmaddr)<< strerror(errno);
    ASSERT_EQ(0, seg.size);
    unlink(filename);
}
