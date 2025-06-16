#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include "php.h"
#include "Zend/zend_exceptions.h"
#include "Zend/zend_types.h"
#include "ext/spl/spl_exceptions.h"
#include "php_neptune.h"
#include "archive.h"

// Exception class definitions
zend_class_entry *neptune_tar_file_not_found_exception_ce;
zend_class_entry *neptune_illegal_tar_format_exception_ce;
zend_class_entry *neptune_tar_ce;

// Tar class property definitions
static zend_object_handlers neptune_tar_handlers;

typedef struct {
    zend_string *tarFile;
    zend_string *mode;
    FILE *fp;
    zend_long size;
    zend_long origin_size;
    zend_object std;
} neptune_tar_t;

#define Z_NEPTUNE_TAR_P(zv) \
    ((neptune_tar_t*)((char *)(Z_OBJ_P(zv)) - XtOffsetOf(neptune_tar_t, std)))

zend_object *neptune_archive_tar_new(zend_class_entry *ce)
{
    neptune_tar_t *tar = ecalloc(1, sizeof(neptune_tar_t) + zend_object_properties_size(ce));

    zend_object_std_init(&tar->std, ce);
    tar->std.handlers = &neptune_tar_handlers;
    return &tar->std;
}

// Forward declarations
static void neptune_tar_free_storage(neptune_tar_t *tar);
static zend_object *neptune_tar_create_object(zend_class_entry *class_type);

// Helper function to convert octal string to integer
static int octal_to_int(const char *str, size_t len) {
    int result = 0;
    while (len > 0 && isdigit(*str)) {
        result = (result << 3) | (*str - '0');
        str++;
        len--;
    }
    return result;
}

// Helper function to convert integer to octal string
static void int_to_octal(int num, char *str, size_t len) {
    char tmp[32];
    int i = 0;

    // Convert to octal
    do {
        tmp[i++] = '0' + (num & 7);
        num >>= 3;
    } while (num > 0);

    // Pad with zeros
    while (i < len - 1) {
        tmp[i++] = '0';
    }

    // Reverse and null terminate
    tmp[i] = '\0';
    for (int j = 0; j < i / 2; j++) {
        char t = tmp[j];
        tmp[j] = tmp[i - 1 - j];
        tmp[i - 1 - j] = t;
    }

    strncpy(str, tmp, len - 1);
    str[len - 1] = '\0';
}

// Calculate tar header checksum
static int calculate_checksum(tar_header_t *header) {
    int sum = 0;
    char *p = (char *)header;

    // Set checksum field to spaces
    /// *** But it will be change original data, that will be used in other places ***
    // memset(header->chksum, ' ', TAR_CHKSUM_SIZE);

    // Calculate sum
    for (size_t i = 0; i < sizeof(tar_header_t); i++) {
        // First ignore chksum, later we will uniformly add 256 (8 spaces, 8 * 32 = 256)
        if (i > 147 && i < 156) {
            continue;
        }
        sum += (unsigned char)p[i];
    }

    return sum + 256;
}

// Read tar header from file
static int read_tar_header(FILE *fp, tar_header_t *header) {
    if (fread(header, 1, sizeof(tar_header_t), fp) != sizeof(tar_header_t)) {
        return -1;
    }

    // Verify checksum
    int checksum = calculate_checksum(header);
    int stored_checksum = octal_to_int(header->chksum, TAR_CHKSUM_SIZE);

    return (checksum == stored_checksum) ? 0 : -1;
}

// Write tar header to file
static int write_tar_header(FILE *fp, tar_header_t *header) {
    // Calculate and set checksum
    int checksum = calculate_checksum(header);
    int_to_octal(checksum, header->chksum, TAR_CHKSUM_SIZE);

    return fwrite(header, 1, sizeof(tar_header_t), fp) == sizeof(tar_header_t) ? 0 : -1;
}

