# Introduction #

This page describes how to compile the projects.

# libplf #

Is a library containing functions to read / write .plf files.

## Requirements ##

  * zlib-1.2.3 (http://www.zlib.net)

## Building ##

Edit Makefile and call

```
make
```

# plf\_inst\_extract #

Is a tool to extract the installer from a given ardrone\_update.plf

## Requirements ##

  * libplf-0.1.0

## Building ##

Edit Makefile and call

```
make
```

# usb\_flash #

Is a tool to download a firmware via USB to the drone.

## Requirements ##

  * libplf-0.1.0
  * libusb-win32-1.2.2.0 (http://libusb.org/wiki/libusb-win32)

## Building ##

Edit Makefile and call

```
make
```