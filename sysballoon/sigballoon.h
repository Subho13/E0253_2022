#ifndef SIGBALLOON_H
#define SIGBALLOON_H

#define SIGBALLOON 42
#define SWAP_FILE_SIZE (512*1024*1024) // 512 MB
// #define SWAP_PAGES_COUNT 128*1024
// #define SWAP_FILE_SIZE (1*1024*1024) // 1 MB
// #define SWAP_FILE_SIZE (32*1024*1024) // 32 MB
#define SWAP_PAGES_COUNT (SWAP_FILE_SIZE/(4*1024))

#endif