// Validate tar file format
static int validate_tar_format(FILE *fp) {
    tar_header_t header;
    long pos = ftell(fp);

    // Read first header
    if (read_tar_header(fp, &header) != 0) {
        fseek(fp, pos, SEEK_SET);
        return -1;
    }

    // Check magic number
    if (memcmp(header.magic, "ustar", 5) != 0) {
        fseek(fp, pos, SEEK_SET);
        return -1;
    }

    fseek(fp, pos, SEEK_SET);
    return 0;
}

// Find file in tar archive
static int find_file_in_tar(FILE *fp, const char *filename, tar_header_t *header) {
    long pos = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    while (read_tar_header(fp, header) == 0) {
        if (strcmp(header->name, filename) == 0) {
            return 0;
        }

        // Skip file data
        size_t size = octal_to_int(header->size, TAR_SIZE_SIZE);
        size_t blocks = (size + TAR_BLOCK_SIZE - 1) / TAR_BLOCK_SIZE;
        fseek(fp, blocks * TAR_BLOCK_SIZE, SEEK_CUR);
    }

    fseek(fp, pos, SEEK_SET);
    return -1;
}

// Extract file data
static int extract_file_data(FILE *src, FILE *dst, size_t size) {
    char buffer[TAR_BLOCK_SIZE];
    size_t remaining = size;

    while (remaining > 0) {
        size_t to_read = (remaining > TAR_BLOCK_SIZE) ? TAR_BLOCK_SIZE : remaining;
        size_t read = fread(buffer, 1, to_read, src);

        if (read != to_read) {
            return -1;
        }

        if (fwrite(buffer, 1, read, dst) != read) {
            return -1;
        }

        remaining -= read;
    }

    // Skip padding
    if (size % TAR_BLOCK_SIZE != 0) {
        fseek(src, TAR_BLOCK_SIZE - (size % TAR_BLOCK_SIZE), SEEK_CUR);
    }

    return 0;
}

// Copy file data between tar files
static int copy_file_data(FILE *src, FILE *dst, size_t size) {
    return extract_file_data(src, dst, size);
}

// Recursive directory creation
static int mkdir_recursive(const char *path, mode_t mode) {
    char tmp[PATH_MAX];
    char *p = NULL;
    size_t len;
    int ret = 0;

    // Copy path to tmp buffer
    len = strlen(path);
    if (len > sizeof(tmp) - 1) {
        return -1;
    }
    memcpy(tmp, path, len);
    tmp[len] = '\0';

    // Remove trailing slash
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = '\0';
    }

    // Create directories recursively
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (access(tmp, F_OK) != 0) {
                ret = mkdir(tmp, mode);
                if (ret != 0 && errno != EEXIST) {
                    return ret;
                }
            }
            *p = '/';
        }
    }

    // Create final directory
    if (access(tmp, F_OK) != 0) {
        ret = mkdir(tmp, mode);
        if (ret != 0 && errno != EEXIST) {
            return ret;
        }
    }

    return 0;
}

// Object handlers
static void neptune_tar_free_storage(neptune_tar_t *tar)
{
    if (tar->fp) {
        fclose(tar->fp);
    }
    if (tar->tarFile) {
        zend_string_release(tar->tarFile);
    }
    if (tar->mode) {
        zend_string_release(tar->mode);
    }

    zend_object_std_dtor(&tar->std);
}

static zend_object *neptune_tar_create_object(zend_class_entry *class_type)
{
    neptune_tar_t *tar = ecalloc(1, sizeof(neptune_tar_t) + zend_object_properties_size(class_type));

    zend_object_std_init(&tar->std, class_type);
    object_properties_init(&tar->std, class_type);

    tar->std.handlers = &neptune_tar_handlers;

    return &tar->std;
}

