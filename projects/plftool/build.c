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
#include <dirent.h>
#include "plf.h"
#include "build.h"
#include "ini.h"

#if defined __WIN32__
#else
# if !defined(stricmp)
#  define stricmp strcasecmp
# endif
#endif


#define BUILD_TYPE_KERNEL   1
#define BUILD_TYPE_ARCHIVE  2


#define __GET_SECT_SAFE(var, hdl, name) (var) = ini_get_section((hdl), (name)); if ( (var) == 0 ) { printf("!!! unable to find section [%s]\n", (name)); return -1; }
#define BK_GET_SECT_SAFE(name) __GET_SECT_SAFE(ini_sect, ini_file, name)

#define __GET_PARAM_SAFE(var, hdl, name, sect) (var) = ini_get_parameter((hdl), (name), (sect)); if ( (var) == 0 ) { printf("!!! unable to find parameter %s\n", (name)); return -1; }
#define BK_GET_PARAM_SAFE(name, sect) __GET_PARAM_SAFE(ini_parm, ini_file, name, sect)


typedef struct s_exec_sect_config_tag
{
    const char* input_file;
    u32 load_addr;

} s_exec_sect_config;

typedef struct s_load_config_tag
{
    u32 entry_point;
    u32 hdr_version;
    
    struct
    {
        u32 major;
        u32 minor;
        u32 bugfix;
    } version;
    
    struct
    {
        u32 plat;
        u32 appl;
    } target;

    u32 hw_compat;
    u32 lang_zone;

    s_exec_sect_config bootloader;
    s_exec_sect_config installer;
    s_exec_sect_config volume_config;
    s_exec_sect_config main_boot;
    const char* facts_input;

} s_load_config;

typedef struct s_kernel_config_tag
{
    u32 entry_point;
    u32 hdr_version;
    struct
    {
        u32 major;
        u32 minor;
        u32 bugfix;
    } version;

    struct
    {
        u32 plat;
        u32 appl;
    } target;

    u32 hw_compat;
    u32 lang_zone;


    s_exec_sect_config zImage;
    s_exec_sect_config initrd;
    s_exec_sect_config bootparams;

} s_kernel_config;

const char* read_archive_config(const s_ini_handle* ini_file, const char* sect_name)
{
    const s_ini_section* ini_sect;
    if (ini_file == 0 || sect_name == 0)
        return NULL;

    ini_sect = ini_get_section(ini_file, sect_name );
    if (!ini_sect)
        return NULL;

    return ini_get_string(ini_file, "file", ini_sect, 0);
}

int read_exec_sect_config(s_exec_sect_config* cfg, const s_ini_handle* ini_file, const char* sect_name)
{
    const s_ini_section* ini_sect;
    if (cfg == 0 || ini_file == 0 || sect_name == 0)
        return -1;

    cfg->input_file = 0;
    cfg->load_addr = 0;

    ini_sect = ini_get_section(ini_file, sect_name );
    if (!ini_sect)
        return -1;

    cfg->input_file = ini_get_string(ini_file, "file", ini_sect, 0);
    cfg->load_addr = ini_get_int(ini_file, "LoadAddr", ini_sect, 0);

    return 0;
}

int write_plf_exec_sect(const s_exec_sect_config* cfg, int fileIdx, int type)
{
    int sectIdx;
    FILE * fp;
    s_plf_section* sect;
    int bytes_read;
    void* buffer;
    if (cfg == 0 || fileIdx < 0)
        return -1;
    
    fp = fopen(cfg->input_file, "rb");
    if (fp == 0)
    {
        printf("!!! unable to open file %s for reading\n", cfg->input_file);
        return -1;
    }

    sectIdx = plf_begin_section(fileIdx);
    if (sectIdx < 0)
    {
        printf("!!! plf_begin_section failed (%d)\n", sectIdx);
        fclose(fp);
        return -1;
    }

    sect = plf_get_section_header(fileIdx, sectIdx);
    sect->dwLoadAddr = cfg->load_addr;
    sect->dwSectionType = type;

    /* Read file and store to section */
    buffer = malloc(0x1000);
    if (buffer == 0)
    {
        printf("!!! memory allocation failed\n");
        fclose(fp);
        return -1;
    }

    do
    {
        bytes_read = fread(buffer, 1, 0x1000, fp);

        if (bytes_read > 0)
        {
            plf_write_payload(fileIdx, sectIdx, buffer, bytes_read, 0); /* ##TBD Compression */
        }
    } while(bytes_read > 0);

    free(buffer);

    plf_finish_section(fileIdx, sectIdx);

    fclose(fp);

    return 0;

}

