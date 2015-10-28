/*
 * plftool.c
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
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <getopt.h>
#include <fcntl.h>
#if defined __WIN32__
# include <io.h>
#else
# include <sys/stat.h>
#endif
#include "plftool.h"
#include "plf.h"

#include "build.h"
#include "replace.h"

#if defined __WIN32__
#else
# if !defined(stricmp)
#  define stricmp strcasecmp
# endif
#endif

static const struct option long_options[] =
{
        { "output", required_argument, 0, 'o' },
        { "input-file", required_argument, 0, 'i' },
        { "help", no_argument, 0, 'h' },
        { "verbose", no_argument, 0, 'v' },
        { "section-type", required_argument, 0, 't' },
        { "section", required_argument, 0, 'n' },
        { "dump", no_argument, 0, 'd' },
        { "extract", required_argument, 0, 'x' },
        { "build", required_argument, 0, 'b' },
        { "replace", required_argument, 0, 'r' },
        { 0, 0, 0, 0 }
};



static const char* section_names[][16] =
{
    {   /* 0 = UNKOWN */
        0,                      /* 0x00 */
        0,                      /* 0x01 */
        0,                      /* 0x02 */
        0,                      /* 0x03 */
        0,                      /* 0x04 */
        0,                      /* 0x05 */
        0,                      /* 0x06 */
        0,                      /* 0x07 */
        0,                      /* 0x08 */
        0,                      /* 0x09 */
        0,                      /* 0x0a */
        0,                      /* 0x0b */
        0,                      /* 0x0c */
        0,                      /* 0x0d */
        0,                      /* 0x0e */
        0,                      /* 0x0f */
    },
    {   /* 1 = EXECUTABLE */
        "zimage",               /* 0x00 */
        0,                      /* 0x01 */
        0,                      /* 0x02 */
        "initrd",               /* 0x03 */
        0,                      /* 0x04 */
        0,                      /* 0x05 */
        0,                      /* 0x06 */
        "bootparams.txt",       /* 0x07 */
        0,                      /* 0x08 */
        0,                      /* 0x09 */
        0,                      /* 0x0a */
        0,                      /* 0x0b */
        0,                      /* 0x0c */
        0,                      /* 0x0d */
        0,                      /* 0x0e */
        0,                      /* 0x0f */
    },
    {   /* 2 = ARCHIVE */

        0,                      /* 0x00 */
        0,                      /* 0x01 */
        0,                      /* 0x02 */
        "main_boot.plf",        /* 0x03 */
        0,                      /* 0x04 */
        0,                      /* 0x05 */
        0,                      /* 0x06 */
        "bootloader.bin",       /* 0x07 */
        0,                      /* 0x08 */
        "file_action",          /* 0x09 */
        0,                      /* 0x0a */
        "volume_config",        /* 0x0b */
        "installer.plf",        /* 0x0c */
        0,                      /* 0x0d */
        0,                      /* 0x0e */
        0,                      /* 0x0f */
    }
};

s_command_args command_args =
{
        .input_file = 0,
        .output = 0,
        .section = -1,
        .section_type = -1,
        .verbose = 0,
        .action = ACTION_NONE,
        .extract_type = EXTRACT_TYPE_RAW,
        .build_file = 0,
        .replace_file = 0
};

int make_dir(const char* path, u32 umask)
{
    if (path == 0)
        return -1;
#ifdef __WIN32__
    return mkdir(path);
#else
    return mkdir(path, umask);
#endif
}

int write_file(const char* path, const void* data, u32 len, u32 umask)
{
    int fi, retval;
    if (path == 0 || data == 0)
        return -1;

    if (umask == 0)
        umask = 0644;
#ifdef __WIN32__
    fi = open(path,  O_WRONLY | O_CREAT | O_BINARY, umask);
#else
    fi = open(path,  O_WRONLY | O_CREAT, umask);
#endif
    retval = 0;

    if (fi >= 0)
    {
        retval = write(fi, data, len);
        close(fi);
    }
    else
    {
        return -1;
    }
    return retval;
}

