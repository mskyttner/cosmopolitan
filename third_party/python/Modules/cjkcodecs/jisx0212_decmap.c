#include "libc/x/x.h"

static _Atomic(void *) jisx0212_decmap_ptr;
static const unsigned char jisx0212_decmap_rodata[] = {
  0xed, 0xcb, 0x2f, 0x2c, 0xc4, 0x71, 0x1c, 0xc6, 0xf1, 0xef, 0xef, 0x70, 0x87,
  0x73, 0xdc, 0xf9, 0xf7, 0x0b, 0x97, 0x9e, 0x8d, 0x09, 0xc2, 0x4d, 0x30, 0x41,
  0x3a, 0x23, 0x09, 0x08, 0x92, 0x80, 0x99, 0x60, 0x82, 0x09, 0xb7, 0x9b, 0xb1,
  0x47, 0xb0, 0x0b, 0x92, 0x60, 0x82, 0x60, 0x82, 0x24, 0x98, 0x20, 0x09, 0x92,
  0x60, 0xc2, 0x25, 0xc1, 0x84, 0x0b, 0x26, 0x48, 0x17, 0x4c, 0x90, 0x6c, 0xde,
  0x9a, 0xc9, 0x82, 0x70, 0xef, 0xed, 0xb5, 0x7d, 0x9e, 0xf0, 0x09, 0xe1, 0x7f,
  0x14, 0x85, 0xc2, 0xe6, 0xcf, 0x3d, 0x19, 0x96, 0x77, 0x96, 0xc2, 0x84, 0xbf,
  0xef, 0x93, 0xa0, 0xd9, 0x6a, 0x50, 0x79, 0x20, 0x52, 0xf9, 0xf7, 0xdf, 0x76,
  0x24, 0x3f, 0x61, 0x2c, 0x21, 0x1f, 0xe1, 0x1d, 0x73, 0x4d, 0xf2, 0x25, 0x32,
  0xcd, 0xf2, 0x3a, 0xee, 0x31, 0xd4, 0x22, 0x57, 0xf0, 0x82, 0x62, 0x52, 0x3e,
  0xc5, 0x27, 0x16, 0x52, 0xf2, 0x35, 0xe2, 0x56, 0xb9, 0x84, 0x07, 0x8c, 0xb4,
  0xc9, 0x07, 0xa8, 0x63, 0xba, 0x5d, 0x3e, 0x47, 0x2a, 0x2d, 0xaf, 0xe2, 0x16,
  0xea, 0x90, 0x77, 0x51, 0xc3, 0x78, 0x46, 0x3e, 0xc6, 0x07, 0xe6, 0x3b, 0xe5,
  0x2b, 0xe4, 0xba, 0xe4, 0x0d, 0x54, 0x31, 0x9c, 0x95, 0xf7, 0xf1, 0x8a, 0xa9,
  0x9c, 0x7c, 0x86, 0xa8, 0x5b, 0x5e, 0xc4, 0x0d, 0xf2, 0x3d, 0xf2, 0x16, 0x1e,
  0x31, 0xda, 0x2b, 0x1f, 0xe2, 0x0d, 0x33, 0x7d, 0xf2, 0x05, 0xd2, 0xfd, 0xf2,
  0x1a, 0xee, 0x30, 0x18, 0xcb, 0x7b, 0x78, 0x8e, 0xb5, 0x12, 0x1a, 0x35, 0xfa,
  0x83, 0xbe, 0x00,
};

optimizesize void *jisx0212_decmap(void) {
  return xload(&jisx0212_decmap_ptr,
               jisx0212_decmap_rodata,
               224, 1024); /* 21.875% profit */
}