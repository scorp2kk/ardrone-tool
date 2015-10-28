/*
 * plf_inst_extract.c
 *
 * Copyright (c) 2011 scorp2kk, All rights reserved
 *
 * Description:
 *  This program extracts and modifies the installer from a given
 *  ardrone_update.plf so it can be used for USB flashing.
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
const char* output_file_name=0;


const char boot_params_default[] = "parrotparts=nand0:256K(Pbootloader),8M(Pmain_boot),8M(Pfactory),16M(Psystem),98048K(Pupdate) "
                        "console=ttyPA0,115200 loglevel=8 "
                        "g_serial.use_acm=0 g_serial.idVendor=0x19cf g_serial.idProduct=0x1000 g_serial.iManufacturer=\"Parrot SA\" "
                        "install";

const char* boot_params = boot_params_default;

static const struct option long_options[] =
{
        { "output-file", required_argument, 0, 'o' },
        { "input-file", required_argument, 0, 'i' },
        { "help", no_argument, 0, 'h' },
        { "verbose", no_argument, 0, 'v' },
        { "boot-params", required_argument, 0, 'b' },
        { 0, 0, 0, 0 }
};

void DumpPLF(int fileidx)
{
    int i, num_sections;

    s_plf_file* header = plf_get_file_header(fileidx);
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

void ExtractInstaller(const char* filename, const char* output_file)
{
    int verify_result;
    int installerIdx, i, num_entries, num_entries_installer;
    int fileIdx, fileIdxInstaller, fileIdxNew;
    s_plf_file *fileHdrInstaller, *fileHdrNew, *fileHdrOld;
    s_plf_section* installerSection = 0;
    void* buffer;

    fileIdx = plf_open_file(filename);

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
        goto ExtractInstaller_exit_1;
    }

    /* Found the installer */
    installerIdx = i;

    /* Now open the installer from RAM */
    buffer = malloc(installerSection->dwSectionSize);
    if (!buffer)
    {
        printf("!!!  unable to alloc %d bytes\n", installerSection->dwSectionSize);
        goto ExtractInstaller_exit_1;
    }
    plf_get_payload_raw(fileIdx, installerIdx, buffer, 0, installerSection->dwSectionSize);

    fileIdxInstaller = plf_open_ram(buffer, installerSection->dwSectionSize);
    if (fileIdxInstaller < 0)
    {
        printf("!!! plf_open_ram for installer failed: %d\n", fileIdxInstaller);
        goto ExtractInstaller_exit_2;
    }


    /* Dump */

    if (verbose > 0)
    {
        printf("-- Dump of Installer in %s:\n", filename);
        DumpPLF(fileIdxInstaller);
    }


    fileHdrInstaller = plf_get_file_header(fileIdxInstaller);
    printf("*** Installer found! Version: %d.%d.%d ***\n", fileHdrInstaller->dwVersionMajor, fileHdrInstaller->dwVersionMinor, fileHdrInstaller->dwVersionBugfix);

    /* Start a new plf file */
    fileIdxNew = plf_create_file(output_file);

    if (fileIdxNew < 0)
    {
        printf("!!! plf_create_file(%s) failed: %d\n", output_file, fileIdxNew);
        fflush(stdout);
        goto ExtractInstaller_exit_3;
    }


    /* Copy file header */
    fileHdrOld = plf_get_file_header(fileIdxInstaller);
    fileHdrNew = plf_get_file_header(fileIdxNew);

    memcpy(fileHdrNew, fileHdrOld, sizeof(s_plf_file));

    /* Modify entry point */
    fileHdrNew->dwEntryPoint += 0x1000000;
    printf("*** Changing executable entry point from 0x%08x to 0x%08x\n",
            fileHdrOld->dwEntryPoint, fileHdrNew->dwEntryPoint);


    /* Copy / modify sections */
    num_entries_installer = plf_get_num_sections(fileIdxInstaller);

    for (i = 0; i < num_entries_installer; ++i)
    {
        s_plf_section* newSection;
        s_plf_section* curSection = plf_get_section_header(fileIdxInstaller, i);

        int newSectIdx = plf_begin_section(fileIdxNew);
        if (newSectIdx < 0)
        {
            printf("!!! plf_begin_section failed: %d\n", newSectIdx);
            goto ExtractInstaller_exit_4;
        }

        printf("*** Processing section: %d\n", newSectIdx);
        newSection = plf_get_section_header(fileIdxNew, newSectIdx);

        /* Copy hdr */
        newSection->dwSectionType = curSection->dwSectionType;
        newSection->dwUncomprSize = curSection->dwUncomprSize;

        /* Add 16MB space at the beginning of RAM */
        newSection->dwLoadAddr = curSection->dwLoadAddr + 0x1000000;
        printf("    Changing section %d load addr from 0x%08x to 0x%08x\n",
                newSectIdx,
                curSection->dwLoadAddr ,
                newSection->dwLoadAddr);

        /* Copy content */
        if (curSection->dwSectionType != 0x7)
        {
            u32 bytes_count = 0;
            u8* copy_buf = (u8*)malloc(0x1000);

            printf("    Copy section %d (%d bytes)\n", newSectIdx, curSection->dwSectionSize);
            while (bytes_count < curSection->dwSectionSize)
            {
                int bytes_copied;

                /* Read */
                bytes_copied = plf_get_payload_raw(fileIdxInstaller, i, copy_buf, bytes_count, 0x1000);

                /* Write */
                plf_write_payload(fileIdxNew, newSectIdx, copy_buf, bytes_copied, 0);

                bytes_count += bytes_copied;
            }
            free(copy_buf);
        }
        else
        {
            /* Modify boot params */
            printf("    Modify boot params in section %d\n", newSectIdx);
            plf_write_payload(fileIdxNew, newSectIdx, boot_params, strlen(boot_params)+1, 0);
        }

        plf_finish_section(fileIdxNew, newSectIdx);

        printf("\n");
    }

    /* Close & open again */
    plf_close(fileIdxNew);
    fileIdxNew = plf_open_file(output_file);

    int verify_res = plf_verify(fileIdxNew);

    if (verify_res >= 0)
    {

        if (verbose > 0)
        {
            printf("Dump of %s:\n", output_file);
            DumpPLF(fileIdxNew);
        }
        printf("*** SUCCESS --> Installer extracted!!! ***\n");
    } else {
        printf("*** FAILED --> Installer invalid!! (%d) ***\n", verify_res);
    }