// Open method implementation
static void neptune_tar_open_internal(neptune_tar_t *tar)
{
    FILE * tmp_fp;
    if (tar->fp) {
        fclose(tar->fp);
    }

    if (ZSTR_VAL(tar->mode)[0] == 'w') {
        // Create directory if it doesn't exist
        char *dir = estrdup(ZSTR_VAL(tar->tarFile));
        char *last_slash = strrchr(dir, '/');
        if (last_slash) {
            *last_slash = '\0';
            if (mkdir_recursive(dir, 0755) != 0) {
                efree(dir);
                zend_throw_exception(zend_ce_exception, "Failed to create directory", 0);
                return;
            }
        }
        efree(dir);
        if (access(ZSTR_VAL(tar->tarFile), F_OK) == -1) {
            tmp_fp = fopen(ZSTR_VAL(tar->tarFile), "w");
            if (!tmp_fp) {
                zend_throw_exception(zend_ce_exception, "Failed to create tar file", 0);
                return;
            } else {
                fclose(tmp_fp);
            }
        }
    }

    tar->fp = fopen(ZSTR_VAL(tar->tarFile), "rb+");
    if (!tar->fp) {
        zend_throw_exception(zend_ce_exception, "Failed to open tar file", 0);
        return;
    }

    // Get file size
    fseek(tar->fp, 0, SEEK_END);
    tar->size = ftell(tar->fp);
    fseek(tar->fp, 0, SEEK_SET);
}

// Constructor implementation
PHP_METHOD(Neptune_Archive_Tar, __construct)
{
    zend_string *tarFile, *mode;
    neptune_tar_t *tar = Z_NEPTUNE_TAR_P(ZEND_THIS);

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STR(tarFile)
        Z_PARAM_STR(mode)
    ZEND_PARSE_PARAMETERS_END();

    // Validate mode
    if (ZSTR_LEN(mode) != 1 || (ZSTR_VAL(mode)[0] != 'r' && ZSTR_VAL(mode)[0] != 'w')) {
        zend_throw_exception(spl_ce_InvalidArgumentException, "Mode must be either 'r' or 'w'", 0);
        return;
    }

    // Check file existence for read mode
    if (ZSTR_VAL(mode)[0] == 'r') {
        if (access(ZSTR_VAL(tarFile), F_OK) == -1) {
            zend_throw_exception(neptune_tar_file_not_found_exception_ce, "Tar file not found", 0);
            return;
        }

        // Open file for format validation
        FILE *fp = fopen(ZSTR_VAL(tarFile), "rb");
        if (!fp) {
            zend_throw_exception(zend_ce_exception, "Failed to open tar file for validation", 0);
            return;
        }

        // Validate tar format
        if (validate_tar_format(fp) != 0) {
            fclose(fp);
            zend_throw_exception(neptune_illegal_tar_format_exception_ce, "Invalid tar file format", 0);
            return;
        }

        fclose(fp);
    }

    // Store properties
    tar->tarFile = zend_string_copy(tarFile);
    tar->mode = zend_string_copy(mode);
    tar->fp = NULL;
    tar->size = 0;
    tar->origin_size = 0;

    // Open the file
    neptune_tar_open_internal(tar);
}

ZEND_BEGIN_ARG_INFO(arginfo_neptune_tar_construct, 0)
    ZEND_ARG_INFO(0, tarFile)
    ZEND_ARG_INFO(0, mode)
ZEND_END_ARG_INFO()

PHP_METHOD(Neptune_Archive_Tar, __destruct)
{
    neptune_tar_t *tar = Z_NEPTUNE_TAR_P(ZEND_THIS);
    neptune_tar_free_storage(tar);
}

ZEND_BEGIN_ARG_INFO(arginfo_neptune_tar_destruct, 0)
ZEND_END_ARG_INFO()

// Read file method implementation
PHP_METHOD(Neptune_Archive_Tar, readFile)
{
    zend_string *filename;
    tar_header_t header;
    zend_string *result;
    neptune_tar_t *tar = Z_NEPTUNE_TAR_P(ZEND_THIS);

    ZEND_PARSE_PARAMETERS_START(1, 1)
        Z_PARAM_STR(filename)
    ZEND_PARSE_PARAMETERS_END();

    if (!tar->fp) {
        zend_throw_exception(zend_ce_exception, "Tar file not opened", 0);
        return;
    }

    // Find file in tar
    if (find_file_in_tar(tar->fp, ZSTR_VAL(filename), &header) != 0) {
        zend_throw_exception(zend_ce_exception, "File not found in tar archive", 0);
        return;
    }

    // Get file size
    size_t size = octal_to_int(header.size, TAR_SIZE_SIZE);

    // Allocate buffer for file data
    result = zend_string_alloc(size, 0);

    // Read file data
    if (fread(ZSTR_VAL(result), 1, size, tar->fp) != size) {
        zend_string_release(result);
        zend_throw_exception(zend_ce_exception, "Failed to read file data", 0);
        return;
    }

    // Skip padding
    if (size % TAR_BLOCK_SIZE != 0) {
        fseek(tar->fp, TAR_BLOCK_SIZE - (size % TAR_BLOCK_SIZE), SEEK_CUR);
    }

    ZSTR_LEN(result) = size;
    ZSTR_VAL(result)[size] = '\0';

    RETURN_STR(result);
}

