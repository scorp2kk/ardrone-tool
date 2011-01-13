/*
 * ini.c
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ini.h"

#if defined(__WIN32__)
#else
# if !defined(stricmp)
#  define stricmp strcasecmp
# endif
#endif


#define IS_ALPHA(ch) ( ( (ch) >= 'a' && (ch) <= 'z') ||  ( (ch) >= 'A' && (ch) <= 'Z') )
#define IS_NUMERIC(ch) ( (ch) >= '0' && (ch) <= '9')
#define IS_POINT(ch)   ( (ch) == '.' || (ch) == ',' || (ch) == '-' || (ch) == '_' )
#define IS_ALPHA_NUMERIC(ch) ( IS_ALPHA(ch)  ||  IS_NUMERIC(ch))



#define STATE_EMPTY             0
#define STATE_SECTION_NAME      1
#define STATE_KEYNAME           2
#define STATE_VALUE             3
#define STATE_SKIP              4

int ini_read_ini(s_ini_handle* hdl);

const s_ini_handle* ini_open(const char* filename)
{
    FILE* fp;
    s_ini_handle* retval = 0;
    size_t read_bytes;
    unsigned int file_size = 0;

    if (filename == 0)
    {
        printf("no filename specified\n");
        return 0;
    }

    fp = fopen(filename, "rb");

    if (fp == 0)
    {
        printf("open failed\n");
        return 0;
    }

    retval = (s_ini_handle*)malloc(sizeof(s_ini_handle));

    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);


    retval->buffer = (unsigned char*)malloc(file_size);
    retval->buffer_size = file_size;
    retval->filename = filename;
    retval->sections = 0;
    retval->parameters = 0;

    read_bytes = fread(retval->buffer, file_size, 1, fp);
    fclose(fp);

    ini_read_ini(retval);
    free(retval->buffer);
    retval->buffer = 0;
    retval->buffer_size = 0;

    /*
    s_ini_section* cs = retval->sections;


    while(cs)
    {
        s_ini_parameter* param = cs->parameters;
        printf("[%s]\n", cs->name);
        while (param)
        {
            printf("  %s=%s\n", param->name, param->value);
            param = param->next;
        }

        cs = cs->next;
    }
    */


    return retval;
}

int ini_add_parameter(s_ini_handle* hdl, s_ini_section* section, char* key, char* value)
{
    s_ini_parameter* parameter;
    if (hdl == 0 || key == 0 || value == 0)
        return -1;

    parameter = (s_ini_parameter*)malloc(sizeof(s_ini_parameter));

    parameter->name = key;
    parameter->value = value;
    parameter->next = 0;

    if (section == 0)
    {
        if (hdl->parameters == 0)
        {
            hdl->parameters = parameter;
        }
        else
        {
            s_ini_parameter* cur = hdl->parameters;
            while (cur->next != 0)
            {
                cur = cur->next;
            }
            cur->next = parameter;
        }
    }
    else
    {
        if (section->parameters == 0)
        {
            section->parameters = parameter;
        }
        else
        {
            s_ini_parameter* cur = section->parameters;
            while (cur->next != 0)
            {
                cur = cur->next;
            }
            cur->next = parameter;
        }
    }


    return 0;
}


s_ini_section* ini_add_section(s_ini_handle* hdl, char* section_name)
{
    s_ini_section* section;
    if (hdl == 0)
        return 0;
    if (section_name == 0)
        return 0;

    section = (s_ini_section*)malloc(sizeof(s_ini_section));
    section->name = section_name;
    section->next = 0;
    section->parameters = 0;

    if (hdl->sections == 0)
    {
        hdl->sections = section;
    }
    else
    {
        s_ini_section* cur_sect = hdl->sections;
        while (cur_sect->next != 0)
        {
            cur_sect = cur_sect->next;
        }
        cur_sect->next = section;
    }

    return section;
}

