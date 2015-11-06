/*
 * plf.c
 *
 * Copyright (c) 2011 scorp2kk, All rights reserved
 *
 * Description:
 *  Implementation of libplf.
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
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "plf_int.h"

#ifdef __WIN32__
# define F_O_BINARY O_BINARY
#else
# define F_O_BINARY 0
#endif

/* From gzip.c */
int  gz_uncompress (u8 *dest, u32 *destLen, const u8 *source, u32 sourceLen);

/* Locals */

static int plf_int_new_file();
static int plf_int_read(int fileIdx, void* dst, u32 offset, u32 len);
static int plf_int_write(int fileIdx, const void* src, u32 offset, u32 len);
static int plf_int_open_file(int fileIdx);
static int plf_int_read_entries(int fileIdx);
static int plf_int_add_entry(int fileIdx, const s_plf_section* section, u32 offset);
static s_plf_section_entry* plf_int_get_section(int fileIdx, int sectIdx);

static s_plf_file_entry plf_files[PLF_MAX_ALLOWED_FILES];

static const s_plf_version_info plf_lib_version = {
        .major = PLF_LIB_VERSION_MAJOR,
        .minor = PLF_LIB_VERSION_MINOR,
        .bugfix= PLF_LIB_VERSION_BUGFIX
};


/*
 * Lib version
 */
const s_plf_version_info* plf_lib_get_version(void)
{
    return &plf_lib_version;
}



/*
 * Open a file fom the file system
 */
int plf_open_file(const char* filename)
{
    int fileIdx;
    s_plf_file_entry* fileEntry;

    if (filename == 0)
        return PLF_E_PARAM;

    /* Reserve a new file */
    fileIdx = plf_int_new_file();
    if (fileIdx < 0)
        return fileIdx;

    fileEntry = &plf_files[fileIdx];

    /* Open the given filename */
    fileEntry->fildes = open(filename, O_RDONLY | F_O_BINARY);

    if (fileEntry->fildes < 0)
    {
        plf_close(fileIdx);
        return PLF_E_IO;
    }

    fileEntry->flags |= PLF_FILE_FLAG_READ;

    return plf_int_open_file(fileIdx);
}

/*
 * Open entry point to PLF file from memory
 */
int plf_open_ram(const void* buffer, u32 buffer_size)
{
    int fileIdx;
    s_plf_file_entry* fileEntry;

    if (buffer == 0)
        return PLF_E_PARAM;

    /* Reserve a new file */
    fileIdx = plf_int_new_file();
    if (fileIdx < 0)
        return fileIdx;

    fileEntry = &plf_files[fileIdx];
    fileEntry->buffer = buffer;
    fileEntry->buffer_size = buffer_size;

    fileEntry->flags |= PLF_FILE_FLAG_READ;

    return plf_int_open_file(fileIdx);
}


/*
 * New PLF File
 */
int plf_create_file(const char* filename)
{
    int fileIdx;
    s_plf_file_entry* fileEntry;

    /* Reserve a new file */
    fileIdx = plf_int_new_file();
    if (fileIdx < 0)
        return fileIdx;

    fileEntry = &plf_files[fileIdx];

    /* Open the given filename */
    fileEntry->fildes = open(filename,  O_RDWR | O_CREAT | F_O_BINARY, 0644  );

    if (fileEntry->fildes < 0)
    {
        plf_close(fileIdx);
        return PLF_E_IO;
    }

    fileEntry->hdr.dwHdrSize = sizeof(s_plf_file);
    fileEntry->hdr.dwSectHdrSize = sizeof(s_plf_section);
    fileEntry->hdr.dwHdrVersion = 0x0A;

    fileEntry->current_size =  plf_int_write(fileIdx, &(fileEntry->hdr), 0, sizeof(s_plf_file));



    fileEntry->flags |= PLF_FILE_FLAG_WRITE;

    return fileIdx;
}


/*
 * Close the files, cleanup the memory
 */
