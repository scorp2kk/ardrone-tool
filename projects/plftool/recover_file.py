#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import re
import glob

'''
binary stuff
b'm\x81' -> lib (very few)
b'\xa4\x81' -> txt files
b'\xedA' -> directory
b'\xed\x81' -> exe
b'\xff\xa1' -> link

b'\x6d\x81' -> lib (very few)   555
b'\xa4\x81' -> txt files        644
b'\xed\x41' -> directory        755
b'\xed\x81' -> exe              775
b'\xff\xa1' -> symbolic links   (path in the file)
'''

for fn in sorted(glob.glob('a/*')):
    if fn == 'a/000_0x0b_0_volume_config' or fn == 'a/001_0x03_0_main_boot.plf':
        continue
    with open(fn, 'rb') as f:
        a, b, c = re.findall(br'([^\x00]*)\x00([^\x00]*)\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00(.*)', f.read(), re.DOTALL)[0]
        print(a, b, len(c))
        if len(c) == 0:
            # Directory
            os.mkdir('fs/' + a.decode("utf-8"))
        elif b == b'\xff\xa1':
            os.symlink(c.decode("utf-8").rstrip('\0'), 'fs/' + a.decode("utf-8").rstrip('\0'))
        else:
            with open('fs/' + a.decode("utf-8"), 'wb') as nf:
                nf.write(c)
            if b == b'\xed\x81':
                os.chmod('fs/' + a.decode("utf-8"), 775)