int ini_read_ini(s_ini_handle* hdl)
{
    unsigned int i, current_line;
    s_ini_section* current_section = 0;
    char* current_section_name = 0;
    char* current_key_name = 0;
    char* current_value = 0;
    unsigned int name_start, value_start;

    int current_state;
    if (hdl == 0)
        return -1;

    if (hdl->buffer == 0)
        return -1;

    /* printf("ini_read_ini\n"); */


    current_state = STATE_EMPTY;
    name_start = 0;
    value_start = 0;
    current_line = 1;
    for (i = 0; i < hdl->buffer_size; ++i)
    {
        switch (current_state)
        {
        case STATE_EMPTY:
            switch (hdl->buffer[i])
            {
            case '[':
                name_start = i;
                current_state = STATE_SECTION_NAME;
                if (current_section_name != 0)
                {
                    free(current_section_name);
                    current_section_name = 0;
                }
                break;

            case ' ':
            case '\t':
            case '\n':
            case '\r':
                /* Skip white spaces */
                break;

            case '#':
                /* Skip comment line */
                current_state = STATE_SKIP;
                break;

            default:
                if ( IS_ALPHA_NUMERIC(hdl->buffer[i]) )
                {
                    current_state = STATE_KEYNAME;
                    name_start = i;
                    if (current_key_name != 0)
                    {
                        free(current_key_name);
                        current_key_name = 0;
                    }
                }
                else
                {
                    printf("Unkown char %c found at line %d\n", hdl->buffer[i], current_line);
                }
                break;

            }
            break;

        case STATE_SECTION_NAME:
            switch(hdl->buffer[i])
            {
            case '\r':
            case '\n':
                printf("Misformated INI file (missing ']') at line %d\n", current_line);
            case ']':
                {
                    unsigned int section_name_len = i - (name_start);
                    current_section_name = (char*)malloc(section_name_len);
                    memcpy(current_section_name, &hdl->buffer[name_start+1], section_name_len);
                    current_section_name[section_name_len-1] = 0x00;

                    /* printf("Found section: '%s' at line %d\n", current_section_name, current_line); */

                    current_section = ini_add_section(hdl, current_section_name);
                    current_section_name = 0; /* prevent current_section_name from being freed */
                    current_state = STATE_EMPTY;
                }
                break;
            }
            break;

        case STATE_KEYNAME:
            if (hdl->buffer[i] == '=')
            {
                unsigned int key_name_len = i - name_start + 1;
                current_key_name = (char*)malloc(key_name_len);
                memcpy(current_key_name, &hdl->buffer[name_start], key_name_len);
                current_key_name[key_name_len-1] = 0x00;

                /* printf("Found key: '%s' at line %d\n", current_key_name, current_line); */
                value_start = i;
                if (current_value != 0)
                {
                    free(current_value);
                    current_value = 0;
                }
                current_state = STATE_VALUE;
            }
            else if (hdl->buffer[i] == '\n' || hdl->buffer[i] == '\r')
            {
                printf("Missing '=' at line %d\n", current_line);
                current_state = STATE_SKIP;
            }
#if 0
            else if ( IS_ALPHA_NUMERIC(hdl->buffer[i])  || IS_POINT(hdl->buffer[i]) || hdl->buffer[i] == '[' || hdl->buffer[i] == ']'  )
            {
                /* Do nothing */
            }
            else
            {
                printf("Unkown char %c found at line %d\n", hdl->buffer[i], current_line);
                current_state = STATE_SKIP;
            }
#endif
            break;

        case STATE_VALUE:
            if (hdl->buffer[i] == '\n' || hdl->buffer[i] == '\r')
            {
                unsigned int value_len = i - value_start;
                current_value = (char*)malloc(value_len);
                memcpy(current_value, &hdl->buffer[value_start+1], value_len);
                current_value[value_len-1] = 0x00;

                /* printf("Value: %s\n", current_value); */
                current_state = STATE_EMPTY;


                ini_add_parameter(hdl, current_section, current_key_name, current_value);

                current_key_name = 0;
                current_value = 0;

            }

#if 0
            else if ( IS_ALPHA_NUMERIC(hdl->buffer[i]) || IS_POINT(hdl->buffer[i]) || hdl->buffer[i] == '[' || hdl->buffer[i] == ']' )
            {
                /* Do nothing */
            }
            else
            {
                printf("Unkown char %c found at line %d\n", hdl->buffer[i], current_line);
                current_state = STATE_SKIP;
            }
#endif
            break;

        case STATE_SKIP:
            if (hdl->buffer[i] == '\n' || hdl->buffer[i] == '\r')
            {
                current_state = STATE_EMPTY;
            }
            break;
        }

        if (hdl->buffer[i] == '\n')
        {
            ++current_line;
        }


    }

    /* Free used memory */
    if (current_section_name != 0)
    {
        free(current_section_name);
    }

    if (current_value != 0)
    {
        free(current_value);
    }

    if (current_key_name != 0)
    {
        free(current_key_name);
    }

    return 0;
}

const s_ini_section* ini_get_section(const s_ini_handle* handle, const char* section_name)
{
    const s_ini_section* cur_section;
    if (handle == 0 || section_name == 0)
        return 0;


    cur_section = handle->sections;

    while (cur_section)
    {
        if (stricmp(cur_section->name, section_name) == 0)
            return cur_section;

        cur_section = cur_section->next;
    }

    return 0;
}

const s_ini_parameter* ini_get_parameter(const s_ini_handle* handle, const char* parameter_name, const s_ini_section* section)
{
    const s_ini_parameter* cur_par;

    if (handle == 0 || parameter_name == 0)
        return 0;

    if (section != 0)
    {
        cur_par = section->parameters;
    }
    else
    {
        cur_par = handle->parameters;
    }

    while (cur_par)
    {
        if (stricmp(cur_par->name, parameter_name)== 0)
            return cur_par;

        cur_par = cur_par->next;
    }

    return 0;
}


void free_parameters(s_ini_parameter* param)
{
    while(param)
    {
        s_ini_parameter* next_param = param->next;
        free(param->name);
        free(param->value);
        free(param);
        param = next_param;
    }
}

void ini_close(s_ini_handle* handle)
{
    s_ini_section* cur_section;
    if (handle == 0)
        return;

    free_parameters(handle->parameters);

    cur_section = handle->sections;
    while(cur_section)
    {
        s_ini_section* next_section = cur_section->next;
        free_parameters(cur_section->parameters);
        free(cur_section->name);
        free(cur_section);
        cur_section = next_section;
    }

    free(handle);
}

int ini_get_int(const s_ini_handle* handle, const char* parameter_name, const s_ini_section* section, int def)
{
    const s_ini_parameter* param = ini_get_parameter(handle, parameter_name, section);
    if (param == 0)
        return def;

    return strtol(param->value, 0, 0);
}

const char* ini_get_string(const s_ini_handle* handle, const char* parameter_name, const s_ini_section* section, const char* def)
{
    const s_ini_parameter* param = ini_get_parameter(handle, parameter_name, section);
    if (param == 0)
        return def;

    return param->value;
}