int plf_close(int fileIdx)
{
    s_plf_file_entry* fileEntry;
    PLF_VERIFY_IDX(fileIdx);

    fileEntry = &plf_files[fileIdx];

    if (fileEntry->flags & PLF_FILE_FLAG_WRITE)
    {
        /* Renew file header */
        fileEntry->hdr.dwFileSize = fileEntry->current_size;
        plf_int_write(fileIdx, &(fileEntry->hdr), 0, sizeof(s_plf_file));
    }

    if (fileEntry->fildes >= 0)
    {
        close(fileEntry->fildes);
    }

    fileEntry->hdr.dwMagic = 0;
    fileEntry->fildes = -1;

    if (fileEntry->entries != 0)
    {
        s_plf_section_entry* nextEntry = 0;
        s_plf_section_entry* curEntry = fileEntry->entries;

        while (curEntry)
        {
            nextEntry = curEntry->next;
            free(curEntry);
            curEntry = nextEntry;
        }

        fileEntry->entries = 0;
    }

    fileEntry->num_entries = 0;

    return 0;
}

/*
 * Get PLF header
 */
s_plf_file* plf_get_file_header(int fileIdx)
{
    if (fileIdx >= PLF_MAX_ALLOWED_FILES || plf_files[fileIdx].hdr.dwMagic
            != PLF_MAGIC_CODE)
    {
        return 0;
    }

    return &(plf_files[fileIdx].hdr);
}


/*
 * Number of section in the file
 */
int plf_get_num_sections(int fileIdx)
{
    PLF_VERIFY_IDX(fileIdx);
    return plf_files[fileIdx].num_entries;
}

/*
 * Get Section header
 */
s_plf_section* plf_get_section_header(int fileIdx, int sectIdx)
{
    s_plf_section_entry* curEntry;
    curEntry = plf_int_get_section(fileIdx, sectIdx);

    if (!curEntry)
        return 0;

    return &(curEntry->hdr);
}


/*
 * Content of a section
 */
int plf_get_payload_raw(int fileIdx, int sectIdx, void* dst_buffer, u32 offset,
        u32 len)
{
    s_plf_section_entry* curEntry;

    PLF_VERIFY_IDX(fileIdx);
    curEntry = plf_int_get_section(fileIdx, sectIdx);

    if (!curEntry)
        return PLF_E_PARAM;

    //printf("plf_get_payload_raw [%d,%d]: offset: 0x%08x len: 0x%08x\n", fileIdx, sectIdx, curEntry->offset, curEntry->hdr.dwSectionSize);
    if (curEntry->hdr.dwSectionSize - offset < len)
        len = curEntry->hdr.dwSectionSize - offset;

    return plf_int_read(fileIdx, dst_buffer, curEntry->offset + offset, len);
}


/*
 * Content of a section, conpressed
 */
int plf_get_payload_uncompressed(int fileIdx, int sectIdx, void** buffer, u32* buffer_size)
{
    s_plf_section_entry* curEntry;
    void* tmpBuffer;

    PLF_VERIFY_IDX(fileIdx);
    curEntry = plf_int_get_section(fileIdx, sectIdx);

    if (!curEntry || buffer == 0 || buffer_size == 0)
        return PLF_E_PARAM;

    /* Allocate a temp buffer */
    tmpBuffer = malloc(curEntry->hdr.dwSectionSize);

    if (tmpBuffer == 0)
        return PLF_E_MEM;

    *buffer_size = 0;
    *buffer = 0;

    /* Read raw data */
    plf_get_payload_raw(fileIdx, sectIdx, tmpBuffer, 0, curEntry->hdr.dwSectionSize);

    if (curEntry->hdr.dwUncomprSize == 0)
    {
        *buffer = tmpBuffer;
        *buffer_size = curEntry->hdr.dwSectionSize;
    }
    else
    {
        void* uncomprBuffer;
        int gz_ret;

        /* Allocate the uncompressed buffer */
        uncomprBuffer = malloc(curEntry->hdr.dwUncomprSize);
        if (uncomprBuffer == 0)
        {
            free(tmpBuffer);
            return PLF_E_MEM;
        }

        *buffer_size = curEntry->hdr.dwUncomprSize;

        /* Decompress */
        gz_ret = gz_uncompress(uncomprBuffer, buffer_size, tmpBuffer, curEntry->hdr.dwSectionSize);
        if (gz_ret < 0)
        {
            *buffer_size = 0;
            free(uncomprBuffer);
            free(tmpBuffer);
            return PLF_E_STREAM;
        }

        free(tmpBuffer);
        *buffer = uncomprBuffer;
    }

    return 0;
}

