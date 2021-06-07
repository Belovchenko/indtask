#include <inc/string.h>
#include <inc/partition.h>
#include <inc/time.h>

#include "fs.h"

struct Super *super; // superblock
uint32_t *bitmap;    // bitmap blocks mapped in memory

uint64_t * curr_snap = 0;
uint32_t type = 0;
uint64_t * help_curr_snap = 0;

// --------------------------------------------------------------
// Super block
// --------------------------------------------------------------

// Validate the file system super-block.
void
check_super(void) {
  if (super->s_magic != FS_MAGIC)
    panic("bad file system magic number");

  if (super->s_nblocks > DISKSIZE / BLKSIZE)
    panic("file system is too large");

  cprintf("superblock is good\n");
}

// --------------------------------------------------------------
// Free block bitmap
// --------------------------------------------------------------

// Check to see if the block bitmap indicates that block 'blockno' is free.
// Return 1 if the block is free, 0 if not.
bool
block_is_free(uint32_t blockno) {
  if (super == 0 || blockno >= super->s_nblocks)
    return 0;
  if (bitmap[blockno / 32] & (1U << (blockno % 32))) // в одной странице 32 блока
    return 1;
  return 0;
}

void print_block(uint32_t blockno)
{
  uint32_t * addr = diskaddr(blockno);
  for (int i=0;i<BLKSIZE/4;i++)
  {
    if (i%10 == 0)
      cprintf("\n");
    cprintf("%d ",addr[i]);
  }
  cprintf("\n");

}

// Mark a block free in the bitmap
void
free_block(uint32_t blockno) {
  // Blockno zero is the null pointer of block numbers.
  //cprintf("Welcome to free_block\n");
  if (blockno == 0)
    panic("attempt to free zero block");
  if (curr_snap!=NULL && *curr_snap!=0 && type == 'f')
  { 
    struct File * next_snap = (struct File *)*curr_snap;
    while(next_snap!=NULL)
    {
      if (tracked_block(blockno, next_snap, 0)!=NULL)
      {
        return;
      }
      file_read(next_snap,&next_snap,sizeof(struct File *),sizeof(struct Snapshot_header)-sizeof(struct File *));
    }
    
  }
  bitmap[blockno / 32] |= 1U << (blockno % 32);
}

// Search the bitmap for a free block and allocate it.  When you
// allocate a block, immediately flush the changed bitmap block
// to disk.
//
// Return block number allocated on success,
// -E_NO_DISK if we are out of blocks.
//
// Hint: use free_block as an example for manipulating the bitmap.
int
alloc_block(void) { // выделяем блок
  // The bitmap consists of one or more blocks.  A single bitmap block
  // contains the in-use bits for BLKBITSIZE blocks.  There are
  // super->s_nblocks blocks in the disk altogether.

  // LAB 10: Your code here.
  int i;
	for (i = 0; i < super->s_nblocks; ++i) {
    if (block_is_free(i)) {
      bitmap[i / 32] &= ~(1 << (i % 32));
      flush_block(&bitmap[i / 32]);
      return i;
    }
	}

  return -E_NO_DISK;
}

// Validate the file system bitmap.
//
// Check that all reserved blocks -- 0, 1, and the bitmap blocks themselves --
// are all marked as in-use.
void
check_bitmap(void) {
  uint32_t i;

  // Make sure all bitmap blocks are marked in-use
  for (i = 0; i * BLKBITSIZE < super->s_nblocks; i++)
    assert(!block_is_free(2 + i));

  // Make sure the reserved and root blocks are marked in-use.
  assert(!block_is_free(0));
  assert(!block_is_free(1));

  cprintf("bitmap is good\n");
}

// --------------------------------------------------------------
// File system structures
// --------------------------------------------------------------

// Initialize the file system
void
fs_init(void) {
  static_assert(sizeof(struct File) == 256, "Unsupported file size");

  // Find a JOS disk.  Use the second IDE disk (number 1) if availabl
  if (ide_probe_disk1())
    ide_set_disk(1);
  else
    ide_set_disk(0);
  bc_init();

  // Set "super" to point to the super block.

  //IZ1

  super = diskaddr(1);
  curr_snap = (uint64_t *) (super + 1);
  help_curr_snap = (uint64_t *) (curr_snap + 1);
  if (*curr_snap != 0)
  {
    struct Snapshot_header header;
    file_read((struct File *)*curr_snap, &header, sizeof(header),0);
    type = header.type;
  }

  //*curr_snap = 0;
  //flush_block(super);

  check_super();

  // Set "bitmap" to the beginning of the first bitmap block.
  bitmap = diskaddr(2);
  check_bitmap();
}

// Find the disk block number slot for the 'filebno'th block in file 'f'.
// Set '*ppdiskbno' to point to that slot.
// The slot will be one of the f->f_direct[] entries,
// or an entry in the indirect block.
// When 'alloc' is set, this function will allocate an indirect block
// if necessary.
//
// Returns:
//	0 on success (but note that *ppdiskbno might equal 0).
//	-E_NOT_FOUND if the function needed to allocate an indirect block, but
//		alloc was 0.
//	-E_NO_DISK if there's no space on the disk for an indirect block.
//	-E_INVAL if filebno is out of range (it's >= NDIRECT + NINDIRECT).
//
// Analogy: This is like pgdir_walk for files.
// Hint: Don't forget to clear any block you allocate.
//возвращаем требуемый блок
int
file_block_walk(struct File *f, uint32_t filebno, uint32_t **ppdiskbno, bool alloc) {
  // LAB 10: Your code here.
  int newb;

  if (filebno >= NDIRECT + NINDIRECT) {
      return -E_INVAL;
  }

  if (filebno < NDIRECT) {
      uint32_t *bno;
      bno = f->f_direct + filebno;
      *ppdiskbno = bno;
  } 
  else {
    if (!f->f_indirect) {
      if (!alloc) {
        return -E_NOT_FOUND;
      }
      if ((newb = alloc_block()) < 0) {
        return -E_NO_DISK;
      } 
      else 
      {
        f->f_indirect = newb;
        memset(diskaddr(f->f_indirect), 0, BLKSIZE);
      }
    }
    *ppdiskbno = (uint32_t *) diskaddr(f->f_indirect) + (filebno - NDIRECT);//указывает на номер
  }
  return 0;
}


// Set *blk to the address in memory where the filebno'th
// block of file 'f' would be mapped.
//
// Returns 0 on success, < 0 on error.  Errors are:
//	-E_NO_DISK if a block needed to be allocated but the disk is full.
//	-E_INVAL if filebno is out of range.
//
// Hint: Use file_block_walk and alloc_block.
int
file_get_block(struct File *f, uint32_t filebno, char **blk) { // создает блок в случае необходимости
  // LAB 10: Your code here.
  int r, newb;
  uint32_t *pdiskbno;
  if ((r = file_block_walk(f, filebno, &pdiskbno, 1)) < 0) { // pdiskbno - указатель на указатель на блок
    return r;
  }

  if (!*pdiskbno) 
  {
    if ((newb = alloc_block()) < 0) {
      return -E_NO_DISK;
  }
    *pdiskbno = newb;
  }

  *blk = (char *) diskaddr(*pdiskbno);
  return 0;
}

// Try to find a file named "name" in dir.  If so, set *file to it.
//
// Returns 0 and sets *file on success, < 0 on error.  Errors are:
//	-E_NOT_FOUND if the file is not found
static int
dir_lookup(struct File *dir, const char *name, struct File **file) {
  int r;
  uint32_t i, j, nblock;
  char *blk;
  struct File *f;

  // Search dir for name.
  // We maintain the invariant that the size of a directory-file
  // is always a multiple of the file system's block size.
  assert((dir->f_size % BLKSIZE) == 0);
  nblock = dir->f_size / BLKSIZE;
  for (i = 0; i < nblock; i++) {
    if ((r = file_get_block(dir, i, &blk)) < 0) // blk - подходящий адрес
      return r;
    f = (struct File *)blk;
    for (j = 0; j < BLKFILES; j++)
      if (strcmp(f[j].f_name, name) == 0) {
        *file = &f[j];
        return 0;
      }
  }
  return -E_NOT_FOUND;
}

// Set *file to point at a free File structure in dir.  The caller is
// responsible for filling in the File fields.
static int
dir_alloc_file(struct File *dir, struct File **file) {
  int r;
  uint32_t nblock, i, j;
  char *blk;
  struct File *f;

  assert((dir->f_size % BLKSIZE) == 0);
  nblock = dir->f_size / BLKSIZE;
  for (i = 0; i < nblock; i++) {
    if ((r = file_get_block(dir, i, &blk)) < 0)
      return r;
    f = (struct File *)blk;
    for (j = 0; j < BLKFILES; j++)
      if (f[j].f_name[0] == '\0') {
        *file = &f[j];
        return 0;
      }
  }
  dir->f_size += BLKSIZE;
  if ((r = file_get_block(dir, i, &blk)) < 0)
    return r;
  f     = (struct File *)blk;
  *file = &f[0];
  return 0;
}

// Skip over slashes.
static const char *
skip_slash(const char *p) {
  while (*p == '/')
    p++;
  return p;
}

