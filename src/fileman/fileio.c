#include "fileio.h"
#include "core/core.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define MAX_FILE_SIZE (1ull << 25) // This is 32MiB, temporary

bool gem_read_entire_file(const char* path, char** src, size_t* size)
{
    GEM_ASSERT(path != NULL);
    GEM_ASSERT(src != NULL);
    GEM_ASSERT(size != NULL);

    bool success = false;
    char* buf = NULL;
    size_t total_read = 0;
    int fd;

    *src = NULL;
    fd = open(path, O_RDONLY);
    if(fd == -1)
        return false;

    off_t file_size = lseek(fd, 0, SEEK_END);
    if(file_size <= 0 || (size_t)file_size > MAX_FILE_SIZE)
        goto end;

    *size = (size_t)file_size;

    if(lseek(fd, 0, SEEK_SET) == -1)
        goto end;

    buf = malloc(*size + 1);
    GEM_ENSURE(buf != NULL);

    do
    {
        ssize_t temp = read(fd, buf + total_read, *size - total_read);
        if(temp <= 0)
            break;
        total_read += temp;
    } while(*size > total_read);
        

    if(total_read != *size)
    {
        free(buf);
        goto end;
    }

    buf[*size] = '\0';
    *src = buf;
    success = true;

end:
    close(fd);
    return success;
}

bool gem_write_text_buffer(const TextBuffer* buf, const char* path)
{
    GEM_ASSERT(buf != NULL);
    GEM_ASSERT(path != NULL);

    bool success = false;
    int fd;

    fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
    if(fd == -1)
        return false;

    const PieceTree* pt = &buf->contents;
    const PTNode* node = piece_tree_next_inorder(pt, NULL);
    size_t total_written = 0;
    while(node != NULL)
    {
        const char* buf = piece_tree_get_node_start(pt, node);
        size_t written = 0;
        do
        {
            ssize_t temp = write(fd, buf + written, node->length - written);
            if(temp <= 0)
                break;
            written += temp;
        } while(node->length > written);
        if(node->length != written)
            break;
        total_written += written;
        node = piece_tree_next_inorder(pt, node);
    }

    if(total_written == pt->size)
    {
        char c = '\n';
        write(fd, &c, 1);
        fsync(fd);
        success = true;
    }

    close(fd);
    return success;

}