int write_file_actions(const s_load_config* payload, int fileIdx)
{
    DIR *dirp;
    s_exec_sect_config* cfg;
    struct dirent *entry;
    char * file_path;

    dirp = opendir(payload->facts_input);
    if (!dirp) {
	    printf("Unable to open File Actions dir!\n");
	    return -1;
    }
    
    while ((entry = readdir(dirp))) {
        
        if (!strcmp (entry->d_name, "."))
            continue; 
        if (!strcmp (entry->d_name, ".."))
            continue;

	file_path = (char *)malloc(strlen(payload->facts_input) + strlen(entry->d_name) + 1);
        sprintf(file_path, "%s/%s", payload->facts_input, entry->d_name);

        cfg = (s_exec_sect_config *)malloc(sizeof(s_exec_sect_config));

        cfg->load_addr = 0;
        cfg->input_file = file_path;
	
        write_plf_exec_sect(cfg, fileIdx, 9);	
	free(cfg);
    }

    closedir (dirp);
    return 0;
}

int verify_kernel_config(const s_kernel_config* cfg)
{
    int error = 0;
    if (!cfg)
        return -1;

    if (cfg->zImage.input_file == 0)
    {
        printf("Input file for [zImage] is missing!\n");
        ++error;
    }

    if (cfg->bootparams.input_file == 0)
    {
        printf("Input file for [bootparams] is missing!\n");
        ++error;
    }

    if (cfg->hdr_version < 10 || cfg->hdr_version > 14)
    {
        printf("!!! unsupported header version");
        ++error;
    }

    return 0-error;
}

int build_load(const s_ini_handle* ini_file)
{
    const s_ini_section* ini_sect;
    s_load_config payload;
    //int has_file_action = 0;
    int plf_file_idx;
    s_plf_file* plf_file_hdr;
    int tmp_val;

    /* 1. Section file */
    __GET_SECT_SAFE(ini_sect, ini_file, "file");
    payload.entry_point = ini_get_int(ini_file, "entrypoint", ini_sect, 0);
    payload.version.major = ini_get_int(ini_file, "versionmajor", ini_sect, 0);
    payload.version.minor = ini_get_int(ini_file, "versionminor", ini_sect, 0);
    payload.version.bugfix = ini_get_int(ini_file, "versionbugfix", ini_sect, 0);
    payload.hdr_version = ini_get_int(ini_file, "hdrversion", ini_sect, 10);     /* Fixed to 10 if not defined */
    payload.target.plat = ini_get_int(ini_file, "targetplat", ini_sect, 0);
    payload.target.appl = ini_get_int(ini_file, "targetappl", ini_sect, 0);
    payload.hw_compat = ini_get_int(ini_file, "hwcompatibility", ini_sect, 0);
    payload.lang_zone = ini_get_int(ini_file, "languagezone", ini_sect, 0);

    /* 2. Section volume_config */
    if (read_exec_sect_config(&(payload.volume_config), ini_file, "volume_config") != 0)
    {
        printf("Section volume_config not found!\n");
        return -1;
    }
    /* 3. Section installer.plf */
    if (read_exec_sect_config(&(payload.installer), ini_file, "installer") != 0)
    {
        printf("Section installer.plf not found!\n");
        return -1;
    }
    /* 4. Section bootloader */
    if (read_exec_sect_config(&(payload.bootloader), ini_file, "bootloader") != 0)
    {
        printf("Section bootloader not found!\n");
        return -1;
    }
    /* 5. Section main_boot */
    if (read_exec_sect_config(&(payload.main_boot), ini_file, "main_boot") != 0)
    {
        printf("Section main_boot.plf not found!\n");
        return -1;
    }
    /* 6. Sections file_action */
    payload.facts_input = read_archive_config(ini_file, "file_actions");
    if (!payload.facts_input)
    {
        printf("Directory for file_actions not found!\n");
        return -1;
    }
    
    printf("Creating %s ...\n", command_args.output);
    
    /* Creation starts */
    plf_file_idx = plf_create_file(command_args.output);
    if (plf_file_idx < 0)
    {
        printf("!!! plf_create_file failed\n");
        return -1;
    }
    
    /* Set file header */
    plf_file_hdr = plf_get_file_header(plf_file_idx);
    plf_file_hdr->dwFileType   = 2; /* ARCHIVE TYPE */
    plf_file_hdr->dwEntryPoint = payload.entry_point;
    plf_file_hdr->dwHdrVersion = payload.hdr_version;
    plf_file_hdr->dwVersionMajor = payload.version.major;
    plf_file_hdr->dwVersionMinor = payload.version.minor;
    plf_file_hdr->dwVersionBugfix = payload.version.bugfix;
    plf_file_hdr->dwTargetAppl = payload.target.appl;
    plf_file_hdr->dwTargetPlat = payload.target.plat;
    plf_file_hdr->dwHwCompat = payload.hw_compat;
    plf_file_hdr->dwLangZone = payload.lang_zone;
    
    /* Create sections */
    printf("Writting sections...\n");
    tmp_val = 0;
    tmp_val += write_plf_exec_sect(&(payload.volume_config), plf_file_idx, 11);
    tmp_val += write_plf_exec_sect(&(payload.installer), plf_file_idx, 12);
    tmp_val += write_plf_exec_sect(&(payload.bootloader), plf_file_idx, 7);
    tmp_val += write_plf_exec_sect(&(payload.main_boot), plf_file_idx, 3);
    tmp_val += write_file_actions(&payload, plf_file_idx);

    if (tmp_val < 0)
    {
        printf("!!! some sections could not be written.. aborting\n");
        return -1;
    }
    printf("Sections written\n");

    plf_close(plf_file_idx);

    /* Verify */
    plf_file_idx = plf_open_file(command_args.output);
    tmp_val = plf_verify(plf_file_idx);
    plf_close(plf_file_idx);
    if (tmp_val < 0)
    {
        printf("plf_verify failed: %d\n", tmp_val);
        return -1;
    }


    return 0;
}