// Evaluate a path name, starting at the root.
// On success, set *pf to the file we found
// and set *pdir to the directory the file is in.
// If we cannot find the file but find the directory
// it should be in, set *pdir and copy the final path
// element into lastelem.
static int
walk_path(const char *path, struct File **pdir, struct File **pf, char *lastelem) {
  const char *p;
  char name[MAXNAMELEN];
  struct File *dir, *f;
  int r;

  // if (*path != '/')
  //	return -E_BAD_PATH;
  path    = skip_slash(path);
  f       = &super->s_root;
  dir     = 0;
  name[0] = 0;

  if (pdir)
    *pdir = 0;
  *pf = 0;
  while (*path != '\0') {
    dir = f;
    p   = path;
    while (*path != '/' && *path != '\0')
      path++;
    if (path - p >= MAXNAMELEN)
      return -E_BAD_PATH;
    memmove(name, p, path - p);
    name[path - p] = '\0';
    path           = skip_slash(path);

    if (dir->f_type != FTYPE_DIR)
      return -E_NOT_FOUND;

    if ((r = dir_lookup(dir, name, &f)) < 0) {
      if (r == -E_NOT_FOUND && *path == '\0') {
        if (pdir)
          *pdir = dir;
        if (lastelem)
          strcpy(lastelem, name);
        *pf = 0;
      }
      return r;
    }
  }

  if (pdir)
    *pdir = dir;
  *pf = f;
  return 0;
}

// --------------------------------------------------------------
// File operations
// --------------------------------------------------------------

// Create "path".  On success set *pf to point at the file and return 0.
// On error return < 0.
int
file_create(const char *path, struct File **pf) {
  char name[MAXNAMELEN];
  int r;
  struct File *dir, *f;

  if ((r = walk_path(path, &dir, &f, name)) == 0)
    return -E_FILE_EXISTS;
  if (r != -E_NOT_FOUND || dir == 0)
    return r;
  if ((r = dir_alloc_file(dir, &f)) < 0)
    return r;

  strcpy(f->f_name, name);
  *pf = f;
  file_flush(dir);
  return 0;
}

// Open "path".  On success set *pf to point at the file and return 0.
// On error return < 0.
int
file_open(const char *path, struct File **pf) {
  return walk_path(path, 0, pf, 0);
}

// Read count bytes from f into buf, starting from seek position
// offset.  This meant to mimic the standard pread function.
// Returns the number of bytes read, < 0 on error.
ssize_t
file_read(struct File *f, void *buf, size_t count, off_t offset) {
  int r, bn;
  off_t pos;
  char *blk;

  if (*curr_snap != 0 && type == 'i' && find_in_snapshot_list(f)==0 && f->f_type != FTYPE_DIR)
  {
    return snapshot_file_read(f,buf,count,offset);
  }

  if (offset >= f->f_size)
    return 0;

  count = MIN(count, f->f_size - offset);

  for (pos = offset; pos < offset + count;) {
    if ((r = file_get_block(f, pos / BLKSIZE, &blk)) < 0)
      return r;
    bn = MIN(BLKSIZE - pos % BLKSIZE, offset + count - pos);
    memmove(buf, blk + pos % BLKSIZE, bn);
    pos += bn;
    buf += bn;
  }

  return count;
}

// Write count bytes from buf into f, starting at seek position
// offset.  This is meant to mimic the standard pwrite function.
// Extends the file if necessary.
// Returns the number of bytes written, < 0 on error.
int
file_write(struct File *f, const void *buf, size_t count, off_t offset) 
{

  //cprintf("writing in file %s...\n",f->f_name);
  if (*curr_snap != 0 && find_in_snapshot_list(f)==0)
  { 
    if (find_in_snapshot_list(f)==0)
    {
      if (type == 'i')
        return snapshot_file_write(f,buf,count,offset);
      else if (type == 'f')
        snapshot_full_file_write(f,buf,count,offset);
      else 
      {
        cprintf("Wrong snapshot type %d\n",type);
        return 0;
      }
    }
  }

  int r, bn;
  off_t pos;
  char *blk;

  // Extend file if necessary
  if (offset + count > f->f_size)
    if ((r = file_set_size(f, offset + count)) < 0)
      return r;

  for (pos = offset; pos < offset + count;) {
    if ((r = file_get_block(f, pos / BLKSIZE, &blk)) < 0)
      return r;
    bn = MIN(BLKSIZE - pos % BLKSIZE, offset + count - pos);
    memmove(blk + pos % BLKSIZE, buf, bn);
    pos += bn;
    buf += bn;
  }

  return count;
}

int check_block_dir(struct File * dir, uint32_t block_num)
{
  uint32_t * addr = (uint32_t *)((uint64_t)dir->f_indirect * BLKSIZE + DISKMAP);
  for (int i=0;i<dir->f_size/BLKSIZE;i++)
  {
    if (i<NDIRECT)
    {
      if (dir->f_direct[i]==block_num)
        return 1;
    }
    else 
    {
      if (addr[i-NDIRECT]==block_num)
        return 1;
    }
  }
  return 0;
}

struct File * find_dir(struct File * dir, uint32_t block_num)
{
  struct File * new_dir;
  char * blk;
  if (check_block_dir(dir, block_num))
    return dir;
  for (int i=0;i<dir->f_size/BLKSIZE;i++)
  {
    file_get_block(dir,i,&blk);
    new_dir = (struct File *) blk;
    for (int j=0;j<BLKFILES;j++)
    {
      if(new_dir[j].f_type==FTYPE_DIR)
      {
        if (check_block_dir(new_dir + j, block_num))
          return new_dir + j;
      }
    }
  }
  return NULL;
}

//Проверяет, принадлежит ли файлу данный блок. 
//Возвращает указатель на соответствующую запись, если блок найден, и NULL иначе.
uint32_t * check_blocks(struct File *f, uint32_t block_num)
{
  int number;
  uint32_t * addr;
  for (int pos=0;pos<f->f_size;pos+=BLKSIZE)
  {
    number = pos/BLKSIZE;
    if (number<NDIRECT)
    {
      if (f->f_direct[number]==block_num)
        return f->f_direct+number;
    }
    else 
    {
      if (number == NDIRECT)
      {
        if (f->f_indirect == 0)
          return NULL;
        addr = (uint32_t *)((uint64_t)f->f_indirect*BLKSIZE+DISKMAP);
      }
      if (addr[number-NDIRECT]==block_num)
        return addr+(number-NDIRECT);
    }
  }
  return NULL;
}

//Print man for full snapshots
int print_full_help()
{
  cprintf("Manual for full snapshots:\n");
  cprintf("-f\t\t\tfull snapshot\n");
  cprintf("-a\t\t\taccept snapshot\n");
  cprintf("-c\t\t\tcreate snapshot\n");
  cprintf("\n");
  cprintf("Examples:\n");
  cprintf("snapshot -f -c snap my comment\tcreate full snapshot\
  with name \"snap\" and comment \"my comment\"\n");
  return 0;
}


//Находит файл в данной директории, которому принадлежит данный блок.
//возвращает указатель на номер блока, если найден, и NULL иначе
uint32_t * find_file(struct File * dir, uint32_t block_num)
{
  int r;
  uint32_t i, j, nblock;
  char *blk;
  struct File *f;
  uint32_t * res;

  nblock = dir->f_size / BLKSIZE;
  for (i = 0; i < nblock; i++) {
    if ((r = file_get_block(dir, i, &blk)) < 0) // blk - подходящий адрес
    {
      cprintf("Something wrong with file_get_block in find_file\n");
      return NULL;
    }
    f = (struct File *)blk;
    for (j = 0; j < BLKFILES; j++)
    {
      if (f[j].f_type==FTYPE_DIR && f[j].f_name[0]!='.')
      {
        res = find_file(f+j,block_num);
      }
      else if (f[j].f_type==FTYPE_REG)
      {
        res = check_blocks(f+j,block_num);
      }

      if (res)
        return res;
    }
  }

  return NULL;

}

//Функция для полных снимков.
//Проверяем, является ли данный блок отслеживаемым.
//Если флаг выставлен, то удаляет его из списка отслеживаемых блоков.
uint32_t * tracked_block(uint32_t block_num, struct File * snapshot, int flag)
{
  if (!block_num)
    return NULL;
  uint32_t addr[100];
  uint32_t buf;
  struct File copied_root; 
  int r = 0, res = 0;
  uint32_t * final_res;

  for (int pos = sizeof(struct Snapshot_header)+sizeof(struct File);pos<snapshot->f_size;pos+=400)
  {
    if ((r=file_read(snapshot, addr, 400, pos))<0)
      return NULL;
    for (int j=0;j<r/4;j++)
      if(addr[j]==block_num)
      {
        if (flag)
        {
          if (snapshot->f_size!=sizeof(struct Snapshot_header)+sizeof(struct File)+sizeof(uint32_t))
          {
            file_read (snapshot, &buf, sizeof(uint32_t), snapshot->f_size - sizeof(uint32_t));
            file_write(snapshot, &buf, sizeof(uint32_t),pos+j*4);
          }
          snapshot->f_size-=4;
        }
        res = 1;
        break;
      }
    if (res==1)
      break;
  }

  if (!res)
    return NULL;

  if (!flag)
    return (uint32_t *)&curr_snap; // любой указатель
  
  file_read(snapshot,&copied_root,sizeof(struct File),sizeof(struct Snapshot_header));
  if ((final_res=find_file(&copied_root, block_num))==NULL)
  {
    cprintf("something wrong in tracked_block\n");
  }
  return final_res;
}

