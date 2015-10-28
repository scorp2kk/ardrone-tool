/*
 * build.c
 *
 * Copyright (c) 2011 scorp2kk, All rights reserved
 *
 * Description:
 *  Functions to create a plf file.
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
#include "plf.h"
#include "replace.h"

#if defined __WIN32__
#else
# if !defined(stricmp)
#  define stricmp strcasecmp
# endif
#endif



int do_replace()
{
    int ret_val = 0;
    int fidx_input;
    int fidx_output;
    int num_sections;
    int i;
    FILE* replace_file;
    void* tmp_buf;
    s_plf_file* fileHdrOld, *fileHdrNew;

    /* Open replace file */
    replace_file = fopen(command_args.replace_file, "rb");

    if (replace_file == 0)
    {
        printf("!!! unable to open replace file %s\n", command_args.replace_file);
        return -1;
    }

    /* Open input plf */
    fidx_input = plf_open_file(command_args.input_file);
    if (fidx_input < 0)
    {
        printf("plf_open_file(%s) failed: %d\n", command_args.input_file, fidx_input);
        fclose(replace_file);
        return -1;
    }

    /* Open output plf */
    fidx_output = plf_create_file(command_args.output);
    if (fidx_output < 0)
    {
        printf("plf_create_file(%s) failed: %d\n", command_args.output, fidx_output);
        fclose(replace_file);
        plf_close(fidx_input);
        return -1;
    }

    /* Copy file header */
    fileHdrOld = plf_get_file_header(fidx_input);
    fileHdrNew = plf_get_file_header(fidx_output);

    memcpy(fileHdrNew, fileHdrOld, sizeof(s_plf_file));

    /* Get number of sections */
    num_sections = plf_get_num_sections(fidx_input);

    /* Iterate over all sections */
    tmp_buf = malloc(0x1000);
    if (tmp_buf == 0)
    {
        printf("!!! malloc failed\n");
        fclose(replace_file);
        plf_close(fidx_input);
        plf_close(fidx_output);
        return -1;
    }

    for (i = 0; i  < num_sections; ++i)
    {
        s_plf_section* newSection;
        s_plf_section* curSection = plf_get_section_header(fidx_input, i);

        int newSectIdx = plf_begin_section(fidx_output);
        if (newSectIdx < 0)
        {
            printf("!!! plf_begin_section failed: %d\n", newSectIdx);
            fclose(replace_file);
            plf_close(fidx_input);
            plf_close(fidx_output);
            free(tmp_buf);
            return -1;
        }

        printf("*** Processing section: %03d: ", newSectIdx);
        newSection = plf_get_section_header(fidx_output, newSectIdx);

        /* Copy hdr */
        newSection->dwSectionType = curSection->dwSectionType;
        newSection->dwUncomprSize = curSection->dwUncomprSize;
        newSection->dwLoadAddr    = curSection->dwLoadAddr;

        if (i != command_args.section)
        {
            /* Copy content */
            u32 bytes_count = 0;

            printf("    Copy section (%d bytes)", curSection->dwSectionSize);
            while (bytes_count < curSection->dwSectionSize)
            {
                int bytes_copied;

                /* Read */
                bytes_copied = plf_get_payload_raw(fidx_input, i, tmp_buf, bytes_count, 0x1000);

                /* Write */
                plf_write_payload(fidx_output, newSectIdx, tmp_buf, bytes_copied, 0);

                bytes_count += bytes_copied;
            }
        }
        else
        {
            /* Replace section */
            u32 bytes_read = 0;

            printf("    Replace section with content of %s", command_args.replace_file);
            do
            {
                bytes_read = fread(tmp_buf, 1, 0x1000, replace_file);

                if (bytes_read > 0)
                    plf_write_payload(fidx_output, newSectIdx, tmp_buf, bytes_read, 0);

            } while (bytes_read > 0);
            ret_val = 1;

        }

        plf_finish_section(fidx_output, newSectIdx);

        printf("\n");

    }

    free(tmp_buf);
    plf_close(fidx_input);





    plf_close(fidx_output);


    return ret_val;
}

int replace(void)
{

    if (command_args.replace_file == 0)
    {
        printf("!!! no replace file specified\n");
        return -1;
    }

    if (command_args.section < 0)
    {
        printf("!!! no section to replace specified\n");
        return -1;
    }

    if (command_args.input_file == 0)
    {
        printf("!!! no input file specified\n");
        return -1;
    }

    if (command_args.output == 0)
    {
        printf("!!! no output file specified\n");
        return -1;
    }

    if (stricmp(command_args.input_file, command_args.output) == 0)
    {
        printf("!!! input and output files are equal. This is not allowed!!!");
        return -1;
    }

    return do_replace();
}

