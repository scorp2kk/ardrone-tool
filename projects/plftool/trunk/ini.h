/*
 * ini.h
 *
 * Copyright (c) 2011 scorp2kk, All rights reserved
 *
 * Description:
 *  Quite simple and slow ini file parser.
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
#ifndef INI_H_
#define INI_H_

typedef struct s_ini_parameter_tag
{
    char* name;
    char* value;
    struct s_ini_parameter_tag* next;
} s_ini_parameter;

typedef struct s_ini_section_tag
{
    char* name;
    s_ini_parameter* parameters;
    struct s_ini_section_tag* next;
} s_ini_section;


typedef struct s_ini_handle_tag
{
    const char* filename;
    unsigned char* buffer;
    unsigned int buffer_size;

    s_ini_parameter* parameters;
    s_ini_section* sections;
} s_ini_handle;


const s_ini_handle* ini_open(const char* filename);
const s_ini_section* ini_get_section(const s_ini_handle* handle, const char* section_name);
const s_ini_parameter* ini_get_parameter(const s_ini_handle* handle, const char* parameter_name, const s_ini_section* section);
int ini_get_int(const s_ini_handle* handle, const char* parameter_name, const s_ini_section* section, int def);
const char* ini_get_string(const s_ini_handle* handle, const char* parameter_name, const s_ini_section* section, const char* def);

void ini_close(s_ini_handle* handle);
#endif /* INI_H_ */
