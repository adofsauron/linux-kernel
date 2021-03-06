#ifndef __COW_SYS_H__
#define __COW_SYS_H__

#include "kern_util.h"
#include "user_util.h"
#include "os.h"
#include "user.h"

static inline void *cow_malloc(int size)
{
	return(um_kmalloc(size));
}

static inline void cow_free(void *ptr)
{
	kfree(ptr);
}

#define cow_printf printk

static inline char *cow_strdup(char *str)
{
	return(uml_strdup(str));
}

static inline int cow_seek_file(int fd, unsigned long long offset)
{
	return(os_seek_file(fd, offset));
}

static inline int cow_file_size(char *file, unsigned long long *size_out)
{
	return(os_file_size(file, size_out));
}

static inline int cow_write_file(int fd, char *buf, int size)
{
	return(os_write_file(fd, buf, size));
}

#endif

/*
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-file-style: "linux"
 * End:
 */