ZEND_BEGIN_ARG_INFO(arginfo_neptune_tar_readFile, 0)
    ZEND_ARG_INFO(0, filename)
ZEND_END_ARG_INFO()

// Extract file method implementation
PHP_METHOD(Neptune_Archive_Tar, extractFile)
{
    zend_string *filename, *path;
    tar_header_t header;
    FILE *outfile;
    struct stat st;
    neptune_tar_t *tar = Z_NEPTUNE_TAR_P(ZEND_THIS);

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_STR(filename)
        Z_PARAM_STR(path)
    ZEND_PARSE_PARAMETERS_END();

    if (!tar->fp) {
        zend_throw_exception(zend_ce_exception, "Tar file not opened", 0);
        return;
    }

    // Find file in tar
    if (find_file_in_tar(tar->fp, ZSTR_VAL(filename), &header) != 0) {
        zend_throw_exception(zend_ce_exception, "File not found in tar archive", 0);
        return;
    }

    // Create output directory if needed
    char *dir = estrdup(ZSTR_VAL(path));
    char *last_slash = strrchr(dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        if (mkdir_recursive(dir, 0755) != 0) {
            efree(dir);
            zend_throw_exception(zend_ce_exception, "Failed to create output directory", 0);
            return;
        }
    }
    efree(dir);

    // Open output file
    outfile = fopen(ZSTR_VAL(path), "wb");
    if (!outfile) {
        zend_throw_exception(zend_ce_exception, "Failed to create output file", 0);
        return;
    }

    // Extract file data
    size_t size = octal_to_int(header.size, TAR_SIZE_SIZE);
    if (extract_file_data(tar->fp, outfile, size) != 0) {
        fclose(outfile);
        zend_throw_exception(zend_ce_exception, "Failed to extract file data", 0);
        return;
    }

    fclose(outfile);

    // Set file permissions and ownership
    chmod(ZSTR_VAL(path), octal_to_int(header.mode, TAR_MODE_SIZE));
    chown(ZSTR_VAL(path),
          octal_to_int(header.uid, TAR_UID_SIZE),
          octal_to_int(header.gid, TAR_GID_SIZE));

    // Set modification time
    struct timespec times[2];
    times[0].tv_sec = times[1].tv_sec = octal_to_int(header.mtime, TAR_MTIME_SIZE);
    times[0].tv_nsec = times[1].tv_nsec = 0;
    utimensat(AT_FDCWD, ZSTR_VAL(path), times, 0);
}
ZEND_BEGIN_ARG_INFO(arginfo_neptune_tar_extractFile, 0)
    ZEND_ARG_INFO(0, filename)
    ZEND_ARG_INFO(0, path)
ZEND_END_ARG_INFO()

