#ifndef COSMOPOLITAN_LIBC_NT_STRUCT_FDSET_H_
#define COSMOPOLITAN_LIBC_NT_STRUCT_FDSET_H_
COSMOPOLITAN_C_START_

struct NtFdSet {
  uint32_t fd_count;
  int64_t fd_array[64];
};

COSMOPOLITAN_C_END_
#endif /* COSMOPOLITAN_LIBC_NT_STRUCT_FDSET_H_ */