/*
 * plf_structs.h
 *
 * Copyright (c) 2011 scorp2kk, All rights reserved
 *
 * Description:
 *  Contains the headers of a generic PLF file (Version > 10).
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

#ifndef PLF_STRUCTS_H_
#define PLF_STRUCTS_H_

#include "types.h"

/* PLF Headers */
typedef struct s_plf_file_tag /* Size: 0x38 Byte */
{
    u32 dwMagic;        /* +0x00 Magic Number */
    u32 dwHdrVersion;   /* +0x04 Header Version */
    u32 dwHdrSize;      /* +0x08 Header Size */
    u32 dwSectHdrSize;  /* +0x0C Section Header Size */
    u32 dwFileType;     /* +0x10 File type (1 = Executable, 2 = Archive) */
    u32 dwEntryPoint;   /* +0x14 Executable entry point */
    u32 dwTargetPlat;   /* +0x18 Target platform */
    u32 dwTargetAppl;   /* +0x1C Target application */
    u32 dwHwCompat;     /* +0x20 Hardware compatibility */
    u32 dwVersionMajor; /* +0x24 Version */
    u32 dwVersionMinor; /* +0x28 Edition */
    u32 dwVersionBugfix;/* +0x2C Extension */
    u32 dwLangZone;     /* +0x30 Language zone */
    u32 dwFileSize;     /* +0x34 File size */
} s_plf_file;

/* Section header */
typedef struct s_plf_section_tag /* Size: 0x14 Byte */
{
    u32 dwSectionType;  /* +0x00  => Types in in plftool/plftool.c */
    u32 dwSectionSize;  /* +0x04 Size of the content of the section */
    u32 dwCRC32;        /* +0x08 */
    u32 dwLoadAddr;     /* +0x0C */
    u32 dwUncomprSize;  /* +0x10 */
} s_plf_section;

#endif /* PLF_STRUCTS_H_ */
