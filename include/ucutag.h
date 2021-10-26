#ifndef UCUTAG_UCUTAG_H
#define UCUTAG_UCUTAG_H

static int do_read( const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi );
static int do_readdir( const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi );
static int do_getattr( const char *path, struct stat *st );


#endif //UCUTAG_UCUTAG_H
