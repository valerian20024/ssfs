#ifndef ERROR_H
#define ERROR_H

extern const int vdisk_ENODISK ;
extern const int vdisk_EACCESS ;
extern const int vdisk_ENOEXIST;
extern const int vdisk_EEXCEED ;
extern const int vdisk_ESECTOR ;
extern const int vdisk_ENOSPACE;  // when there is not enough space on disk to format it
extern const int vdisk_EINVALID;  // when the disk is not a valid SSFS partition

#endif