/*
 * CRC32 checksum of a section
 */
int plf_check_crc(int fileIdx, int sectIdx)
{
    u32 crc_accum = 0;
    u32 crc_num_size = 0;
    s_plf_section_entry* section;
    s_plf_file_entry* fileEntry;

    PLF_VERIFY_IDX(fileIdx);

    section = plf_int_get_section(fileIdx, sectIdx);
    fileEntry = &plf_files[fileIdx];

    if (section == 0)
        return PLF_E_PARAM;

    if (fileEntry->fildes == -1)
    {
        // Buffer case

        crc32_calc_buffer(&crc_accum, &crc_num_size, (((u8*) fileEntry->buffer)
                + section->offset), section->hdr.dwSectionSize);
        crc32_calc_dw(&crc_accum, &crc_num_size);
    }
    else
    {
        // File case.. we need to read the contents of the file
        u8* tmpBuf = (u8*) malloc(0x1000);

        if (tmpBuf == 0)
            return PLF_E_MEM;

        u32 bytes_remaining = section->hdr.dwSectionSize;

        while (bytes_remaining > 0)
        {
            u32 read_len;
            if (bytes_remaining > 0x1000)
            {
                read_len = 0x1000;
            }
            else
            {
                read_len = bytes_remaining;
            }

            u32 offset = section->hdr.dwSectionSize - bytes_remaining;

            u32 bytes_read = plf_get_payload_raw(fileIdx, sectIdx, tmpBuf,
                    offset, read_len);
            if (bytes_read < 0)
            {
                free(tmpBuf);
                return bytes_read;
            }

            crc32_calc_buffer(&crc_accum, &crc_num_size, tmpBuf, read_len);

            bytes_remaining -= read_len;

        }

        // finish crc
        free(tmpBuf);
        crc32_calc_dw(&crc_accum, &crc_num_size);

    }

    if (crc_accum == section->hdr.dwCRC32)
        return 0;
    else
        return PLF_E_CRC;

}

/*
 * Verify CRC32 of all sections
 */
int plf_verify(int fileIdx)
{
    int i, num_entries;

    PLF_VERIFY_IDX(fileIdx);

    num_entries = plf_get_num_sections(fileIdx);

    if (num_entries <= 0)
        return num_entries;

    for (i = 0; i < num_entries; ++i)
    {
        if (plf_check_crc(fileIdx, i) != 0)
            return PLF_E_CRC;
    }


    return 0;
}

/*
 * Initialize new section
 */
int plf_begin_section(int fileIdx)
{
    int newSctIdx;
    s_plf_file_entry* fileEntry;

    PLF_VERIFY_IDX(fileIdx);

    fileEntry = &plf_files[fileIdx];

    /* Check if this filentry is writable */
    if ( (fileEntry->flags & PLF_FILE_FLAG_WRITE) == 0)
        return PLF_E_WRITE;

    /* Check if section previous section is closed */
    if ( (fileEntry->flags & PLF_FILE_FLAG_SECTOPEN) != 0)
        return PLF_E_OPENED;

    fileEntry->flags |= PLF_FILE_FLAG_SECTOPEN;

    /* Create a new section */
    s_plf_section* newSection = (s_plf_section*)malloc(sizeof(s_plf_section));

    newSection->dwCRC32=0;
    newSection->dwLoadAddr=0;
    newSection->dwSectionSize=0;
    newSection->dwSectionType=0;
    newSection->dwUncomprSize=0;

    newSctIdx = plf_int_add_entry(fileIdx, newSection, fileEntry->current_size + sizeof(s_plf_section));

    if (newSctIdx >= 0)
    {
        if (plf_int_write(fileIdx, newSection, fileEntry->current_size, sizeof(s_plf_section)) == sizeof(s_plf_section))
        {
            fileEntry->flags |= PLF_FILE_FLAG_SECTOPEN;
            fileEntry->current_size += sizeof(s_plf_section);
        }
    }

    free(newSection);
    return newSctIdx;
}

/*
 * Add payload to a section
 */