//Функция для полных снимков.
//Создает копию тех блоков, которые оригинальный file_write собирается менять.
int snapshot_full_file_write(struct File * f, const void * buf, size_t count, off_t offset)
{
  int r, bn;
  uint32_t *addr = NULL;
  uint32_t block_num;
  uint32_t * cow_block = NULL;
  struct File * snapshot, * next_snapshot;

  size_t size = MIN(offset+count,f->f_size);
  offset = MIN(offset, f->f_size);

  if (f->f_indirect)
    addr = (uint32_t *)((uint64_t)(f->f_indirect)*BLKSIZE+DISKMAP);

  int i;
  for (int pos=offset;pos<size;)
  {
    i = pos/BLKSIZE;
    //cprintf("block num = %d\n",i);
    bn = MIN(BLKSIZE - pos%BLKSIZE,size-pos);
    if (i<NDIRECT)
      block_num = f->f_direct[i];
    else if (addr!=NULL)
      block_num = addr[i-NDIRECT];
    else
      break;

    snapshot = (struct File *)*(curr_snap);
    r=0;
    while(snapshot!=NULL)
    {
      if ((cow_block = tracked_block(block_num, snapshot, 1))!=NULL)
      {
        if (!r)
        { 
          if ((r = alloc_block())<0)
            return r;
          memmove((void *)((uint64_t)r*BLKSIZE+DISKMAP), (void *)((uint64_t)block_num*BLKSIZE+DISKMAP), BLKSIZE);
          flush_block((void *)((uint64_t)r*BLKSIZE+DISKMAP));
        }
        *cow_block = r;
        flush_block(cow_block);
        file_write(snapshot,&r,sizeof(uint32_t),snapshot->f_size);
      }
      file_read(snapshot,&next_snapshot,sizeof(struct File *),sizeof(struct Snapshot_header)-sizeof(uint64_t));
      snapshot = next_snapshot;
    }

    pos+=bn;
  }

  return 0;
}

//Ищет данный файл в списке снимков. 
//Возвращает 1 в случае успеха.
int find_in_snapshot_list(struct File * f)
{
  int r;
  struct File * next_snapshot = (struct File *)*curr_snap;

  while (next_snapshot!=NULL)
  {
    if (f==next_snapshot)
      return 1;
    else
    {
      if ((r=file_read((struct File *)next_snapshot,&next_snapshot,sizeof(struct File *),sizeof(struct Snapshot_header)-sizeof(uint64_t)))<0)
        return r;
    }
  }

  return 0;
}

//Функция для инкрементальных снимков.
//Ищет данный адрес по всем снимкам, начиная с самого последнего.
int find_in_snapshot(struct File * snapshot,uint64_t my_addr, off_t * offset)
{
  int r;
  uint32_t disk_addr = (uint32_t)(my_addr - DISKMAP);

  char buf[SNAP_BUF_SIZE];
  char * buffer;
  
  for (int pos=sizeof(struct Snapshot_header);pos<snapshot->f_size;pos+=SNAP_BUF_SIZE)
  {

    if ((r=file_read(snapshot,buf,SNAP_BUF_SIZE,pos))<0)
      return r;

    buffer = buf;

    for (int i=0;i<r/5;i++)
      if (*(uint32_t *)buffer == disk_addr)
      {
        *offset = pos + i*5;
        return 1;
      }
      else
      {
        buffer+=5;
      }
      
  }

  return 0;
}

//Функция для инкрементальных снимков.
//Ищет реальный размер файла по снэпшотам.
off_t snapshot_find_size(struct File * f) 
{
  char * addr = (char *)&f->f_size;
  struct File * snapshot = (struct File *)(*curr_snap);
  struct File * buf_snapshot;
  int r;
  int my_number;
  char * number = (char *)&my_number;
  off_t buf_offset;

  for (int i=0;i<sizeof(uint32_t);i++)
  {
    buf_snapshot = snapshot;

    while(buf_snapshot!=NULL)
    {
      if ( (r = find_in_snapshot(buf_snapshot, (uint64_t)addr, &buf_offset)) == 1)
      {
        file_read(buf_snapshot, number+i, 1, buf_offset + sizeof(uint32_t));
        break;  
      }
      else if (!r)
      {
        if ((r=file_read(buf_snapshot,&buf_snapshot,sizeof(struct File *),sizeof(struct Snapshot_header)-sizeof(uint64_t)))<0)
          return r;
      }
      else 
        return r;
    }

    if (buf_snapshot==NULL)
      number[i] = *addr;
    
    addr++;
  }

  return my_number;
}

/*
void snapshot_output(struct File * snapshot)
{
  char * addr = NULL;
  file_get_block(snapshot,0,&addr);
  struct Snapshot_header header;
  file_read(snapshot,&header,sizeof(struct Snapshot_header),0);

  struct Snapshot_table elem;
  cprintf("date:%d comment:%s type:%d name:%s\n",header.date, header.comment, header.type, snapshot->f_name);
  for (int i=sizeof(struct Snapshot_header);i<snapshot->f_size;i+=5)
  {
    file_read(snapshot, &elem, 5, i);
    cprintf("offset: %d %x %x\n",i,(int)elem.disk_addr, elem.value);
  }
}*/


//Функция для инкрементальных снэпшотов
//Вызывается вместо file_write и записывает все изменения в последний снимок.
int snapshot_file_write(struct File *f, const void *buf, size_t count, off_t offset)
{
  int r, bn;
  off_t pos;
  char * addr;
  off_t new_size;

  uint32_t disk_addr;

  struct File * snapshot = (struct File *) *curr_snap; 
  int snap_offset = snapshot->f_size;

  off_t buf_offset;

  if (offset + count > snapshot_find_size(f)) //исправить
  {
    addr = (char *)&f->f_size;
    new_size = offset + count;

    for (int i=0; i<sizeof(new_size);i++)
    {
      addr += i;

      if ( (r = find_in_snapshot(snapshot, (uint64_t)addr, &buf_offset)) == 1) // ищем адрес
      {
        file_write(snapshot, (char *)&new_size + i, 1, buf_offset + sizeof(uint32_t)); 
      } 
      else
      {
        disk_addr = (uint32_t)((uint64_t)addr - DISKMAP);

        file_write(snapshot, &disk_addr, sizeof(uint32_t), snap_offset);
        snap_offset+=sizeof(uint32_t);
        
        file_write(snapshot, (char *)&new_size + i, 1, snap_offset);
        snap_offset++;
      }
    }
  }


  for (pos = offset; pos < offset + count;) 
  {

    if ((r = file_get_block(f, pos / BLKSIZE, &addr)) < 0)
      return r;

    addr = addr + pos % BLKSIZE; 
    bn = MIN(BLKSIZE - pos % BLKSIZE, offset + count - pos);

    for (int i=0;i<bn;i++)
    {

      if ( (r = find_in_snapshot(snapshot,(uint64_t)addr, &buf_offset)) == 1)
      {
        file_write(snapshot, buf, 1, buf_offset + sizeof(uint32_t));
      }
      else if (!r)
      {
        disk_addr = (uint32_t)((uint64_t)addr - DISKMAP);

        file_write(snapshot, &disk_addr, sizeof(uint32_t), snap_offset); 
        snap_offset+=sizeof(uint32_t);

        file_write(snapshot, buf, 1, snap_offset); 
        snap_offset ++;
      }
      else 
        return r;

      addr++;
      buf++;
    }
    pos += bn;
  }

  return count;
}

//Функция для инкрементальныъ снэпшотов.
//Читает файл сначала по всем снимкам, далее, если нужный адрес не найден, читает непосредственно
//из файловой системы. И так с каждым адресом.
int snapshot_file_read(struct File *f, void *buf, size_t count, off_t offset)
{
  int r, bn;
  off_t pos;

  char * addr;
  off_t buf_offset;

  if (offset >= snapshot_find_size(f))
  {
    return 0;
  }

  count = MIN(count, snapshot_find_size(f) - offset);

  struct File * snapshot = (struct File *) *curr_snap; 
  struct File * buf_snapshot;
  for (pos = offset; pos < offset + count;) 
  {

    if ((r = file_get_block(f, pos / BLKSIZE, &addr)) < 0)
      return r;

    addr = addr + pos % BLKSIZE; 
    bn = MIN(BLKSIZE - pos % BLKSIZE, offset + count - pos);

    for (int i=0;i<bn;i++)
    {
      buf_snapshot = snapshot;

      while(buf_snapshot!=0)
      {
        if ( (r = find_in_snapshot(buf_snapshot, (uint64_t)addr, &buf_offset)) == 1)
        {
          file_read(buf_snapshot, buf, 1, buf_offset + sizeof(uint32_t));
          break;  
        }
        else if (!r)
        {
          if ((r=file_read(buf_snapshot,&buf_snapshot,sizeof(struct File *),sizeof(struct Snapshot_header)-sizeof(uint64_t)))<0)
            return r;
        }
        else 
          return r;
      }

      if (!buf_snapshot)
      {
        *(char *)buf = *addr;
      }

      addr++;
      buf++;
    }

    pos+=bn;
  }
  return count;
}


