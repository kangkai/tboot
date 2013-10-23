tboot
=====

tboot is derived from [droidboot][1], which is a user-space [fastboot][2]
protocol server implementation in Android build environment. tboot port that
to general Linux and GNU Autotools build system, and extend it with some extra
features.

tboot is currently using in Pre-OS runtime as fastboot server, communicating
with client side [prekit][3] to achieve image flashing and other maintenance
tasks.


How to build?
-------------

This project use GNU Autotools as build system, so it's quite simple to build
it like below:

    $ autoreconf -i (if you're using git tree source)
    $ ./configure CFLAGS="-m32 -O2"
    $ make

By default, autoconf will add "-g -O2" to CFLAGS, so it's a good idea to
overwrite it by the above to strip the output binary size.

For more advanced usage, please refer to INSTALL.


[1]: https://android-review.01.org/#/admin/projects/platform/bootable/droidboot
[2]: https://android.googlesource.com/platform/system/core/+/master/fastboot/
[3]: https://github.com/kangkai/prekit