// Add from method implementation
PHP_METHOD(Neptune_Archive_Tar, addFrom)
{
    neptune_tar_t *tar, *source_tar;
    zend_string *filename;
    zval *source_tar_obj;
    tar_header_t header;
    long pos;

    ZEND_PARSE_PARAMETERS_START(2, 2)
        Z_PARAM_OBJECT(source_tar_obj)
        Z_PARAM_STR(filename)
    ZEND_PARSE_PARAMETERS_END();

    tar = Z_NEPTUNE_TAR_P(ZEND_THIS);;
    source_tar = Z_NEPTUNE_TAR_P(source_tar_obj);

    if (!tar->fp || !source_tar->fp) {
        zend_throw_exception(zend_ce_exception, "Tar file not opened", 0);
        return;
    }

    // Find file in source tar
    if (find_file_in_tar(source_tar->fp, ZSTR_VAL(filename), &header) != 0) {
        zend_throw_exception(zend_ce_exception, "File not found in source tar archive", 0);
        return;
    }

    // Save current position in target tar
    pos = ftell(tar->fp);

    // Write header to target tar
    if (write_tar_header(tar->fp, &header) != 0) {
        fseek(tar->fp, pos, SEEK_SET);
        zend_throw_exception(zend_ce_exception, "Failed to write tar header", 0);
        return;
    }

    // Copy file data
    size_t size = octal_to_int(header.size, TAR_SIZE_SIZE);
    if (copy_file_data(source_tar->fp, tar->fp, size) != 0) {
        fseek(tar->fp, pos, SEEK_SET);
        zend_throw_exception(zend_ce_exception, "Failed to copy file data", 0);
        return;
    }

    // Update tar size
    tar->size = ftell(tar->fp);
    tar->origin_size += size;
}

ZEND_BEGIN_ARG_INFO(arginfo_neptune_tar_addFrom, 0)
    ZEND_ARG_INFO(0, source_tar)
    ZEND_ARG_INFO(0, filename)
ZEND_END_ARG_INFO()


static zend_function_entry neptune_tar_methods[] = {
    PHP_ME(Neptune_Archive_Tar, __construct, arginfo_neptune_tar_construct, ZEND_ACC_PUBLIC)
    PHP_ME(Neptune_Archive_Tar, __destruct, arginfo_neptune_tar_destruct, ZEND_ACC_PUBLIC)
    PHP_ME(Neptune_Archive_Tar, readFile, arginfo_neptune_tar_readFile, ZEND_ACC_PUBLIC)
    PHP_ME(Neptune_Archive_Tar, extractFile, arginfo_neptune_tar_extractFile, ZEND_ACC_PUBLIC)
    PHP_ME(Neptune_Archive_Tar, addFrom, arginfo_neptune_tar_addFrom, ZEND_ACC_PUBLIC)
    PHP_FE_END
};


// class registration
void neptune_archive_init(void)
{
    zend_class_entry ce;

    // Register TarFileNotFoundException
    INIT_CLASS_ENTRY(ce, "Neptune\\Archive\\TarFileNotFoundException", NULL);
    neptune_tar_file_not_found_exception_ce = zend_register_internal_class_ex(&ce, zend_ce_exception);

    // Register IllegalTarFormatException
    INIT_CLASS_ENTRY(ce, "Neptune\\Archive\\IllegalTarFormatException", NULL);
    neptune_illegal_tar_format_exception_ce = zend_register_internal_class_ex(&ce, zend_ce_exception);

    // Register Tar class
    INIT_CLASS_ENTRY(ce, "Neptune\\Archive\\Tar", neptune_tar_methods);
    neptune_tar_ce = zend_register_internal_class(&ce);
    neptune_tar_ce->create_object = neptune_tar_create_object;
    memcpy(&neptune_tar_handlers, &std_object_handlers,
        sizeof(zend_object_handlers));
    neptune_tar_handlers.offset = XtOffsetOf(neptune_tar_t, std);

    // Register Tar class properties
    zend_declare_property_string(neptune_tar_ce, "tarFile", sizeof("tarFile")-1, "", ZEND_ACC_PRIVATE);
    zend_declare_property_string(neptune_tar_ce, "mode", sizeof("mode")-1, "", ZEND_ACC_PRIVATE);
    zend_declare_property_null(neptune_tar_ce, "fp", sizeof("fp")-1, ZEND_ACC_PRIVATE);
    zend_declare_property_long(neptune_tar_ce, "size", sizeof("size")-1, 0, ZEND_ACC_PRIVATE);
    zend_declare_property_long(neptune_tar_ce, "origin_size", sizeof("origin_size")-1, 0, ZEND_ACC_PRIVATE);

}