// Remove a block from file f.  If it's not there, just silently succeed.
// Returns 0 on success, < 0 on error.
static int
file_free_block(struct File *f, uint32_t filebno) { // тут надо подумать
  int r;
  uint32_t *ptr;
  //cprintf("file_free_block %s %d\n",f->f_name,filebno);
  if ((r = file_block_walk(f, filebno, &ptr, 0)) < 0)
    return r;
  if (*ptr) {
    free_block(*ptr);
    uint32_t blockno = ((uint64_t)(ptr)-DISKMAP)/BLKSIZE;
    if (curr_snap!=NULL && *curr_snap!=0 && type == 'f')
    { 
      struct File * next_snap = (struct File *)*curr_snap;
      while(next_snap!=NULL)
      {
        if (tracked_block(blockno, next_snap, 0)!=NULL)
        {
          return 0;
        }
      file_read(next_snap,&next_snap,sizeof(struct File *),sizeof(struct Snapshot_header)-sizeof(struct File *));
      }
    }
    *ptr = 0;
  }
  return 0;
}

// Remove any blocks currently used by file 'f',
// but not necessary for a file of size 'newsize'.
// For both the old and new sizes, figure out the number of blocks required,
// and then clear the blocks from new_nblocks to old_nblocks.
// If the new_nblocks is no more than NDIRECT, and the indirect block has
// been allocated (f->f_indirect != 0), then free the indirect block too.
// (Remember to clear the f->f_indirect pointer so you'll know
// whether it's valid!)
// Do not change f->f_size.
static void
file_truncate_blocks(struct File *f, off_t newsize) {
  int r;
  uint32_t bno, old_nblocks, new_nblocks;

  old_nblocks = (f->f_size + BLKSIZE - 1) / BLKSIZE;
  new_nblocks = (newsize + BLKSIZE - 1) / BLKSIZE;
  for (bno = new_nblocks; bno < old_nblocks; bno++)
    if ((r = file_free_block(f, bno)) < 0)
      cprintf("warning: file_free_block: %i", r);

  if (new_nblocks <= NDIRECT && f->f_indirect) {
    free_block(f->f_indirect);
    f->f_indirect = 0;
  }
}

// Set the size of file f, truncating or extending as necessary.
int
file_set_size(struct File *f, off_t newsize) 
{
  //cprintf("new size %d for file %s\n",(int)newsize,f->f_name);
  if ((curr_snap != NULL) && (*curr_snap != 0) && type=='i' && find_in_snapshot_list(f)==0)
  {
    char *addr;
    off_t buf_offset;
    uint32_t disk_addr;
    struct File *snap = (struct File *)(*curr_snap);
    addr = (char *)&f->f_size;
    int r, snap_offset = snap->f_size;

    for (int i=0; i<sizeof(newsize);i++)
    {
      addr += i;
      
      if ( (r = find_in_snapshot(snap, (uint64_t)addr, &buf_offset)) == 1) // ищем адрес
      {
        file_write(snap, (char *)&newsize + i, 1, buf_offset + sizeof(uint32_t)); 
      } 
      else
      {
        disk_addr = (uint32_t)((uint64_t)addr - DISKMAP);

        file_write(snap, &disk_addr, sizeof(uint32_t), snap_offset);
        snap_offset+=sizeof(uint32_t);
        
        file_write(snap, (char *)&newsize + i, 1, snap_offset);
        snap_offset++;
      }
    }
  }
  else
  {
    if (f->f_size > newsize)
      file_truncate_blocks(f, newsize);

    f->f_size = newsize;
    flush_block(f);
  }
  //cprintf("OUT FROM SET SIZE\n");
  return 0;
}

// Flush the contents and metadata of file f out to disk.
// Loop over all the blocks in file.
// Translate the file block number into a disk block number
// and then check whether that disk block is dirty.  If so, write it out.
void
file_flush(struct File *f) {
  int i;
  uint32_t *pdiskbno;

  if (*curr_snap != 0 && type == 'i')
  {  
    if (find_in_snapshot_list(f)==1)
    {
      for (i = 0; i < (f->f_size + BLKSIZE - 1) / BLKSIZE; i++) {
        if (file_block_walk(f, i, &pdiskbno, 0) < 0 ||
            pdiskbno == NULL || *pdiskbno == 0)
          continue;
        flush_block(diskaddr(*pdiskbno));
      }
      flush_block(f);
      if (f->f_indirect)
        flush_block(diskaddr(f->f_indirect));
    }
    else
      file_flush((struct File *)*curr_snap);
  }
  else
  {
    for (i = 0; i < (f->f_size + BLKSIZE - 1) / BLKSIZE; i++) {
        if (file_block_walk(f, i, &pdiskbno, 0) < 0 ||
            pdiskbno == NULL || *pdiskbno == 0)
          continue;
        flush_block(diskaddr(*pdiskbno));
      }
      flush_block(f);
      if (f->f_indirect)
      {
        flush_block(diskaddr(f->f_indirect));
      }
  }
}

// Sync the entire file system.  A big hammer.
void
fs_sync(void) {
  int i;
  for (i = 1; i < super->s_nblocks; i++)
    flush_block(diskaddr(i));
}


//Функция для полных снэпшотов.
//Добавляет в список отслеживаемых блоков все блоки данного файла.
int add_file_blocks(struct File * snapshot, struct File * f, off_t offset)
{
  char * addr;
  uint32_t table_elem;
  //cprintf("%s\n",f->f_name);
  for (int i=0;i<(f->f_size+BLKSIZE-1)/BLKSIZE;i++)
  {
    if (i==NDIRECT)
    {
      file_write(snapshot, &f->f_indirect, sizeof(uint32_t),offset);
      offset+=sizeof(uint32_t);
    }
    file_get_block(f, i, &addr); //file_free_block аналогия
    table_elem = (uint32_t)(((uint64_t)addr - DISKMAP)/BLKSIZE);
    //cprintf("table elem = %d\n",table_elem);
    file_write(snapshot, &table_elem, sizeof(uint32_t),offset);
    offset+=sizeof(uint32_t);
  }
  return offset;
}

//Функция для полных снэпшотов.
//Добавляет в список отслеживаемых блоков блоки всех файлов данной директории.
int add_dir_blocks(struct File * snapshot, struct File * dir, off_t offset)
{
  if (dir->f_type!=FTYPE_DIR)
    panic("add dir blocks file is not dir\n");

  int r;
  char * blk;
  struct File * f;
  uint32_t nblock = dir->f_size/ BLKSIZE;
  for (int i = 0; i < nblock; i++) 
  {
    if ((r = file_get_block(dir, i, &blk)) < 0)
      return r;
    f = (struct File *)blk;
    for (int j = 0; j < BLKFILES; j++)
    {
      if (f[j].f_type!=FTYPE_DIR)
      {
        offset = add_file_blocks(snapshot,f+j,offset);
      }
      else
      {
        offset = add_dir_blocks(snapshot,f+j,offset);
      }
    }
  }
  return offset;
}

//Функция для полных снэпшотов.
//Формирует в снимке список отслеживаемых блоков.
int create_table_blocks(struct File * snapshot, struct File * root)
{
  off_t offset = sizeof(struct Snapshot_header)+sizeof(struct File);

  offset = add_dir_blocks(snapshot, root, offset);
  return offset;
}

//Функция для полных снэпшотов.
//Удаляет снимок по имени.
int delete_full_snapshot(const char * name)
{
  struct File * snapshot = (struct File *)(*curr_snap);
  //Find snapshot with name
  while (snapshot!=NULL)
  {
    if (!strcmp(snapshot->f_name, name))
      break;
    file_read(snapshot,&snapshot,sizeof(struct File *),sizeof(struct Snapshot_header) - sizeof(struct File *));
  }
  if (snapshot==NULL)
  {
    cprintf("No snapshot with name %s\n",name);
    return -1;
  }

  struct File root;
  file_read(snapshot,&root,sizeof(struct File),sizeof(struct Snapshot_header));
  clean_directory(&root);
  free_direct(&root);

  struct File ** buf = (struct File **)(curr_snap);
  while((*buf)!=snapshot)
  {
    buf = (struct File **)((uint64_t)(*buf)->f_direct[0]*BLKSIZE + DISKMAP + 
                                  sizeof(struct Snapshot_header) - sizeof(uint64_t));
  }

  file_read(snapshot,buf,sizeof(struct File *),sizeof(struct Snapshot_header) - sizeof(uint64_t));
  if (buf==(struct File **)(curr_snap))
    flush_block(curr_snap);
  file_set_size(snapshot, 0);
  memset(snapshot, 0, sizeof(struct File));
  return 0;

}