int make_dir_out(const char* path, u32 umask)
{
    int ret_val = -1;
    if (path == 0)
        return -1;

    if (command_args.output != 0)
    {
        int str_len;
        char* buffer;

        str_len = strlen(command_args.output)
                + strlen(path)
                + 10;

        buffer = (char*)malloc(str_len);

        sprintf(buffer, "%s/%s", command_args.output, path);

        ret_val = make_dir(buffer, umask);

        free(buffer);
    }
    else
    {
        ret_val = make_dir(path, umask);
    }

    return ret_val;
}

int write_file_out(const char* path, const void* data, u32 len, u32 umask)
{
    int ret_val = -1;
    if (path == 0)
        return -1;

    if (command_args.output != 0)
    {
        int str_len;
        char* buffer;

        str_len = strlen(command_args.output)
                + strlen(path)
                + 10;

        buffer = (char*)malloc(str_len);

        sprintf(buffer, "%s/%s", command_args.output, path);

        ret_val = write_file(buffer, data, len, umask);

        free(buffer);
    }
    else
    {
        ret_val = write_file(path, data, len, umask);
    }

    return ret_val;
}

int parse_options(int argc, char** argv)
{
    if (argc < 2)
        return -1;

    while(1)
    {
        int option_index;
        int result = getopt_long(argc, argv, "o:i:t:n:hvde:b:r:", long_options, &option_index);

        if (result < 0)
            return 0;


        switch(result)
        {

        case 'h':
            return -1;
            break;

        case 'o':
            command_args.output = optarg;
            break;

        case 'i':
            command_args.input_file = optarg;
            break;

        case 'v':
            command_args.verbose = 1;
            break;

        case 't':
            command_args.section_type = atoi(optarg);
            break;

        case 'n':
            command_args.section = atoi(optarg);
            break;

        case 'e':
            command_args.action = ACTION_EXTRACT;
            if ( stricmp(optarg, "raw") == 0)
            {
                command_args.extract_type = EXTRACT_TYPE_RAW;
            }
            else if (stricmp(optarg, "nice") == 0)
            {
                command_args.extract_type = EXTRACT_TYPE_NICE;
            }
            else
            {
                printf("!!! %s is not a valid extract type\n", optarg);
                return -1;
            }
            break;

        case 'b':
            command_args.build_file = optarg;
            command_args.action = ACTION_BUILD;
            break;

        case 'd':
            command_args.action = ACTION_DUMP;

            break;

        case 'r':
            command_args.action = ACTION_REPLACE;
            command_args.replace_file = optarg;
            break;

        }


    }

    return 0;
}

void print_help()
{
    printf("Usage: plftool [-d][-e] [OPTIONS]\n");

}


int do_dump()
{
    int i, num_sections, fileidx, section_start, section_end;
    s_plf_file* header;

    if (command_args.input_file == 0)
    {
        printf("!!! no input_file specified");
        return -1;
    }

    /* Open the file */
    fileidx = plf_open_file(command_args.input_file);
    if (fileidx < 0)
    {
        printf("!!! unable to open %s\n", command_args.input_file);
        return -1;
    }

    printf("*** DUMP %s ***\n\n", command_args.input_file);

    /* Get the file header and dump it */
    header = plf_get_file_header(fileidx);
    printf("    dwHdrVersion: 0x%08x\n", header->dwHdrVersion);
    printf("       dwHdrSize: 0x%08x\n", header->dwHdrSize);
    printf("   dwSectHdrSize: 0x%08x\n", header->dwSectHdrSize);
    printf("      dwFileType: 0x%08x (%s)\n", header->dwFileType,
         (header->dwFileType == 1 ? "EXECUTABLE" : "ARCHIVE"));
    printf("    dwEntryPoint: 0x%08x\n", header->dwEntryPoint);
    printf("    dwTargetPlat: 0x%08x\n", header->dwTargetPlat);
    printf("    dwTargetAppl: 0x%08x\n", header->dwTargetAppl);
    printf("      dwHwCompat: 0x%08x\n", header->dwHwCompat);
    printf("  dwVersionMajor: 0x%08x\n", header->dwVersionMajor);
    printf("  dwVersionMinor: 0x%08x\n", header->dwVersionMinor);
    printf(" dwVersionBugfix: 0x%08x\n", header->dwVersionBugfix);
    printf("      dwLangZone: 0x%08x\n", header->dwLangZone);
    printf("      dwFileSize: 0x%08x\n", header->dwFileSize);


    num_sections = plf_get_num_sections(fileidx);
    printf("-- Number of sections: %d --\n", num_sections);


    section_start = 0;
    section_end   = num_sections;

    if (command_args.section >= 0)
    {
        section_start = command_args.section;
        section_end   = command_args.section+1;
    }


    for (i = section_start; i < section_end; ++i)
    {
     s_plf_section* section = plf_get_section_header(fileidx, i);

     /* Skip not wanted section types */
     if (command_args.section_type >= 0 && section->dwSectionType != command_args.section_type )
         continue;

     printf(
             "Sect %04i: Type: 0x%08x, Size: 0x%08x, CRC32: 0x%08x, LoadAddr: 0x%08x, UncomprSize: 0x%08x\n",
             i, section->dwSectionType, section->dwSectionSize,
             section->dwCRC32, section->dwLoadAddr, section->dwUncomprSize);
    }

    printf("*** END OF DUMP ***\n\n");

    plf_close(fileidx);

    return 0;
}


