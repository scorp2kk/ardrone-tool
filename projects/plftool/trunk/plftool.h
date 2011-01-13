/*
 * plftool.h
 *
 * Copyright (c) 2011 scorp2kk, All rights reserved
 *
 * Description:
 *  This program allows to dump, create or modify plf files.
 *
 * License:
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef PLFTOOL_H_
#define PLFTOOL_H_

#include "types.h"

typedef struct s_command_args_tag
{
    const char* input_file;
    const char* output;
    int section;
    int section_type;
    u8  verbose;
    u32 action;
#define ACTION_NONE    0
#define ACTION_EXTRACT 1
#define ACTION_DUMP    2
#define ACTION_BUILD   3
#define ACTION_REPLACE 4
    u32 extract_type;
#define EXTRACT_TYPE_NICE 0
#define EXTRACT_TYPE_RAW  1
    const char* build_file;
    const char* replace_file;
} s_command_args;

extern s_command_args command_args;

#endif /* PLFTOOL_H_ */