//Функция для полных снэпшотов.
//Создает снимок.
int create_snapshot_full(const char * comment, const char * name)
{
  struct File * new_snap;
  int r;
  struct Snapshot_header new_header, buf_header;

  if ((r = file_create(name, &new_snap)) < 0)
  {
    cprintf("Error create_snapshot\n");
    return r;
  }

  if (*curr_snap!=0)
  {
    if ((r = file_read((struct File *)*curr_snap, &buf_header, sizeof(struct Snapshot_header), 0))<0)
    {
      cprintf("Error reading old snapshot\n");
      return r;
    }

    if ((char)buf_header.type == 'i')
    {
      cprintf("Old snapshot is incremental, but you want to create full\n");
      return -1;
    }

    file_flush((struct File *)*curr_snap);
  }
  else 
  {
    fs_sync();
  }

  new_header.type = 'f';
  type = 'f';
  new_header.date = sys_gettime();
  new_header.prev_snapshot = (uint64_t)*curr_snap;
  strcpy(new_header.comment, comment);

  *curr_snap = (uint64_t)new_snap;

  if ((r=file_write(new_snap, &new_header, sizeof(new_header),0))!=sizeof(new_header))
  {
    cprintf("Error with writing in snapshot\n");
    return r;
  }

  struct File new_super;
  *curr_snap = (uint64_t)new_snap;
  copy_dir(&new_super,super->s_root); // копирует директорию и все поддиректории рекурсивно

  struct File * buf_snapshot = (struct File *)(*curr_snap);
  struct File * delete_snap;
  
  while(buf_snapshot!=NULL)
  {
    dir_lookup(&new_super, buf_snapshot->f_name, &delete_snap);
    memset(delete_snap, 0, sizeof(struct File));
    file_read(buf_snapshot, &buf_snapshot, sizeof(struct File *), sizeof(struct Snapshot_header) - sizeof(struct File *));
  }

  file_write(new_snap,&new_super,sizeof(struct File),sizeof(struct Snapshot_header)); // записали модифицированную корневую директорию
  create_table_blocks(new_snap,&new_super);
  file_flush(new_snap);
  flush_block(curr_snap);
  return 0;
}

//Функция, выводящая актуальное состояние bitmap
int print_bitmap() // 0 если занят
{
  int i;
  char res;
	for (i = 0; i < super->s_nblocks; ++i) {
      if (i%32 == 0)
        cprintf("\n");
      res = (bitmap[i / 32] & (1 << (i % 32))) >> (i % 32);
      cprintf("%d",res);
  }
  cprintf("\n");
  return 0;
}


//Функция для полных снэпшотов.
//Создает копию переданной директории рекурсивно со всеми поддиректориями.
int copy_dir(struct File * new,struct File old)
{
  int r,i,j;
  uint32_t nblock = old.f_size / BLKSIZE;
  char * blk;
  uint32_t * addr = NULL;
  struct File * f;

  //Part1
  memmove(new,&old,sizeof(struct File));
  for (i=0;i<nblock;i++)
  {
    if (i<NDIRECT)
    {
      if ((r=alloc_block())<0)
      {
        cprintf("No free memory on disk\n");
        return r;
      }
      memmove((void *)((uint64_t)r*BLKSIZE + DISKMAP), (void *)((uint64_t)(new->f_direct[i])*BLKSIZE + DISKMAP), BLKSIZE);
      new->f_direct[i]=r;
    }
    else 
    {
      if (i==NDIRECT)
      {
        if ((r=alloc_block())<0)
        {
          cprintf("No free memory on disk\n");
          return r;
        }
        memmove((void *)((uint64_t)r*BLKSIZE + DISKMAP), (void *)((uint64_t)(new->f_indirect)*BLKSIZE + DISKMAP), BLKSIZE);
        new->f_indirect=r;
        addr = (uint32_t *)((uint64_t)r*BLKSIZE + DISKMAP);
      }
      if ((r=alloc_block())<0)
      {
        cprintf("No free memory on disk\n");
        return r;
      }
      memmove((void *)((uint64_t)r*BLKSIZE + DISKMAP), (void *)((uint64_t)(addr[i-NDIRECT])*BLKSIZE + DISKMAP), BLKSIZE);
      addr[i-NDIRECT]=r;
    }
  }

  struct File * buf;
  if ((r=dir_lookup(new,".",&buf))<0)
  {
    //cprintf("Hmm can`t find directory with name \".\"\n");
  }
  else
    memmove(buf,new,sizeof(struct File));

  //Part2
  nblock = new->f_size / BLKSIZE;
  for (int i = 0; i < nblock; i++) 
  {
    if ((r = file_get_block(new, i, &blk)) < 0) // blk - подходящий адрес
      return r;
    f = (struct File *)blk;
    for (j = 0; j < BLKFILES; j++)
      if (f[j].f_type == FTYPE_DIR && f[j].f_name[0]!='.') 
      {
        //cprintf("We have dir\n");
        copy_dir(f+j,f[j]);
        if ((r=dir_lookup(f+j,"..",&buf))<0)
        {
          cprintf("Hmm can`t find directory with name \"..\"");
        }
        else
          memmove(buf,new,sizeof(struct File));
      }
  } 

  //Part 3 file_flush(new)
  uint32_t *pdiskbno;

  for (i = 0; i < (new->f_size + BLKSIZE - 1) / BLKSIZE; i++) 
  {
    if (file_block_walk(new, i, &pdiskbno, 0) < 0 ||
        pdiskbno == NULL || *pdiskbno == 0)
      continue;
    flush_block(diskaddr(*pdiskbno));
  }
  if (new->f_indirect)
    flush_block(diskaddr(new->f_indirect));

  return 0;
}


//Функция для полных снэпшотов.
//Применяет снимок по его имени.
int accept_full_snapshot(const char * name)
{
  int r;
  struct File * snapshot = (struct File *)(*curr_snap);
  //Find snapshot with name
  while (snapshot!=NULL)
  {
    if (!strcmp(snapshot->f_name, name))
      break;
    file_read(snapshot,&snapshot,sizeof(struct File *),sizeof(struct Snapshot_header) - sizeof(struct File *));
  }
  if (snapshot==NULL)
  {
    cprintf("No snapshot with name %s\n",name);
    return -1;
  }
  //Part2
  struct File new_super;
  struct File ** buf;
  struct File * new_snap;
  file_read(snapshot,&new_super, sizeof(struct File), sizeof(struct Snapshot_header));
  clean_directory(&super->s_root);

  //снэпшоты есть в текущем состоянии, мы их не копировали в другие снэпшоты так как информация в них не актуальная
  buf = (struct File **)(curr_snap);
  //*curr_snap = 0;
  //еще не исключили
  //важно изменить прмиеняемый снэпшот 

  //отдельно меняем применяемый снэпшот
  struct File buf_super;

  copy_dir(&buf_super,new_super); //отслеживаемые блоки
  while((*buf)!=NULL)
  {
    dir_alloc_file(&new_super, &new_snap);
    memcpy(new_snap, *buf, sizeof(struct File));
    if (*buf == snapshot)
    {
      //скопировать все директории по-новой в new_snap
      memmove((void *)((uint64_t)(new_snap->f_direct[0])*BLKSIZE+DISKMAP+sizeof(struct Snapshot_header)),
                              &buf_super,sizeof(struct File));
    }
    memset(*buf,0,sizeof(struct File));//очистить старое
    *buf = new_snap; 
    r = new_snap->f_direct[0];
    buf = (struct File **)((uint64_t)r*BLKSIZE + DISKMAP + sizeof(struct Snapshot_header) - sizeof(uint64_t));
  }
  free_direct(&super->s_root);
  memcpy(&super->s_root, &new_super,sizeof(struct File));
  //flush_block(curr_snap);
  //file_flush(&super->s_root);
  fs_sync();
  return 0;
}

//Функция для полных снэпшотов
//Очищает директорию рекурсивно со всеми поддиректориями.
void free_direct(struct File * dir)
{
  int r;
  uint32_t i, j, nblock;
  char *blk;
  struct File *f;

  assert((dir->f_size % BLKSIZE) == 0);
  nblock = dir->f_size / BLKSIZE;
  for (i = 0; i < nblock; i++) {
    if ((r = file_get_block(dir, i, &blk)) < 0) // blk - подходящий адрес
      return;
    f = (struct File *)blk;
    for (j = 0; j < BLKFILES; j++)
      if (f[j].f_type==FTYPE_DIR) {
        free_direct(f+j);
      }
    
    file_free_block(dir,i);
  }
}

//Функция для полных снэпшотов.
//Очищает все файлы из дериктории рекурсивно со всеми поддиректориями.
void clean_directory(struct File * dir)
{
  int r;
  uint32_t i, j, nblock;
  char *blk;
  struct File *f;

  assert((dir->f_size % BLKSIZE) == 0);
  nblock = dir->f_size / BLKSIZE;
  for (i = 0; i < nblock; i++) {
    if ((r = file_get_block(dir, i, &blk)) < 0) // blk - подходящий адрес
      return;
    //cprintf("%i = d\n",i);
    f = (struct File *)blk;
    for (j = 0; j < BLKFILES; j++)
    {
      //cprintf("j=%d %s %d\n",j, f[j].f_name, f[j].f_type);
      if (f[j].f_type==FTYPE_DIR) {
        clean_directory(f+j);
      }
      else if (!find_in_snapshot_list(f+j))
      {
        file_set_size(f+j,0);
      }
    }
  }
}