int build_kernel(const s_ini_handle* ini_file)
{
    const s_ini_section* ini_sect;
    s_kernel_config kernel;
    int has_init_rd = 0;
    int plf_file_idx;
    s_plf_file* plf_file_hdr;
    int tmp_val;
    /* const s_ini_parameter* ini_parm; */

    /* Collect configuration */

    /* 1. Section file */
    __GET_SECT_SAFE(ini_sect, ini_file, "file");
    kernel.entry_point = ini_get_int(ini_file, "entrypoint", ini_sect, 0);
    kernel.version.major = ini_get_int(ini_file, "versionmajor", ini_sect, 0);
    kernel.version.minor = ini_get_int(ini_file, "versionminor", ini_sect, 0);
    kernel.version.bugfix = ini_get_int(ini_file, "versionbugfix", ini_sect, 0);
    kernel.hdr_version = ini_get_int(ini_file, "hdrversion", ini_sect, 10);     /* Fixed to 10 if not defined */
    kernel.target.plat = ini_get_int(ini_file, "targetplat", ini_sect, 0);
    kernel.target.appl = ini_get_int(ini_file, "targetappl", ini_sect, 0);
    kernel.hw_compat = ini_get_int(ini_file, "hwcompatibility", ini_sect, 0);
    kernel.lang_zone = ini_get_int(ini_file, "languagezone", ini_sect, 0);

    /* 2. Section zImage */
    if (read_exec_sect_config(&(kernel.zImage), ini_file, "zImage") != 0)
    {
        printf("Section zImage not found!\n");
        return -1;
    }

    /* 3. Section boot params */
    if (read_exec_sect_config(&(kernel.bootparams), ini_file, "bootparams") != 0)
    {
        printf("Section bootparams not found!\n");
        return -1;
    }

    /* 4. Section initrd (optional) */
    if (read_exec_sect_config(&(kernel.initrd), ini_file, "initrd") == 0)
    {
        has_init_rd = 1;
    }

    if (verify_kernel_config(&kernel) < 0)
    {
        printf("!!! invalid configuration\n");
        return -1;
    }


    printf("Creating %s based on this config:\n", command_args.output);

    printf("File [HdrVersion: %d EntryPoint: 0x%08x; Version: %d.%d.%d\n",
            kernel.hdr_version,
            kernel.entry_point,
            kernel.version.major,
            kernel.version.minor,
            kernel.version.bugfix);
    printf("TargetPlat=%d, TargetAppl=%d, HwCompat=%d, LangZone=%d]\n",
            kernel.target.plat,
            kernel.target.appl,
            kernel.hw_compat,
            kernel.lang_zone);
    printf("  zImage     @ 0x%08x (%s)\n", kernel.zImage.load_addr, kernel.zImage.input_file);
    printf("  initrd     @ ");
    if (has_init_rd)
    {
         printf("0x%08x (%s)\n", kernel.initrd.load_addr, kernel.initrd.input_file);
    }
    else
    {
        printf("---------- (no initrd)\n");
    }
    printf("  bootparams @ 0x%08x (%s)\n", kernel.bootparams.load_addr, kernel.bootparams.input_file);
    printf("\n");


    /* Creation starts */
    plf_file_idx = plf_create_file(command_args.output);
    if (plf_file_idx < 0)
    {
        printf("!!! plf_create_file failed\n");
        return -1;
    }

    /* Set file header */
    plf_file_hdr = plf_get_file_header(plf_file_idx);
    plf_file_hdr->dwFileType   = 1;
    plf_file_hdr->dwEntryPoint = kernel.entry_point;
    plf_file_hdr->dwHdrVersion = kernel.hdr_version;
    plf_file_hdr->dwVersionMajor = kernel.version.major;
    plf_file_hdr->dwVersionMinor = kernel.version.minor;
    plf_file_hdr->dwVersionBugfix = kernel.version.bugfix;
    plf_file_hdr->dwTargetAppl = kernel.target.appl;
    plf_file_hdr->dwTargetPlat = kernel.target.plat;
    plf_file_hdr->dwHwCompat = kernel.hw_compat;
    plf_file_hdr->dwLangZone = kernel.lang_zone;


    /* Create sections */
    tmp_val = 0;
    tmp_val += write_plf_exec_sect(&(kernel.zImage), plf_file_idx, 0);
    if (has_init_rd != 0)
    {
        tmp_val += write_plf_exec_sect(&(kernel.initrd), plf_file_idx, 3);
    }
    tmp_val += write_plf_exec_sect(&(kernel.bootparams), plf_file_idx, 7);

    if (tmp_val < 0)
    {
        printf("!!! some sections could not be written.. aborting\n");
        return -1;
    }

    plf_close(plf_file_idx);

    /* Verify */
    plf_file_idx = plf_open_file(command_args.output);
    tmp_val = plf_verify(plf_file_idx);
    plf_close(plf_file_idx);
    if (tmp_val < 0)
    {
        printf("plf_verify failed: %d\n", tmp_val);
        return -1;
    }


    return 0;
}