int plf_write_payload(int fileIdx, int sectIndx, const void* buffer, u32 len, u8 compress)
{
    u32 bytes_written;
    s_plf_file_entry* fileEntry;
    s_plf_section_entry* sectEntry;

    PLF_VERIFY_IDX(fileIdx);

    fileEntry = &plf_files[fileIdx];

    if (sectIndx > fileEntry->num_entries || buffer == 0)
        return PLF_E_PARAM;

    /* Check if this filentry is writable */
    if ( (fileEntry->flags & PLF_FILE_FLAG_WRITE) == 0)
        return PLF_E_WRITE;

    /* Check if section previous section is opened */
    if ( (fileEntry->flags & PLF_FILE_FLAG_SECTOPEN) == 0)
        return PLF_E_NOT_OPENED;

    if (compress != 0)
        return PLF_E_NOT_IMPLEMENTED;

    sectEntry = plf_int_get_section(fileIdx, sectIndx);

    bytes_written =  plf_int_write(fileIdx, buffer, fileEntry->current_size, len);

    if (bytes_written > 0)
    {
        u32 num_crc=0;
        fileEntry->current_size += bytes_written;
        sectEntry->hdr.dwSectionSize += bytes_written;

        crc32_calc_buffer(&sectEntry->hdr.dwCRC32, &num_crc, buffer, bytes_written);
    }

    return bytes_written;
}

/*
 * Finalize section, compute CRC 32, add to file
 */
int plf_finish_section(int fileIdx, int sectIdx)
{
    s_plf_file_entry* fileEntry;
    s_plf_section_entry* sectEntry;
    int bytes_written;

    PLF_VERIFY_IDX(fileIdx);

    fileEntry = &plf_files[fileIdx];

    if (sectIdx > fileEntry->num_entries)
        return PLF_E_PARAM;

    /* Check if this filentry is writable */
    if ( (fileEntry->flags & PLF_FILE_FLAG_WRITE) == 0)
        return PLF_E_WRITE;

    /* Check if section previous section is opened */
    if ( (fileEntry->flags & PLF_FILE_FLAG_SECTOPEN) == 0)
        return PLF_E_NOT_OPENED;

    sectEntry = plf_int_get_section(fileIdx, sectIdx);


    crc32_calc_dw(&sectEntry->hdr.dwCRC32, &sectEntry->hdr.dwSectionSize);

    /* Renew header */
    bytes_written =  plf_int_write(fileIdx, &(sectEntry->hdr), sectEntry->offset-sizeof(s_plf_section), sizeof(s_plf_section));

    if (bytes_written != sizeof(s_plf_section) )
        return bytes_written;


    /* Align */
    int bytes_to_skip = 4 - (sectEntry->hdr.dwSectionSize & 3);
    if (bytes_to_skip != 4)
    {
        u32 data = 0;
        plf_int_write(fileIdx, &data, fileEntry->current_size, bytes_to_skip );

        fileEntry->current_size += bytes_to_skip;

    }

    fileEntry->flags &= ~PLF_FILE_FLAG_SECTOPEN;

    return 0;
}

/*
 * Create a new file.
 */
static int plf_int_new_file()
{
    int fileIdx;
    s_plf_file_entry* fileEntry;

    /* Look for a free index in the plf_file array */
    for (fileIdx = 0; fileIdx < PLF_MAX_ALLOWED_FILES; ++fileIdx)
    {
        if (plf_files[fileIdx].hdr.dwMagic != PLF_MAGIC_CODE)
            break;
    }

    /* No free entry left */
    if (fileIdx >= PLF_MAX_ALLOWED_FILES)
        return PLF_E_NO_SPACE;

    /* Init this entry */
    fileEntry = &plf_files[fileIdx];
    fileEntry->buffer = 0;
    fileEntry->buffer_size = 0;
    fileEntry->entries = 0;
    fileEntry->fildes = -1;
    fileEntry->num_entries = 0;
    fileEntry->flags = 0;
    fileEntry->current_size = 0;

    fileEntry->hdr.dwMagic = PLF_MAGIC_CODE;

    /* Found a free index */
    return fileIdx;
}


static s_plf_section_entry* plf_int_get_section(int fileIdx, int sectIdx)
{
    int i;
    s_plf_section_entry* curEntry;

    if (fileIdx >= PLF_MAX_ALLOWED_FILES || plf_files[fileIdx].hdr.dwMagic
            != PLF_MAGIC_CODE)
        return 0;

    if ((u32) sectIdx >= plf_files[fileIdx].num_entries)
        return 0;

    i = 0;
    curEntry = plf_files[fileIdx].entries;

    while (curEntry)
    {
        if (i == sectIdx)
            break;

        curEntry = curEntry->next;
        ++i;
    }

    return curEntry;
}