ExtractInstaller_exit_4:
    plf_close(fileIdxNew);

ExtractInstaller_exit_3:
    plf_close(fileIdxInstaller);

ExtractInstaller_exit_2:
    free(buffer);

ExtractInstaller_exit_1:
    plf_close(fileIdx);

}

void print_help(const char* name)
{
    printf("usage: %s [-h] [-o <output_file>] -i <updater_file>\n", name);

    printf("%-40s %s\n", "-h, --help",                      "Print this information");
    printf("%-40s %s\n", "-o, --output-file <output_file>", "Output file name");
    printf("%-40s %s\n", "-i, --input-file <input_file>",   "Input file name");
    printf("%-40s %s\n", "-v, --verbose",   "Verbose mode");
    /* printf("%-40s %s\n", "    --boot-params <params>",   "Boot parameters");*/

    printf("\n\n");

}

int parse_options(int argc, char** argv)
{
    if (argc < 2)
        return -1;

    while(1)
    {
        int option_index;
        int result = getopt_long(argc, argv, "i:o:h", long_options, &option_index);

        if (result < 0)
            return 0;


        switch(result)
        {
        case 'i':
            input_file_name = optarg;
            break;

        case 'o':
            output_file_name = optarg;
            break;

        case 'h':
            return -1;
            break;

        case 'v':
            verbose = 1;
            break;

        case 'b':
            boot_params = optarg;
            break;
        }


    }

    return 0;
}


int main(int argc, char** argv)
{
    const s_plf_version_info * libplf_version;
    printf("\n\nplf_inst_extract\n");
    printf("****************\n");
    printf("(c) 2011 scorp2kk, All rights reserved\n");
    printf("\n\n");

    libplf_version = plf_lib_get_version();

    printf("libplf Version: %d.%d.%d\n\n", libplf_version->major, libplf_version->minor, libplf_version->bugfix);


    if (parse_options(argc, argv) < 0 || input_file_name == 0)
    {
        print_help("plf_inst_extract");
        return 0;
    }

    if (output_file_name == 0)
    {
        output_file_name = "extracted_installer.plf";
    }

    printf("Input file: %s\n", input_file_name);
    printf("Output file: %s\n", output_file_name);
    printf("New boot parameters: %s\n\n", boot_params),


    ExtractInstaller(input_file_name, output_file_name);

    printf("==> DONE <==\n\n");

    return 0;
}