//Функция для инкрементальных снэпшотов.
//Создает инкрементальный снимок.
int 
create_snapshot(const char * comment, const char * name)
{
  //cprintf("Welcome to create_snapshot!\n");
  struct File * new_snap;
  int r;
  char * addr;
  struct Snapshot_header new_header;

  if ((r = file_create(name, &new_snap)) < 0)
  {
    cprintf("Error create_snapshot\n");
    return r;
  }

  if (*curr_snap!=0)
  {
    //cprintf("Note: snapshot already exist\n");
    file_flush((struct File *)*curr_snap);
  }
  else 
  {
    fs_sync();
  }

  if ((r=alloc_block())<0)
    return r;
  
  new_header.old_bitmap = r;
  addr = diskaddr(r);
  memcpy(addr,diskaddr(1),BLKSIZE);
  flush_block(addr);

  /*
  //так как сделал free_block(r), чтобы old_bitmap был таким, как до создания снимка
  bitmap[r / 32] &= ~(1 << (r % 32));
  flush_block(&bitmap[r / 32]);
  */

  new_header.type = 'i';
  type = 'i';
  new_header.prev_snapshot = *curr_snap;
  strcpy(new_header.comment, comment);
  new_header.date = sys_gettime();

  *curr_snap = (uint64_t) new_snap;
  file_write(new_snap, &new_header, sizeof(struct Snapshot_header), 0);
  flush_block(curr_snap);

  //cprintf("old: %llx new: %llx \n",(unsigned long long)new_header.prev_snapshot, (unsigned long long)*curr_snap);

  //cprintf("Snapshot created name: %s size:%d %d!\n",name, new_snap->f_size, (int)sizeof(struct Snapshot_header));
  /* cprintf("%d %d %d %d %d\n",(int)sizeof(new_header.comment),(int)sizeof(new_header.date),(int)sizeof(new_header.old_bitmap),(int)sizeof(new_header.prev_snapshot),
                                (int)sizeof(new_header.type)); */

  return 0;
}


//Функция для инкрементальных снэпшотов.
//Удаляет снимок по имени.
int
delete_snapshot(const char *name)
{
  char lastelem[MAXNAMELEN];
  int r;
  struct File *dir, *f;
  struct File *snap, *help_snap;
  struct Snapshot_header new_header, help_header;
  if (*curr_snap)
  {
    snap = help_snap = (struct File *)(*curr_snap);
  }
  else
  {
    cprintf("error1 in delete_snapshot\n");
    return 0;
  }

  while (strcmp(snap->f_name, name) != 0)
  {
    file_read(snap, &new_header, sizeof(struct Snapshot_header), 0);
    if (new_header.prev_snapshot != 0)
    {
      help_snap = snap;
      snap = (struct File *)new_header.prev_snapshot;
    }
    else
    {
      cprintf("There is no such snapshot\n");
      return 0;
    }
  }

  if (!strcmp(help_snap->f_name, name))
  {
    file_read(help_snap, &new_header, sizeof(struct Snapshot_header), 0);
    *curr_snap = new_header.prev_snapshot;
    flush_block(curr_snap);
  }
  else
  {
    file_read(snap, &help_header, sizeof(struct Snapshot_header), 0);
    new_header.prev_snapshot = help_header.prev_snapshot;
    file_write(help_snap, &new_header, sizeof(struct Snapshot_header), 0);
  }
  

  file_set_size(snap, 0); //?
  if ((r = walk_path(name, &dir, &f, lastelem)) != 0)
  {
    cprintf("There is no such file to delete\n");
    return 0;
  }
  else
  {
    if (snap != f)
    {
      cprintf("I dont know why\n");
      return 0;
    }
    /*for(int i = 0; i <= MAXNAMELEN - 1; i++)
      snap->f_name[i] = '\0';*/
    memset(snap, 0, sizeof(struct File));
      //что-то не так
  }
  fs_sync();
  //cprintf("End of delete_snapshot();\n\n\n");
  return 1;
}


//Функция для инкрементальных снэпшотов.
//Применяет снимок по имени.
int
accept_snapshot(const char *name)
{
  struct Snapshot_header new_header;
  struct File *snap;
  struct File *help_snap;
  uint8_t *virt_address;
  uint32_t address;
  uint8_t value;
  off_t offset = sizeof(struct Snapshot_header);

  if (*curr_snap)
  {
    snap = (struct File *)(*curr_snap);
  }
  else
  {
    cprintf("error1 in accept_snapshot\n");
    return 0;
  }
  
  //ищем файл снапшота по названию
  while (strcmp(snap->f_name, name) != 0)
  {
    file_read(snap, &new_header, sizeof(struct Snapshot_header), 0);
    if (new_header.prev_snapshot != 0)
    {
      snap = (struct File *)new_header.prev_snapshot;
    }
    else
    {
      cprintf("There is no such snapshot\n");
      return 0;
    }
  }

  //сливаем все снапшоты до применяемого в один (меняем ссылку на предыдущий в применяемом на ноль)
  file_read(snap, &new_header, sizeof(struct Snapshot_header), 0);
  if (new_header.prev_snapshot != 0)
  {
    //cprintf("But i am not here!\n\n\n");
    help_snap = (struct File *)new_header.prev_snapshot;

    while (merge_snapshot(help_snap)) {}

    //new_header.prev_snapshot = 0;
    //file_write(snap, &new_header, sizeof(struct Snapshot_header), 0);

    //применяем все изменения и приводим фс в актуальное состояние    
    while (file_read(help_snap, &address, 4, offset) == 4)
    {
      file_read(help_snap, &value, 1, offset + 4);
      virt_address = (uint8_t *)((uint64_t)address + DISKMAP);
      *virt_address = value;
      offset += 5;
    }
    delete_snapshot(help_snap->f_name);
    
  }
  fs_sync();

  *help_curr_snap = *curr_snap;
  *curr_snap = 0;
  flush_block(super);

  return 1;

}


//Функция для инкрементальных снэпшотов.
//Включает использование снимков.
int enable_snapshot()
{
  if ((*help_curr_snap != 0) && (*curr_snap == 0))
  {
    //cprintf("help_curr_snap = %llx", (unsigned long long)*help_curr_snap);
    *curr_snap = *help_curr_snap;
    *help_curr_snap = 0;
    flush_block(super);
    return 1;
  }
  else
  {
    cprintf("Error in activate_snapshot();\n");
    return 0;
  }
}

//Функция для инкрементальных снэпшотов.
//Объединяет снимки.
int
merge_snapshot(struct File *snap)
{
  struct File *prev_snap;
  struct Snapshot_header header, prev_header;
  file_read(snap, &header, sizeof(struct Snapshot_header), 0);
  if (header.prev_snapshot)
  {
    prev_snap = (struct File *)header.prev_snapshot;
  }
  else
  {
    //cprintf("Merging snaps ended\n");
    return 0;
  }
  file_read(prev_snap, &prev_header, sizeof(struct Snapshot_header), 0);
  //надо слить snap и prev_snap

  off_t prev_offset = sizeof(struct Snapshot_header);
  off_t offset = sizeof(struct Snapshot_header);
  uint32_t prev_address, address;
  uint8_t value;
  int condition = 0;

  while (file_read(prev_snap, &prev_address, 4, prev_offset) == 4)
  {
    offset = sizeof(struct Snapshot_header);
    file_read(prev_snap, &value, 1, prev_offset + 4);
    condition = 1;
    while ((file_read(snap, &address, 4, offset) == 4) && condition)
    {
      if (prev_address == address)
      {
        condition = 0;
      }
      offset += 5;
    }
    if (condition == 1)
    {
      file_write(snap, &prev_address, 4, snap->f_size);
      file_write(snap, &value, 1, snap->f_size);
    }
    prev_offset += 5;
  }

  delete_snapshot(prev_snap->f_name);
  //header.prev_snapshot = prev_header.prev_snapshot;
  //file_write(snap, &header, sizeof(struct Snapshot_header), 0);

  file_flush(snap);

  return 1;

}

//Список снимков.
int rec_print_snapshot_list(struct File *snap, struct Snapshot_header header)
{
  struct Snapshot_header help_header;
  struct File *help_snap;
  struct tm time;
  if (header.prev_snapshot != 0)
  {
    help_snap = (struct File *)(header.prev_snapshot);
    file_read(help_snap, &help_header, sizeof(struct Snapshot_header), 0);
    rec_print_snapshot_list(help_snap, help_header);
  }
  mktime(header.date, &time);
  cprintf("   Name: %s\n", snap->f_name);
  cprintf("Comment: %s\n", header.comment);
  cprintf("   Time: %d/%d/%d %d:%d:%d\n", time.tm_mday, time.tm_mon+1, time.tm_year+1900, (time.tm_hour+3)%24, time.tm_min-2, time.tm_sec);
  cprintf("   Type: ");
  if (header.type == 'i')
    cprintf("incremental\n");
  else
    cprintf("full\n");
  
  cprintf("___________  ___________\n");
  cprintf("           \\/\n");

  return 1;
}

