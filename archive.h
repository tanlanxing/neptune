#ifndef PHP_NEUTRON_ARCHIVE_H
#define PHP_NEUTRON_ARCHIVE_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <php.h>


// Tar format constants
#define TAR_BLOCK_SIZE 512
#define TAR_NAME_SIZE 100
#define TAR_MODE_SIZE 8
#define TAR_UID_SIZE 8
#define TAR_GID_SIZE 8
#define TAR_SIZE_SIZE 12
#define TAR_MTIME_SIZE 12
#define TAR_CHKSUM_SIZE 8
#define TAR_TYPEFLAG_SIZE 1
#define TAR_LINKNAME_SIZE 100
#define TAR_MAGIC_SIZE 6
#define TAR_VERSION_SIZE 2
#define TAR_UNAME_SIZE 32
#define TAR_GNAME_SIZE 32
#define TAR_DEVMAJOR_SIZE 8
#define TAR_DEVMINOR_SIZE 8
#define TAR_PREFIX_SIZE 155

// Tar header structure
typedef struct {
    char name[TAR_NAME_SIZE];
    char mode[TAR_MODE_SIZE];
    char uid[TAR_UID_SIZE];
    char gid[TAR_GID_SIZE];
    char size[TAR_SIZE_SIZE];
    char mtime[TAR_MTIME_SIZE];
    char chksum[TAR_CHKSUM_SIZE];
    char typeflag;
    char linkname[TAR_LINKNAME_SIZE];
    char magic[TAR_MAGIC_SIZE];
    char version[TAR_VERSION_SIZE];
    char uname[TAR_UNAME_SIZE];
    char gname[TAR_GNAME_SIZE];
    char devmajor[TAR_DEVMAJOR_SIZE];
    char devminor[TAR_DEVMINOR_SIZE];
    char prefix[TAR_PREFIX_SIZE];
    char padding[12];
} tar_header_t;


// Method declarations
PHP_METHOD(Neptune_Archive_Tar, __construct);
PHP_METHOD(Neptune_Archive_Tar, open);
PHP_METHOD(Neptune_Archive_Tar, readFile);
PHP_METHOD(Neptune_Archive_Tar, extractFile);
PHP_METHOD(Neptune_Archive_Tar, addFrom);

// Internal helper functions
static int validate_tar_format(FILE *fp);
static int read_tar_header(FILE *fp, tar_header_t *header);
static int write_tar_header(FILE *fp, tar_header_t *header);
static int calculate_checksum(tar_header_t *header);
static int octal_to_int(const char *str, size_t len);
static void int_to_octal(int num, char *str, size_t len);
static int find_file_in_tar(FILE *fp, const char *filename, tar_header_t *header);
static int extract_file_data(FILE *src, FILE *dst, size_t size);
static int copy_file_data(FILE *src, FILE *dst, size_t size);
static int mkdir_recursive(const char *path, mode_t mode);
void neptune_archive_init(void);
#endif /* PHP_NEUTRON_ARCHIVE_H */