int do_extract_section_nice(int fileidx, int sectionidx)
{
    /* ##TBD
    s_plf_file* file = plf_get_file_header(fileidx);
    s_plf_section* section = plf_get_section_header(fileidx, sectionidx);


    switch (section->dwSectionType)
    {

    }
    */
    return 0;
}

int do_extract()
{
    int i, num_sections, fileidx, section_start, section_end;
    int file_type = 0;
    s_plf_file* header;

    if (command_args.input_file == 0)
    {
        printf("!!! no input-file specified\n");
        return -1;
    }

    if (command_args.output == 0)
    {
        printf("!!! no output dir specified\n");
        return -1;
    }

    /* Open the file */
    fileidx = plf_open_file(command_args.input_file);
    if (fileidx < 0)
    {
        printf("!!! unable to open %s\n", command_args.input_file);
        return -1;
    }


    header = plf_get_file_header(fileidx);
    file_type = header->dwFileType;
    if (file_type > 3 || file_type < 0)
        file_type = 0;


    num_sections = plf_get_num_sections(fileidx);
    section_start = 0;
    section_end   = num_sections;

    if (command_args.section >= 0)
    {
        section_start = command_args.section;
        section_end   = command_args.section+1;
    }

    make_dir(command_args.output, 0644);

    for (i = section_start; i < section_end; ++i)
    {
        void* buffer = 0;
        const char* section_type_fmt =0;
        char section_type_name[255];
        u32 buffer_size;
        s_plf_section* section = plf_get_section_header(fileidx, i);

        /* Skip not wanted section types */
        if (command_args.section_type >= 0 && section->dwSectionType != command_args.section_type )
            continue;

        if (section->dwSectionType < 0x10u)
        {
            section_type_fmt = section_names[file_type][section->dwSectionType];
        }

        if (section_type_fmt == 0)
        {
            section_type_fmt = "unk";
        }

        sprintf(section_type_name, "%03d_0x%02x_%x_%s",
                i,
                (char)section->dwSectionType,
                (section->dwUncomprSize!=0?1:0),
                section_type_fmt);

        if (command_args.extract_type == EXTRACT_TYPE_RAW)
        {
            printf("dumping section %d (%s)\n", i, section_type_name);
            plf_get_payload_uncompressed(fileidx, i, &buffer, &buffer_size);

            write_file_out(section_type_name, buffer, buffer_size, 0);
            free(buffer);
        }
        else
        {
            do_extract_section_nice(fileidx, i);
        }
    }

    return 0;
}

int main(int argc, char** argv)
{
    int parse_option_result = parse_options(argc, argv);
    int ret_val = 0;

    if (parse_option_result < 0)
    {
        print_help();
        return -1;
    }


#if 1

    /* Determine action */
    switch (command_args.action)
    {
    case ACTION_EXTRACT:
        ret_val = do_extract();
        break;

    case ACTION_DUMP:
        ret_val = do_dump();
        break;

    case ACTION_BUILD:
        ret_val = build();
        break;

    case ACTION_REPLACE:
        ret_val = replace();
        break;

    default:
        printf("No or wrong action specified!\n");
        ret_val = -1;
        break;
    }

#endif
    return ret_val;;
}