int print_snapshot_list()
{
  struct File *snap;
  struct Snapshot_header header;
  if (!(*curr_snap))
  {
      cprintf("There is no snapshots! Please create one\n");
      return 0;
  }
  else
  {
    snap = (struct File *)(*curr_snap);
    file_read(snap, &header, sizeof(struct Snapshot_header), 0);
    //cprintf("----------\/---------\n");
    cprintf("___________  ___________\n");
    //if (header.prev_snapshot != 0) cprintf("           \\/\n");
    cprintf("           \\/\n");

    rec_print_snapshot_list(snap, header);
    return 1;
  }
  
}
//incremantal snapshots

uint32_t *defrag_check_blocks(struct File *f, uint32_t block_num, struct File **help_file)
{
  int number;
  uint32_t * addr;
  //struct File *file = NULL;
  //cprintf("checking file %s size %d\n",f->f_name, (int)f->f_size);
  for (int pos = 0; pos < f->f_size; pos += BLKSIZE)
  {
    number = pos / BLKSIZE;
    if (number < NDIRECT)
    {
      if (f->f_direct[number] == block_num)
      {
        *help_file = f;
        return f->f_direct + number;
      }
    }
    else 
    {
      if (number == NDIRECT)
      {
        if (f->f_indirect == 0)
        {
          *help_file = NULL;
          return NULL;
        }
        addr = (uint32_t *)((uint64_t)f->f_indirect * BLKSIZE + DISKMAP);
      }

      if (addr[number - NDIRECT] == block_num)
      {
        *help_file = f;
        return addr + (number - NDIRECT);
      }
    }
  }
  *help_file = NULL;
  return NULL;
}

uint32_t *defrag_find_file(struct File * dir, uint32_t block_num, struct File **help_file)
{
  int r;
  uint32_t i, j, nblock;
  char *blk;
  struct File *f, *file = NULL;
  uint32_t *res;
  file = NULL;
  nblock = dir->f_size / BLKSIZE;
  for (i = 0; i < nblock; i++) {
    if ((r = file_get_block(dir, i, &blk)) < 0) // blk - подходящий адрес
    {
      cprintf("Something wrong with file_get_block in find_file\n");
      *help_file = NULL;
      return NULL;
    }
    f = (struct File *)blk;
    for (j = 0; j < BLKFILES; j++)
    {
      if ((f[j].f_type == FTYPE_DIR) && (f[j].f_name[0] != '.'))
      {
        res = defrag_find_file(f + j, block_num, &file);
      }
      else if (f[j].f_type == FTYPE_REG)
      {
        res = defrag_check_blocks(f + j, block_num, &file);
      }

      if (res)
      {
        *help_file = file;
        return res;
      }
    }
  }
  *help_file = NULL;
  return NULL;

}

int
is_bitmap_block(uint32_t blockno)
{
  for (int i = 0; i * BLKBITSIZE < super->s_nblocks; i++)
  {
    if ((i + 2) == blockno)
    {
      return 1;
    }
  }
  return 0;
}


int
test_de_frag(int k)
{

  struct File *curr_file, *test_for_defrag;
  uint32_t i;
  uint32_t *curr_pdiskbno;
  
 
  if (k)
  {
    int mas[1024];
    /*if (block_is_free(686))
    {
      bitmap[686 / 32] &= ~(1 << (686 % 32));
      flush_block(&bitmap[686 / 32]);
    }*/

    for(int j = 691; j < 694; j++)
    {
      if (block_is_free(j))
      {
        bitmap[j / 32] &= ~(1 << (j % 32));
        flush_block(&bitmap[j / 32]);
      }
    }

    for(int j = 701; j < 707; j++)
    {
      if (block_is_free(j))
      {
        bitmap[j / 32] &= ~(1 << (j % 32));
        flush_block(&bitmap[j / 32]);
      }
    }


    file_create("test_for_defrag", &test_for_defrag);

    for (int j = 0; j < 100; j++) 
    {
      for(int m = 0; m < 1024; m++)
      {
        mas[m] = j + m + 1;
      }
      file_write(test_for_defrag, mas, 1024, test_for_defrag->f_size);
      
    }
    file_flush(test_for_defrag);
    
    //if (!block_is_free(686)) free_block(686);

    for(int j = 691; j < 694; j++)
    {
      if (!block_is_free(j)) free_block(j);
    }

    for(int j = 701; j < 707; j++)
    {
      if (!block_is_free(j)) free_block(j);
    }
  }

  /*for(int j = 0; j < 10; j++)
  {
    cprintf("%d ", super->s_root.f_direct[j]);
  }
  cprintf("%d ", super->s_root.f_indirect);
  */
  for (i = 2; i < super->s_nblocks; i++)
  {
    if (!is_bitmap_block(i))
    {
      curr_pdiskbno = defrag_find_file(&super->s_root, i, &curr_file);
      if (curr_pdiskbno != NULL)
      {
        cprintf("block[%d] = %s\n", i, curr_file->f_name);
      }
      else
      {
        if (block_is_free(i)) cprintf("block[%d] = EMPTY\n", i); else cprintf("block[%d] = OCCUPIED\n", i); 
      }
    }
    else
    {
      cprintf("block[%d] = bitmap block \n", i);
    }
    
  }
  return 1;
}


