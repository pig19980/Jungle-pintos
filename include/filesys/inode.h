#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/disk.h"
/*inode는 파일 시스템에서 파일이나 디렉토리에 대한 정보를 저장하는 데이터 구조이다.
inode는 파일의 메타데이터와 실제 데이터 블록에 대한 포인터를 가지고 있어, 파일
시스템이 파일을 관리하고 접근할 수 있게 도와준다.
즉 pintOS에서는 직접적인 데이터 블록의 주소가 inode에 저장되며, 이를 통해 파일 시스템이
디스크에서 데이터를 찾을 수 있게된다.*/
struct bitmap;

void inode_init(void);
bool inode_create(disk_sector_t, off_t);
struct inode *inode_open(disk_sector_t);
struct inode *inode_reopen(struct inode *);
disk_sector_t inode_get_inumber(const struct inode *);
void inode_close(struct inode *);
void inode_remove(struct inode *);
off_t inode_read_at(struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at(struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write(struct inode *);
void inode_allow_write(struct inode *);
off_t inode_length(const struct inode *);

#endif /* filesys/inode.h */
