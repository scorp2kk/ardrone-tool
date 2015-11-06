/*
 * plf.h
 *
 * Copyright (c) 2011 scorp2kk, All rights reserved
 *
 * Description:
 *  Header file of libplf.
 *
 * License:
 *  This file is part of libplf.
 *
 *  libplf is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  libplf is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with libplf.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PLF_H_
#define PLF_H_

#include "types.h"
#include "plf_structs.h"
#include "crc32.h"

#define PLF_LIB_VERSION_MAJOR       0u
#define PLF_LIB_VERSION_MINOR       1u
#define PLF_LIB_VERSION_BUGFIX      0u


#define PLF_LIB_VERSION ( (PLF_LIB_VERSION_MAJOR << 16) | (PLF_LIB_VERSION_MINOR << 8) | (PLF_LIB_VERSION_BUGFIX) )
#define PLF_MAGIC_CODE 0x21464C50  // Not a PLF file without that.


typedef struct s_plf_version_info_tag
{
    u8 major;
    u8 minor;
    u8 bugfix;
} s_plf_version_info;


int plf_create_file(const char* filename);
int plf_create_ram(const void* buffer, u32 buffer_size);
int plf_open_file(const char* filename);
int plf_open_ram(const void* buffer, u32 buffer_size);

int plf_get_num_sections(int fileIdx);
int plf_check_crc(int fileIdx, int entryIdx);
int plf_verify(int fileIdx);

int plf_begin_section(int fileIdx);
int plf_write_payload(int fileIdx, int sectIndx, const void* buffer, u32 len, u8 compress);
int plf_finish_section(int fileIdx, int sectIdx);

int plf_get_payload_raw(int fileIdx, int sectIdx, void* dst_buffer, u32 offset, u32 len);
int plf_get_payload_uncompressed(int fileIdx, int sectIdx, void** buffer, u32* buffer_size);
s_plf_section* plf_get_section_header(int fileIdx, int sectIdx);
s_plf_file* plf_get_file_header(int fileIdx);

int plf_close(int fileIdx);

const s_plf_version_info* plf_lib_get_version(void);


#define PLF_E_NO_SPACE      -1
#define PLF_E_PARAM         -2
#define PLF_E_IO            -3
#define PLF_E_STREAM        -4
#define PLF_E_FILE_IDX      -5
#define PLF_E_MEM           -6
#define PLF_E_CRC           -7
#define PLF_E_OPENED        -8
#define PLF_E_WRITE         -9
#define PLF_E_NOT_OPENED    -10
#define PLF_E_NOT_IMPLEMENTED -11

#endif /* PLF_H_ */
