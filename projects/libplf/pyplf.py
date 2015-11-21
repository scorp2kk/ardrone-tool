#!/usr/bin/env python
# -*- coding: utf-8 -*-

import ctypes

libplf = ctypes.CDLL("./libplf.so")


class Version(ctypes.Structure):
    """ creates a struct to match s_plf_version_info_tag """

    _fields_ = [('major', ctypes.c_ubyte),
                ('minor', ctypes.c_ubyte),
                ('bugfix', ctypes.c_ubyte)]

    @classmethod
    def get(self):
        version = libplf.plf_lib_get_version
        version.argtypes = None
        version.restype = ctypes.POINTER(Version)
        a = version()
        return a[0]


if __name__ == '__main__':
    v = Version.get()
    print(v.major, v.minor, v.bugfix)
