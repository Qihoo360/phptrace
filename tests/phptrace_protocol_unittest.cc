#include<iostream>
#include "gtest/gtest.h"
extern "C"{
#include "phptrace_protocol.h"
#include "sds/sds.h"
}

template <typename T>
T GET(uint8_t **m){
    T val;
    val = *(T *)(*m);
    *m += sizeof(T);
    return val;
}
sds GET_S(uint8_t **m, int len){
    sds s;
    s = sdsnewlen(*m, len);
    *m += len;
    return s;
}

template <typename T>
uint8_t* SET(uint8_t **m, T val){
    *(T *)(*m) = val;
    *m += sizeof(T);
    return *m;
}
uint8_t* SET_S(uint8_t **m, sds s){
    int len;
    len = sdslen(s);
    memcpy(*m, s, len);
    *m += len;
    return *m;
}
TEST(ProtocolWriteTest, phptrace_mem_write_header)
{
    phptrace_file_header_t header = {MAGIC_NUMBER_HEADER, 255, 256};
    uint8_t buf[128], *wr, *rd;
    wr = buf;
    wr = (uint8_t *)phptrace_mem_write_header(&header, wr);

    rd = buf;
    ASSERT_EQ(MAGIC_NUMBER_HEADER, GET<uint64_t>(&rd));
    ASSERT_EQ(255, GET<uint8_t>(&rd));
    /*test about overflow*/
    ASSERT_EQ(0, GET<uint8_t>(&rd));
}

TEST(ProtocolWriteTest, phptrace_mem_write_tailer)
{
    sds filename = sdsnew("/tmp/phptrace.log");
    phptrace_file_tailer_t tailer = {MAGIC_NUMBER_TAILER, filename};
    uint8_t buf[128], *wr, *rd;
    wr = buf;
    wr = (uint8_t *)phptrace_mem_write_tailer(&tailer, wr);

    rd = buf;
    ASSERT_EQ(MAGIC_NUMBER_TAILER, GET<uint64_t>(&rd));
    ASSERT_EQ(sdslen(filename), GET<uint32_t>(&rd));
    ASSERT_STREQ(filename, sdsnewlen(rd, sdslen(filename)));
}

TEST(ProtocolWriteTest, phptrace_mem_write_record_entry)
{
    phptrace_file_record_t record;
    uint8_t buf[128], *wr, *rd;
    uint32_t len;

    record.seq   = 1;
    record.flag  = RECORD_FLAG_ENTRY;
    record.level = 3;
    record.function_name = sdsnew("say");
    record.start_time = 1234;
    record.info.entry.params = sdsnew("(\"world\")");
    record.info.entry.filename = sdsnew("test.php");
    record.info.entry.lineno = 15;

    wr = buf;
    wr = (uint8_t *)phptrace_mem_write_record(&record, wr);

    rd = buf;
    ASSERT_EQ(record.seq, GET<uint64_t>(&rd));
    ASSERT_EQ(record.flag, GET<uint8_t>(&rd));
    ASSERT_EQ(record.level, GET<uint16_t>(&rd));
    ASSERT_EQ(sdslen(record.function_name), len = GET<uint32_t>(&rd));
    ASSERT_STREQ(record.function_name, GET_S(&rd, len));
    ASSERT_EQ(record.start_time, GET<uint64_t>(&rd));
    ASSERT_EQ(sdslen(record.info.entry.params), len = GET<uint32_t>(&rd));
    ASSERT_STREQ(record.info.entry.params, GET_S(&rd, len));
    ASSERT_EQ(sdslen(record.info.entry.filename), len = GET<uint32_t>(&rd));
    ASSERT_STREQ(record.info.entry.filename, GET_S(&rd, len));
    ASSERT_EQ(record.info.entry.lineno, GET<uint32_t>(&rd));
}
TEST(ProtocolWriteTest, phptrace_mem_write_record_exit)
{
    phptrace_file_record_t record;
    uint8_t buf[128], *wr, *rd;
    uint32_t len;

    record.seq   = 1;
    record.flag  = RECORD_FLAG_EXIT;
    record.level = 3;
    record.function_name = sdsnew("say");
    record.start_time = 12345;
    record.info.exit.return_value = sdsnew("hello world");
    record.info.exit.cost_time = 1234;

    wr = buf;
    wr = (uint8_t *)phptrace_mem_write_record(&record, wr);

    rd = buf;
    ASSERT_EQ(record.seq,   GET<uint64_t>(&rd));
    ASSERT_EQ(record.flag,  GET<uint8_t>(&rd));
    ASSERT_EQ(record.level, GET<uint16_t>(&rd));
    ASSERT_EQ(sdslen(record.function_name), len = GET<uint32_t>(&rd));
    ASSERT_STREQ(record.function_name, GET_S(&rd, len));
    ASSERT_EQ(record.start_time,   GET<uint64_t>(&rd));
    ASSERT_EQ(sdslen(record.info.exit.return_value), len = GET<uint32_t>(&rd));
    ASSERT_STREQ(record.info.exit.return_value, GET_S(&rd, len));
    ASSERT_EQ(record.info.exit.cost_time, GET<uint64_t>(&rd));
}
TEST(ProtocolReadTest, phptrace_mem_read_header)
{
    phptrace_file_header_t header;
    uint8_t buf[128], *wr, *rd;

    wr = buf;
    SET<uint64_t>(&wr, 123456);
    SET<uint8_t> (&wr, 1);
    SET<uint8_t> (&wr, 2);

    rd = buf;
    phptrace_mem_read_header(&header, rd);

    ASSERT_EQ(123456, header.magic_number);
    ASSERT_EQ(1, header.version);
    ASSERT_EQ(2, header.flag);
}
TEST(ProtocolReadTest, phptrace_mem_read_header_overflow){
    phptrace_file_header_t header;
    uint8_t buf[128], *wr, *rd;

    wr = buf;
    SET<uint64_t>(&wr, -1);
    SET<uint8_t> (&wr, -1);
    SET<uint8_t> (&wr, -1);

    rd = buf;
    phptrace_mem_read_header(&header, rd);

    ASSERT_EQ((uint64_t)-1, header.magic_number);
    ASSERT_EQ((uint8_t)-1,  header.version);
    ASSERT_EQ((uint8_t)-1,  header.flag);
}

