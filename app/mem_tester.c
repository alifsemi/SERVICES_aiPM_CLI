#include <stdio.h>
#include <stdint.h>

void memory_set(const uint32_t mem_addr, uint32_t block_count, uint32_t block_size)
{
    uint32_t pointer_index, block_index;
    uint32_t test_value;

    block_size >>= 2; // convert number of bytes to number of 32-bit words
    for (block_index = 0; block_index < block_count; block_index++) {
        uint32_t *mem_pointer = (uint32_t *) mem_addr;
        mem_pointer += block_index * block_size;
        test_value = 0x1234;

        for (pointer_index = 0; pointer_index < block_size; pointer_index++) {
            *mem_pointer = test_value;
            mem_pointer++;
            test_value++;
        }
    }
}

int32_t memory_test(const uint32_t mem_addr, uint32_t block_count, uint32_t block_size)
{
    uint32_t bad_blocks, bad_words;
    uint32_t pointer_index, block_index;
    uint32_t test_value;

    bad_blocks = 0;
    block_size >>= 2; // convert number of bytes to number of 32-bit words
    for (block_index = 0; block_index < block_count; block_index++) {
        uint32_t *mem_pointer = (uint32_t *) mem_addr;
        mem_pointer += block_index * block_size;
        test_value = 0x1234;
        bad_words = 0;

        for (pointer_index = 0; pointer_index < block_size; pointer_index++) {
            if (*mem_pointer != test_value) {
                bad_words++;
            }
            mem_pointer++;
            test_value++;
        }

        if (bad_words) bad_blocks++;
        if (bad_words) printf("x");
        else printf("-");
    }
    printf("\r\n");
    return bad_blocks ? -1 : 0;
}
