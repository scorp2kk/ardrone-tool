/*
 * plfinfo.c
 *
 * Copyright (c) 2015 @SteveClement, All rights reserved
 *
 * Description:
 *   Returns information on any given .plf file
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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#include "plf.h"


int verbose = 0;
const char* input_file_name =0;

const char boot_params_default[] = "parrotparts=nand0:256K(Pbootloader),8M(Pmain_boot),8M(Pfactory),16M(Psystem),98048K(Pupdate) "
                        "console=ttyPA0,115200 loglevel=8 "
                        "g_serial.use_acm=0 g_serial.idVendor=0x19cf g_serial.idProduct=0x1000 g_serial.iManufacturer=\"Parrot SA\" "
                        "install";

const char boot_params_minidrone[] = "parrotparts=nand0:256K(Pbootloader),16M(Pmain_boot),8M(Pfactory),32M(Psystem),73472K(Pupdate) "
                        "console=ttyPA1,115200 loglevel=5 "
                        "quiet lpj=1038336 ubi.mtd=Pfactory,2048 ubi.mtd=Psystem,2048 root=ubi1:system rootfstype=ubifs ubi.mtd=Pupdate,2048  boxinit.plateform=PTA_P6 boxinit.application=PA_DELOSEVO";


const char* boot_params = boot_params_default;

static const struct option long_options[] =
{
        { "input-file", required_argument, 0, 'i' },
        { "help", no_argument, 0, 'h' },
        { "verbose", no_argument, 0, 'v' },
        { 0, 0, 0, 0 }
};

void DumpPLF(int fileidx)
{
    int i, num_sections;

    s_plf_file* header = plf_get_file_header(fileidx);
    printf("         dwMagic: 0x%08x\n", header->dwMagic);
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

    for (i = 0; i < num_sections; ++i)
    {
        s_plf_section* section = plf_get_section_header(fileidx, i);

        printf(
                "Sect %04i: Type: 0x%08x, Size: 0x%08x, CRC32: 0x%08x, LoadAddr: 0x%08x, UncomprSize: 0x%08x\n",
                i, section->dwSectionType, section->dwSectionSize,
                section->dwCRC32, section->dwLoadAddr, section->dwUncomprSize);
    }



    printf("*** END OF DUMP ***\n\n");

}

void FileInfo(const char* filename)
{
    int verify_result;
    int installerIdx, i, num_entries;
    int fileIdx, fileIdxInstaller;
    s_plf_file *fileHdrInstaller;
    s_plf_section* installerSection = 0;
    void* buffer;

    fileIdx = plf_open_file(filename);

    DumpPLF(fileIdx);

    if (fileIdx < 0)
    {
        printf("!!! plf_open_file(%s) failed: %d\n", filename, fileIdx);
        return;
    }

    /* Verify the file */
    verify_result = plf_verify(fileIdx);

    if (verify_result < 0)
    {
        printf("!!! plf_verify(%s) failed: %d\n", filename, verify_result);
    }

    /* Find the installer section */
    num_entries = plf_get_num_sections(fileIdx);

    for(i = 0; i < num_entries; ++i)
    {
        installerSection = plf_get_section_header(fileIdx, i);

        if (installerSection != 0 && installerSection->dwSectionType == 0x0c)
            break;
    }

    if (i >= num_entries || installerSection == 0)
    {
        printf("!!! unable to find installer section in %s\n", filename);
        goto FileInfo_exit_1;
    }

    /* Found the installer */
    installerIdx = i;

    /* Now open the installer from RAM */
    buffer = malloc(installerSection->dwSectionSize);
    if (!buffer)
    {
        printf("!!!  unable to alloc %d bytes\n", installerSection->dwSectionSize);
        goto FileInfo_exit_1;
    }
    plf_get_payload_raw(fileIdx, installerIdx, buffer, 0, installerSection->dwSectionSize);

    fileIdxInstaller = plf_open_ram(buffer, installerSection->dwSectionSize);
    if (fileIdxInstaller < 0)
    {
        printf("!!! plf_open_ram for installer failed: %d\n", fileIdxInstaller);
        goto FileInfo_exit_2;
    }


    fileHdrInstaller = plf_get_file_header(fileIdxInstaller);
    printf("*** Installer found! Version: %d.%d.%d ***\n", fileHdrInstaller->dwVersionMajor, fileHdrInstaller->dwVersionMinor, fileHdrInstaller->dwVersionBugfix);

FileInfo_exit_2:
    free(buffer);

FileInfo_exit_1:
    plf_close(fileIdx);

}

void print_help(const char* name)
{
    printf("usage: %s [-h] [-o <output_file>] -i <updater_file>\n", name);

    printf("%-40s %s\n", "-h, --help",                      "Print this information");
    printf("%-40s %s\n", "-i, --input-file <input_file>",   "Input file name");
    printf("%-40s %s\n", "-v, --verbose",   "Verbose mode");

    printf("\n\n");

}

int parse_options(int argc, char** argv)
{
    if (argc < 1)
        return -1;

    while(1)
    {
        int option_index;
        int result = getopt_long(argc, argv, "i:h", long_options, &option_index);

        if (result < 0)
            return 0;


        switch(result)
        {
        case 'i':
            input_file_name = optarg;
            break;

        case 'h':
            return -1;
            break;

        case 'v':
            verbose = 1;
            break;

        }


    }

    return 0;
}


int main(int argc, char** argv)
{
    const s_plf_version_info * libplf_version;
    printf("\n\nplfinfo\n");
    printf("****************\n");
    printf("(c) 2015 @SteveClement, All rights reserved\n");
    printf("\n\n");

    libplf_version = plf_lib_get_version();

    printf("libplf Version: %d.%d.%d\n\n", libplf_version->major, libplf_version->minor, libplf_version->bugfix);


    if (parse_options(argc, argv) < 0 || input_file_name == 0)
    {
        print_help("plfinfo");
        return 0;
    }

    printf("Input file: %s\n", input_file_name);

    FileInfo(input_file_name);

    printf("==> DONE <==\n\n");

    return 0;
}
