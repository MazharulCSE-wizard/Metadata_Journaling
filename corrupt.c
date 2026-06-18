#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

#define BLOCK_SIZE   4096U
#define INODE_BMAP_IDX  17U   // inode bitmap lives at block 17

int main(void) {
    // 1. Open vsfs.img in read+write mode
    int fd = open("vsfs.img", O_RDWR);
    if (fd < 0) {
        perror("Error opening vsfs.img");
        return 1;
    }

    // 2. Read the inode bitmap block (block 17) into a buffer
    uint8_t bitmap[BLOCK_SIZE];
    lseek(fd, INODE_BMAP_IDX * BLOCK_SIZE, SEEK_SET);
    if (read(fd, bitmap, BLOCK_SIZE) != BLOCK_SIZE) {
        perror("Error reading inode bitmap");
        close(fd);
        return 1;
    }

    printf("Before corruption: bitmap[0] = 0x%02x\n", bitmap[0]);

    bitmap[0] |= (1 << 1);   

    printf("After  corruption: bitmap[0] = 0x%02x\n", bitmap[0]);

    lseek(fd, INODE_BMAP_IDX * BLOCK_SIZE, SEEK_SET);
    if (write(fd, bitmap, BLOCK_SIZE) != BLOCK_SIZE) {
        perror("Error writing inode bitmap");
        close(fd);
        return 1;
    }

    printf("\nCorruption done! Simulated a crash after bitmap write.\n");
    printf("inode bitmap says inode 1 is USED\n");
    printf("but inode table still shows inode 1 as FREE (type=0)\n");
    printf("and no directory entry points to inode 1\n");


    close(fd);
    return 0;
}
