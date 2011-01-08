/*
 * crc32.c
 *
 * Copyright (c) 2011 scorp2kk, All rights reserved
 *
 * Description:
 *  Contains function to calculate crc32 hashes.
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
#ifndef CRC32_H_
#define CRC32_H_

#include "types.h"

void crc32_calc_dw(u32* pCrcAccum, u32* pVal);
void crc32_calc_buffer(u32* pCrcAccum, u32* num_crc,  const u8* buffer, u32 len);


#endif /* CRC32_H_ */
