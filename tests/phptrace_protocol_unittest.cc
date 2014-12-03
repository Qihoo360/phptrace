#include<iostream>
#include "gtest/gtest.h"
#include "util/phptrace_protocol.h"

TEST(ProtocolWriteTest, phptrace_mem_write_header){
    phptrace_file_header_t header = {MAGIC_NUMBER_HEADER, 255, 256};
    uint8_t buf[128], *wr, *rd;
    wr = buf;
    wr = (uint8_t *)phptrace_mem_write_header(&header, wr);

    rd = buf;
    ASSERT_EQ(*(uint64_t *)rd, MAGIC_NUMBER_HEADER);
    rd += sizeof(uint64_t);
    
    ASSERT_EQ(*(uint8_t *)rd, 255);
    rd += sizeof(uint8_t);

    ASSERT_EQ(*(uint8_t *)rd, 0);
}

TEST(ProtocolWriteTest, phptrace_mem_write_tailer){
    phptrace_file_tailer_t tailer = {MAGIC_NUMBER_TAILER, 0};
    uint8_t buf[128], *wr, *rd;
    wr = buf;
    wr = (uint8_t *)phptrace_mem_write_tailer(&tailer, wr);
}
