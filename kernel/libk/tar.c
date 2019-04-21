#include <wheel.h>

typedef struct tar_hdr {
    char name    [100];
    char mode    [  8];
    char uid     [  8];
    char gid     [  8];
    char size    [ 12];
    char mtime   [ 12];
    char chksum  [  8];
    char linkflag;
    char linkname[100];
    char magic   [  8];
    char uname   [ 32];
    char gname   [ 32];
    char devmajor[  8];
    char devminor[  8];
} tar_hdr_t;

static int is_eoa(u8 * ptr) {
    for (int i = 0; i < 512; ++i) {
        if (0 != *(ptr + i)) {
            return NO;  // this is not end-of-archive
        }
    }
    return YES;
}

// search for a file entry in the tar file
int tar_find(u8 * tar, const char * name, u8 ** buff, usize * size) {
    *buff = NULL;
    *size = 0;

    usize offset = 0;
    while (1) {
        tar_hdr_t * hdr = (tar_hdr_t *) (tar + offset);
        if (hdr->name[0] == '\0') {
            if (is_eoa(tar + offset)) {
                return ERROR;
            }
        }

        usize filesize = 0;
        for (int i = 0; (i < 12) && (hdr->size[i] != '\0'); ++i) {
            filesize *= 8;
            filesize += hdr->size[i] - '0';
        }

        if (0 == strncmp(hdr->name, name, 100)) {
            // TODO: retrieve file size
            *buff = tar + offset + 512;
            *size = filesize;
            return OK;
        }

        offset += 512 + ROUND_UP(filesize, 512);
    }
}

// print all entries in the tar file
void tar_dump(u8 * tar) {
    usize offset = 0;
    while (1) {
        tar_hdr_t * hdr = (tar_hdr_t *) (tar + offset);
        if (hdr->name[0] == '\0') {
            if (is_eoa(tar + offset)) {
                return;
            }
        }

        usize filesize = 0;
        for (int i = 0; (i < 12) && (hdr->size[i] != '\0'); ++i) {
            filesize *= 8;
            filesize += hdr->size[i] - '0';
        }

        dbg_print("entry: %s, mode: %s, size: %d.\r\n", hdr->name, hdr->mode, filesize);
        offset += 512 + ROUND_UP(filesize, 512);
    }
}
