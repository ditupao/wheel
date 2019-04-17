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

// search for a file entry in the tar file
void tar_find(u8 * tar, const char * name, u8 ** buff, usize * size) {
    *buff = NULL;
    *size = 0;

    usize offset = 0;
    while (1) {
        tar_hdr_t * hdr = (tar_hdr_t *) (tar + offset);

        if (hdr->name[0] == '\0') {
            for (int i = 0; i < 1024; ++i) {
                if (0 != *(tar + offset + i)) {
                    goto pass;  // this is not end-of-archive
                }
            }
            return; // we've found end-of-archive
        }

pass:
        usize len = 0;
        for (int i = 0; (i < 12) && (hdr->size[i] != '\0'); ++i) {
            len *= 8;
            len += hdr->size[i] - '0';
        }

        if (0 == strncmp(hdr->name, name, 100)) {
            // TODO: retrieve file size
            *buff = tar + offset + 512;
            *size = len;
            return;
        }
        offset += 512 + ROUND_UP(len, 512);
    }
}

void tar_dump(u8 * tar) {
    usize offset = 0;
    while (1) {
        tar_hdr_t * hdr = (tar_hdr_t *) (tar + offset);

        if (hdr->name[0] == '\0') {
            for (int i = 0; i < 1024; ++i) {
                if (0 != *(tar + offset + i)) {
                    goto pass;  // this is not end-of-archive
                }
            }
            return; // we've found end-of-archive
        }
pass:
        usize len = 0;
        for (int i = 0; (i < 12) && (hdr->size[i] != '\0'); ++i) {
            len *= 8;
            len += hdr->size[i] - '0';
        }

        dbg_print("entry: %s.\r\n", hdr->name);
        offset += 512 + ROUND_UP(len, 512);
    }
}