/*
 * Read all the entries
 */
static int plf_int_read_entries(int fileIdx)
{
    s_plf_file_entry* fileEntry;
    u32 current_offset;
    int ret_val;

    PLF_VERIFY_IDX(fileIdx);

    fileEntry = &plf_files[fileIdx];

    /* Check if entries are already read */
    if ( (fileEntry->flags & PLF_FILE_FLAG_OPENED) != 0)
        return fileEntry->num_entries;

    fileEntry->flags |= PLF_FILE_FLAG_OPENED;

    /* Skip file header */
    current_offset = sizeof(s_plf_file);

    if (fileEntry->fildes == -1)
    {
        s_plf_section* tmpSection;

        /* Load from RAM */
        while (current_offset < fileEntry->buffer_size)
        {
            u32 section_offset_start = current_offset;

            /* Check if entry header is available */
            if (current_offset + fileEntry->hdr.dwSectHdrSize
                    > fileEntry->buffer_size)
                break;

            /* Read entry header */
            tmpSection = (s_plf_section*) ((u8*) (fileEntry->buffer)
                    + current_offset);

            /* Skip payload */
            current_offset += fileEntry->hdr.dwSectHdrSize
                    + tmpSection->dwSectionSize;

            /* Check if payload is completely available */
            if (current_offset > fileEntry->buffer_size)
                break;

            /* Add entry */
            ret_val = plf_int_add_entry(fileIdx, tmpSection,
                    fileEntry->hdr.dwSectHdrSize + section_offset_start);
            if (ret_val < 0)
            {
                return ret_val;
            }

            /* Align */
            int bytes_to_seek = 4 - (tmpSection->dwSectionSize & 3);
            if (bytes_to_seek != 4)
            {
                current_offset += bytes_to_seek;
            }
        }
    }
    else
    {
        /* load from file */
        struct stat file_stat;

        /* get file details */
        if (fstat(fileEntry->fildes, &file_stat) < 0)
        {
            return PLF_E_IO;
        }

        /* allocate memory for the read buffer */
        s_plf_section* tmpSection = (s_plf_section*) malloc(
                fileEntry->hdr.dwSectHdrSize);

        if (tmpSection == 0)
            return PLF_E_MEM;

        /* seek to the first entry */
        current_offset = lseek(fileEntry->fildes, fileEntry->hdr.dwHdrSize,
                SEEK_SET);

        /* read all entries */
        while (current_offset >= 0 && current_offset < file_stat.st_size)
        {
            int read_bytes;

            u32 section_offset_start = current_offset;

            /* read the header */
            read_bytes = read(fileEntry->fildes, tmpSection,
                    fileEntry->hdr.dwSectHdrSize);

            /* check if at least the header was read */
            if (read_bytes < fileEntry->hdr.dwSectHdrSize)
                break;

            /* skip payload of this header */
            current_offset = lseek(fileEntry->fildes,
                    tmpSection->dwSectionSize, SEEK_CUR);

            /* check if payload is available in the file */
            if (current_offset > file_stat.st_size)
                break;

            /* add the entry */
            ret_val = plf_int_add_entry(fileIdx, tmpSection,
                    fileEntry->hdr.dwSectHdrSize + section_offset_start);

            if (ret_val < 0)
            {
                free(tmpSection);
                return ret_val;
            }

            /* Align... */
            int bytes_to_seek = 4 - (tmpSection->dwSectionSize & 3);
            if (bytes_to_seek != 4)
            {
                current_offset = lseek(fileEntry->fildes, bytes_to_seek,
                        SEEK_CUR);
            }
        } /* while(...) */

        free(tmpSection);
    }

    return 0;
}


/*
 * Open entry point of the file. This section is a s_plf_file (see plf_structs.h)
 */
