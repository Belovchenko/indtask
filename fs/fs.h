#include <inc/fs.h>
#include <inc/lib.h>

#define SECTSIZE 512                  // bytes per disk sector
#define BLKSECTS (BLKSIZE / SECTSIZE) // sectors per block

/* Disk block n, when in memory, is mapped into the file system
 * server's address space at DISKMAP + (n*BLKSIZE). */
#define DISKMAP 0x10000000

/* Maximum disk size we can handle (3GB) */
#define DISKSIZE 0xC0000000

extern struct Super *super; // superblock
extern uint32_t *bitmap;    // bitmap blocks mapped in memory

/* ide.c */
bool ide_probe_disk1(void);
void ide_set_disk(int diskno);
void ide_set_partition(uint32_t first_sect, uint32_t nsect);
int ide_read(uint32_t secno, void *dst, size_t nsecs);
int ide_write(uint32_t secno, const void *src, size_t nsecs);

/* bc.c */
void *diskaddr(uint32_t blockno);
bool va_is_mapped(void *va);
bool va_is_dirty(void *va);
void flush_block(void *addr);
void bc_init(void);

/* fs.c */
void fs_init(void);
int file_get_block(struct File *f, uint32_t file_blockno, char **pblk);
int file_create(const char *path, struct File **f);
int file_block_walk(struct File *f, uint32_t filebno, uint32_t **ppdiskbno, bool alloc);
int file_open(const char *path, struct File **f);
ssize_t file_read(struct File *f, void *buf, size_t count, off_t offset);
int file_write(struct File *f, const void *buf, size_t count, off_t offset);
int file_set_size(struct File *f, off_t newsize);
void file_flush(struct File *f);
int file_remove(const char *path);
void fs_sync(void);

//IZ1 incremental_snapshot
int find_in_snapshot_list(struct File * f);
int snapshot_file_read(struct File *f, void *buf, size_t count, off_t offset);
int snapshot_file_write(struct File *f, const void *buf, size_t count, off_t offset);
int find_in_snapshot(struct File * snapshot,uint64_t my_addr, off_t * offset);
int create_snapshot(const char * comment, const char * name);
int snapshot_find_size(struct File * f);
int delete_snapshot(const char *name);
int accept_snapshot(const char *name);
int merge_snapshot(struct File *snap);
int print_snapshot_list();
int rec_print_snapshot_list(struct File *snap, struct Snapshot_header header);
int enable_snapshot();

//IZ1 Full_snapshot
int check_block_dir(struct File * dir, uint32_t block_num);
struct File * find_dir(struct File * dir, uint32_t block_num);
uint32_t * tracked_block(uint32_t block_num, struct File * snapshot, int flag);
struct File * cow_block(struct File * addr);
uint32_t * find_file(struct File * root, uint32_t block_num);
int snapshot_full_file_write(struct File * f, const void * buf, size_t count, off_t offset);
int add_file_blocks(struct File * snapshot, struct File * f, off_t offset);
int add_dir_blocks(struct File * snapshot, struct File * dir, off_t offset);
int create_table_blocks(struct File * snapshot, struct File * root);
int create_snapshot_full(const char * comment, const char * name);
int accept_full_snapshot(const char * name);
int copy_dir(struct File * new,struct File old);
void clean_directory(struct File * dir);
void free_direct(struct File * dir);
int delete_full_snapshot(const char * name);
int print_bitmap();
int print_full_help();


//IZ1 Defrag
int de_frag();
int test_de_frag(int k);

/* int	map_block(uint32_t); */
bool block_is_free(uint32_t blockno);
int alloc_block(void);

/* test.c */
void fs_test(void);
void snapshots_test(void);