int build(void)
{
    int ret_val;
    const s_ini_handle* ini_file;
    const s_ini_section* ini_sect;
    const s_ini_parameter* ini_parm;


    if (command_args.build_file == 0)
    {
        printf("!!! no build file specified!");
        return -1;
    }

    if (command_args.output == 0)
    {
        printf("!!! no output file specified");
        return -1;
    }

    /* Open ini file */
    ini_file = ini_open(command_args.build_file);

    if (ini_file == 0)
    {
        printf("!!! ini_open(%s) failed!", command_args.build_file);
        return -1;
    }

    /* Get file section */
    ini_sect = ini_get_section(ini_file, "file");
    if (ini_sect == 0)
    {
        printf("!!! missing section [file] in ini file %s!\n", command_args.build_file);
        ini_close((s_ini_handle*)ini_file);
        return -1;
    }

    /* Get type */
    ini_parm = ini_get_parameter(ini_file, "type", ini_sect);
    if (ini_parm == 0)
    {
        printf("!!! missing parameter type in section [file]\n");
        ini_close((s_ini_handle*)ini_file);
        return -1;
    }

    if (stricmp(ini_parm->value, "kernel") == 0)
        ret_val = build_kernel(ini_file);
    else if (stricmp(ini_parm->value, "payload") == 0)
	ret_val = build_load(ini_file);
    else
    {
        printf("!!! unkown value %s for parameter type\n", ini_parm->value);
        ini_close((s_ini_handle*)ini_file);
        ret_val = -1;
    }

    ini_close((s_ini_handle*)ini_file);

    if (ret_val == 0)
        printf(" *** %s created ***\n", command_args.output);

    return ret_val;
}