int
de_frag()
{
  struct File *curr_file, *help_file;
  int r, newb;
  uint32_t file_nblocks, i, j;
  uint32_t *new_pdiskbno, *help_pdiskbno, *curr_pdiskbno;

  if ((newb = alloc_block()) < 0) 
  {
    cprintf("I can't handle de_frag()\n");
    return -E_NO_DISK;
  }

  for (i = 2; i < super->s_nblocks; i++)
  {
    //cprintf("i=%d\n", i);
    curr_pdiskbno = defrag_find_file(&super->s_root, i, &curr_file);
    if (curr_pdiskbno != NULL)
    {
      //cprintf("%d: %s\n", i, curr_file->f_name);
    }
    if (!is_bitmap_block(i))
    {
      curr_pdiskbno = defrag_find_file(&super->s_root, i, &curr_file);
      if (curr_pdiskbno != NULL)
      {
        //cprintf("%d: %s\n", i, curr_file->f_name);

        file_nblocks = (curr_file->f_size + BLKSIZE - 1) / BLKSIZE;
        for (j = 0; j < file_nblocks; j++)
        {
          free_block(newb);
          newb = alloc_block();

          //работаем с direct блоками curr_file'а
          if (j < NDIRECT)
          {
            help_pdiskbno = defrag_find_file(&super->s_root, i, &help_file);
            if (help_pdiskbno != NULL)
            {
              *help_pdiskbno = curr_file->f_direct[j];
              flush_block(help_file);
              memcpy(diskaddr(newb), diskaddr(i), BLKSIZE);
              flush_block(diskaddr(newb));
              memcpy(diskaddr(i), diskaddr(curr_file->f_direct[j]), BLKSIZE);
              flush_block(diskaddr(i));
              memcpy(diskaddr(curr_file->f_direct[j]), diskaddr(newb), BLKSIZE);
              flush_block(diskaddr(curr_file->f_direct[j]));
              curr_file->f_direct[j] = i;
              flush_block(curr_file);
            }
            else
            {
              if (block_is_free(i))
              {
                bitmap[i / 32] &= ~(1 << (i % 32));
                flush_block(&bitmap[i / 32]);
                memcpy(diskaddr(i), diskaddr(curr_file->f_direct[j]), BLKSIZE);
                flush_block(diskaddr(i));
                free_block(curr_file->f_direct[j]);
                curr_file->f_direct[j] = i;
                flush_block(curr_file);
              }
              else if (i == newb)
              {
                memcpy(diskaddr(i), diskaddr(curr_file->f_direct[j]), BLKSIZE);
                flush_block(diskaddr(i));
                newb = curr_file->f_direct[j];
                curr_file->f_direct[j] = i;
                flush_block(curr_file);
              }
              
            }
            if (((i + 1) < super->s_nblocks)&&((j + 1) < file_nblocks)) i++; //else cprintf("error4\n\n");
            if (is_bitmap_block(i) && ((i + 1) < super->s_nblocks)) i++;
          }

          //за direct блоками должен быть информационный блок f_indirect
          //перемещаем его, затем первый indireсt блок
          if (j == NDIRECT)
          {
            if (curr_file->f_indirect == 0)
            {
              cprintf("error2\n\n");
              return 0;
            }

            help_pdiskbno = defrag_find_file(&super->s_root, i, &help_file);

            if (help_pdiskbno != NULL)
            {
              *help_pdiskbno = curr_file->f_indirect;
              flush_block(help_file);
              memcpy(diskaddr(newb), diskaddr(i), BLKSIZE);
              flush_block(diskaddr(newb));
              memcpy(diskaddr(i), diskaddr(curr_file->f_indirect), BLKSIZE);
              flush_block(diskaddr(i));
              memcpy(diskaddr(curr_file->f_indirect), diskaddr(newb), BLKSIZE);
              flush_block(diskaddr(curr_file->f_indirect));
              curr_file->f_indirect = i;
              flush_block(curr_file);
            }
            else
            {
              if (block_is_free(i))
              {
                bitmap[i / 32] &= ~(1 << (i % 32));
                flush_block(&bitmap[i / 32]);
                memcpy(diskaddr(i), diskaddr(curr_file->f_indirect), BLKSIZE);
                flush_block(diskaddr(i));
                free_block(curr_file->f_indirect);
                curr_file->f_indirect = i;
                flush_block(curr_file);
              }
              else if (i == newb)
              {
                //cprintf("indirect = %d i = %d \n", curr_file->f_indirect, i);
                memcpy(diskaddr(i), diskaddr(curr_file->f_indirect), BLKSIZE);
                flush_block(diskaddr(i));
                newb = curr_file->f_indirect;
                curr_file->f_indirect = i;
                flush_block(curr_file);
              }
            }
            if (((i + 1) < super->s_nblocks)&&((j + 1) < file_nblocks)) i++; //else cprintf("error2\n\n");
            if (is_bitmap_block(i) && ((i + 1) < super->s_nblocks)) i++;
            free_block(newb);
            newb = alloc_block();
          }

          if (j >= NDIRECT)
          {
            if ((r = file_block_walk(curr_file, j, &curr_pdiskbno, 0)) < 0) return r;
            help_pdiskbno = defrag_find_file(&super->s_root, i, &help_file);
            if (help_pdiskbno != NULL)
            {
              //cprintf("I will do it 32\n");
              //cprintf("*curr_pdiskbno = %d\n", *curr_pdiskbno);
              //cprintf("*help_pdiskbno = %d\n", *help_pdiskbno);
              //cprintf("*curr_pdiskbno = %d *help_pdiskbno = %d\n", *curr_pdiskbno, *help_pdiskbno);
              *help_pdiskbno = *curr_pdiskbno;
              flush_block(help_file);
              memcpy(diskaddr(newb), diskaddr(i), BLKSIZE);
              flush_block(diskaddr(newb));
              memcpy(diskaddr(i), diskaddr(*curr_pdiskbno), BLKSIZE);
              flush_block(diskaddr(i));
              memcpy(diskaddr(*curr_pdiskbno), diskaddr(newb), BLKSIZE);
              flush_block(diskaddr(*curr_pdiskbno));
              //curr_file->f_indirect = i;
              new_pdiskbno = (uint32_t *) diskaddr(curr_file->f_indirect) + (j - NDIRECT);
              *new_pdiskbno = i;
              flush_block(diskaddr(curr_file->f_indirect));
            }
            else
            {
              if (block_is_free(i))
              {
                bitmap[i / 32] &= ~(1 << (i % 32));
                flush_block(&bitmap[i / 32]);
                memcpy(diskaddr(i), diskaddr(*curr_pdiskbno), BLKSIZE);
                flush_block(diskaddr(i));
                new_pdiskbno = (uint32_t *) diskaddr(curr_file->f_indirect) + (j - NDIRECT);
                free_block(*new_pdiskbno);
                *new_pdiskbno = i;
                flush_block(diskaddr(curr_file->f_indirect));              
              }
              else if (i == newb)
              {
                memcpy(diskaddr(i), diskaddr(*curr_pdiskbno), BLKSIZE);
                flush_block(diskaddr(i));
                new_pdiskbno = (uint32_t *) diskaddr(curr_file->f_indirect) + (j - NDIRECT);
                //cprintf("HI indirect = %d i = %d \n", *new_pdiskbno, i);
                newb = *new_pdiskbno;
                *new_pdiskbno = i;
                flush_block(diskaddr(curr_file->f_indirect));  
              }

            }
            if (((i + 1) < super->s_nblocks)&&((j + 1) < file_nblocks)) i++; //else cprintf("error3\n\n");
            if (is_bitmap_block(i) && ((i + 1) < super->s_nblocks)) i++;
          }
        }
        
      }
    }
  }
  if (!block_is_free(newb)) free_block(newb);
  //test_de_frag();
  cprintf("End of de_frag()\nde_frag() is good\nHappy Christmas!\n");
  return 1;
}



/*int
de_frag(struct File *dir)
{
  //hello
  struct File *f, *curr_file, *help_file;
  int r, file_found, newb;
  uint32_t dir_nblocks, file_nblocks, i, j, m, n, k, help_nblocks;
  char *blk;
  uint32_t *pdiskbno1, *pdiskbno2, *help_pdiskbno, *new_pdiskbno;
  
  if ((newb = alloc_block()) < 0) 
  {
    cprintf("I can't handle de_frag()\n");
    return -E_NO_DISK;
  }
  
  //dir = &super->s_root;
  dir_nblocks = dir->f_size / BLKSIZE;
  for (i = 0; i < dir_nblocks; i++)
  {
    if ((r = file_get_block(dir, i, &blk)) < 0) 
      return r;
    f = (struct File *)blk;
    for (j = 0; j < BLKFILES; j++)
    {
      curr_file = &f[j];
      if (curr_file->f_type != FTYPE_DIR)
      {
        //file_nblocks = curr_file->f_size / BLKSIZE
        file_nblocks = (curr_file->f_size + BLKSIZE - 1) / BLKSIZE;
        for (m = 0; m < file_nblocks - 1; m++)
        {
          //if (m <= NDIRECT - 2)
          //{
            if ((r = file_block_walk(curr_file, m, &pdiskbno1, 0)) < 0) return r;
            if ((r = file_block_walk(curr_file, m + 1, &pdiskbno2, 0)) < 0) return r; // файл из одного блока
            //первый файл блоки 2,3 ; второй - 1,4 
            //глобальная непрерывность
            if ((*pdiskbno2 - *pdiskbno1) != 1)
            {
              cprintf("I am here %s ", curr_file->f_name);
              //if (!block_is_free(*pdiskbno + 1))  !!!!!!!!!!!
              file_found = 0;
              //надо найти, какому файлу принадлежит блок с номером (*pdiskbno1 + 1)
              for (n = 0; ((n < BLKFILES) && (!file_found)); m++)
              {
                help_file = &f[n];
                //file_nblocks = curr_file->f_size / BLKSIZE
                help_nblocks = (help_file->f_size + BLKSIZE - 1) / BLKSIZE;
                for (k = 0; ((k < help_nblocks) && (!file_found)); k++)
                {
                  if ((r = file_block_walk(help_file, k, &help_pdiskbno, 0)) < 0) return r;
                  if ((*help_pdiskbno) == (*pdiskbno1 + 1))
                  {
                    file_found = 1;
                  }
                }
                //cprintf("debug message");
              }
              cprintf("Hello\n");
              if (!file_found)
              {
                cprintf("error1 in de_frag()\n");
                return 0;
              }
              //(*pdiskbno1 + 1) == (*help_pdiskbno)
              // (uint8_t * ?????)
              help_pdiskbno = find_file(&super->s_root, *pdiskbno1 + 1, help_file);
              if ((help_pdiskbno == NULL) && (help_file == NULL))
              {
                cprintf("error1 in de_frag()\n");
                return 0;
              }
              memcpy(diskaddr(newb), diskaddr(*help_pdiskbno), BLKSIZE);
              flush_block(diskaddr(newb));
              memcpy(diskaddr(*pdiskbno1 + 1), diskaddr(*pdiskbno2), BLKSIZE);
              flush_block(diskaddr(*pdiskbno1 + 1));
              memcpy(diskaddr(*pdiskbno2), diskaddr(newb), BLKSIZE);
              flush_block(diskaddr(*pdiskbno2));

              //теперь надо менять структуры File для curr_file и help_file (номера обновленных блоков)
              //k - это номер блока в файле help_file, который надо обновить
              // ???
              if (k < NDIRECT)
              {
                help_file->f_direct[k] = *pdiskbno2;
                flush_block(help_file);
              }
              else
              {
                new_pdiskbno = (uint32_t *) diskaddr(help_file->f_indirect) + (k - NDIRECT);
                *new_pdiskbno = *pdiskbno2;
                memcpy(diskaddr(*new_pdiskbno), diskaddr(*pdiskbno2), sizeof(uint32_t));
                flush_block(diskaddr(help_file->f_indirect));
              }

              if (m < NDIRECT - 1)
              {
                curr_file->f_direct[m + 1] = *pdiskbno1 + 1;
                flush_block(curr_file);
              }
              else
              {
                new_pdiskbno = (uint32_t *) diskaddr(curr_file->f_indirect) + (m + 1 - NDIRECT);
                *new_pdiskbno = *pdiskbno2;
                memcpy(diskaddr(*new_pdiskbno), diskaddr(*pdiskbno1 + 1), sizeof(uint32_t));
                flush_block(diskaddr(curr_file->f_indirect));
              }
              //надо ли перемещать сам f->indirect блок?
            }
          //}
        }
        cprintf("Hi");
      }
      else
      {
        free_block(newb);
        de_frag(curr_file);
        cprintf("hello_world");
      }
    }
  }
  cprintf("hello_world");
  //надо сделать то же самое для директории
  if (!block_is_free(newb)) free_block(newb);
  fs_sync();
  return 1;
}*/
//IZ1