static int plf_int_open_file(int fileIdx)
{
    int retval;
    s_plf_file_entry* fileEntry;
    int bytes_read;

    PLF_VERIFY_IDX(fileIdx);

    fileEntry = &plf_files[fileIdx];

    bytes_read = plf_int_read(fileIdx, (void*) (&fileEntry->hdr), 0,
            sizeof(s_plf_file));

    if (bytes_read != sizeof(s_plf_file))
    {
        plf_close(fileIdx);
        return PLF_E_IO;
    }

    /* Fill unused bytes */
    if (fileEntry->hdr.dwHdrSize < sizeof(s_plf_file))
    {
        u8* p_start = ((u8*) (&fileEntry->hdr)) + fileEntry->hdr.dwHdrSize;
        u32 fill_size = sizeof(s_plf_file) - fileEntry->hdr.dwHdrSize;
        memset(p_start, 0, fill_size);
    }

    retval = plf_int_read_entries(fileIdx);
    if (retval < 0)
        return retval;

    return fileIdx;
}

/*
 * Read one section
 */
static int plf_int_read(int fileIdx, void* dst, u32 offset, u32 len)
{
    s_plf_file_entry* fileEntry;
    int bytes_read;

    PLF_VERIFY_IDX(fileIdx);

    if (dst == 0)
        return PLF_E_PARAM;

    fileEntry = &plf_files[fileIdx];

    // From memory?
    if (fileEntry->fildes == -1)
    {
        if (offset >= fileEntry->buffer_size)
            return PLF_E_PARAM;

        if (len > fileEntry->buffer_size - offset)
        {
            bytes_read = fileEntry->buffer_size - offset;
        }
        else
        {
            bytes_read = len;
        }

        memcpy(dst, (u8*) fileEntry->buffer + offset, bytes_read);
    }
    else
    {
        lseek(fileEntry->fildes, offset, SEEK_SET);
        bytes_read = read(fileEntry->fildes, dst, len);
    }

    return bytes_read;
}

/*
 * Write (to file or in memory)
 */
static int plf_int_write(int fileIdx, const void* src, u32 offset, u32 len)
{
    s_plf_file_entry* fileEntry;
    int bytes_written;

    PLF_VERIFY_IDX(fileIdx);

    if (src == 0)
        return PLF_E_PARAM;

    fileEntry = &plf_files[fileIdx];

    if (fileEntry->fildes == -1)
    {
        if (offset >= fileEntry->buffer_size)
            return PLF_E_PARAM;

        if (len > fileEntry->buffer_size - offset)
        {
            bytes_written = fileEntry->buffer_size - offset;
        }
        else
        {
            bytes_written = len;
        }

        memcpy((u8*) fileEntry->buffer + offset, src,  bytes_written);
    }
    else
    {
        lseek(fileEntry->fildes, offset, SEEK_SET);
        bytes_written = write(fileEntry->fildes, src, len);
    }

    return bytes_written;
}


/*
 * Add a section at the end of the chained list
 */
static int plf_int_add_entry(int fileIdx, const s_plf_section* section, u32 offset)
{
    int entryIdx;
    s_plf_file_entry* fileEntry;
    s_plf_section_entry* sectionEntry;
    PLF_VERIFY_IDX(fileIdx);

    if (section == 0)
        return PLF_E_PARAM;

    fileEntry = &plf_files[fileIdx];

    sectionEntry = (s_plf_section_entry*) malloc(sizeof(s_plf_section_entry));
    if (sectionEntry == 0)
        return PLF_E_MEM;

    sectionEntry->hdr.dwSectionType = section->dwSectionType;
    sectionEntry->hdr.dwSectionSize = section->dwSectionSize;
    sectionEntry->hdr.dwCRC32 = section->dwCRC32;
    sectionEntry->hdr.dwLoadAddr = section->dwLoadAddr;
    sectionEntry->hdr.dwUncomprSize = section->dwUncomprSize;

    sectionEntry->next = 0;
    sectionEntry->offset = offset;

    entryIdx = 0;
    /* Add to entries list */
    if (fileEntry->entries == 0)
    {
        fileEntry->entries = sectionEntry;
    }
    else
    {
        s_plf_section_entry* curEntry = fileEntry->entries;

        entryIdx = 1;

        // Find end of the list
        while (curEntry->next != 0)
        {
            curEntry = curEntry->next;
            ++entryIdx;
        }
        curEntry->next = sectionEntry;
    }

    ++fileEntry->num_entries;

    return entryIdx;
}