TEST(ProtocolReadTest, phptrace_mem_read_tailer)
{
    int len;
    phptrace_file_tailer_t tailer;
    uint8_t buf[128], *wr, *rd;
    sds filename = sdsnew("phptrace.trace.1234");

    wr = buf;
    SET<uint64_t>(&wr, 123456);
    SET<uint32_t>(&wr, len = sdslen(filename));
    SET_S(&wr, filename);

    rd = buf;
    phptrace_mem_read_tailer(&tailer, rd);

    ASSERT_EQ(123456, tailer.magic_number);
    ASSERT_EQ(len, sdslen(tailer.filename));
    ASSERT_STREQ(filename, tailer.filename);
}
TEST(ProtocolReadTest, phptrace_mem_read_record_entry)
{
    phptrace_file_record_t record;
    uint8_t buf[128], *wr, *rd;

    uint64_t seq = 1;
    uint8_t flag = RECORD_FLAG_ENTRY;
    uint16_t level = 1;
    sds function_name = sdsnew("say");
    uint64_t start_time = 123456;
    sds params = sdsnew("(\"hello\")");
    sds filename = sdsnew("test.php");
    uint32_t lineno = 123;

    wr = buf;
    SET<uint64_t>(&wr, seq);
    SET<uint8_t> (&wr, flag);
    SET<uint16_t>(&wr, level);
    SET<uint32_t>(&wr, sdslen(function_name));
    SET_S(&wr, function_name);
    SET<uint64_t>(&wr, start_time);
    SET<uint32_t>(&wr, sdslen(params));
    SET_S(&wr, params);
    SET<uint32_t>(&wr, sdslen(filename));
    SET_S(&wr, filename);
    SET<uint32_t>(&wr, lineno);

    rd = buf;
    phptrace_mem_read_record(&record, rd, seq);

    ASSERT_EQ(seq, record.seq);
    ASSERT_EQ(flag, record.flag);
    ASSERT_EQ(level, record.level);
    ASSERT_STREQ(function_name,record.function_name);
    ASSERT_EQ(start_time, record.start_time);
    ASSERT_STREQ(params, record.info.entry.params);
    ASSERT_STREQ(filename, record.info.entry.filename);
    ASSERT_EQ(lineno, record.info.entry.lineno);
}
TEST(ProtocolReadTest, phptrace_mem_read_record_exit)
{
    phptrace_file_record_t record;
    uint8_t buf[128], *wr, *rd;

    uint64_t seq = 1;
    uint8_t flag = RECORD_FLAG_EXIT;
    uint16_t level = 1;
    uint64_t start_time = 123456;
    sds function_name = sdsnew("say");
    sds return_value = sdsnew("hello world");
    uint64_t cost_time = 1419493969;
    
    wr = buf;
    SET<uint64_t>(&wr, seq);
    SET<uint8_t> (&wr, flag);
    SET<uint16_t>(&wr, level);
    SET<uint32_t>(&wr, sdslen(function_name));
    SET_S(&wr, function_name);
    SET<uint64_t>(&wr, start_time);
    SET<uint32_t>(&wr, sdslen(return_value));
    SET_S(&wr, return_value);
    SET<uint64_t>(&wr, cost_time);

    rd = buf;
    phptrace_mem_read_record(&record, rd, seq);

    ASSERT_EQ(seq, record.seq);
    ASSERT_EQ(flag, record.flag);
    ASSERT_EQ(level, record.level);
    ASSERT_STREQ(function_name,record.function_name);
    ASSERT_EQ(start_time,record.start_time);
    ASSERT_STREQ(return_value,record.info.exit.return_value);
    ASSERT_EQ(cost_time, record.info.exit.cost_time);
}
