#!/bin/bash
# Script outline to install and build kernel.
# Author: Siddhant Jajoo.

# Stops the execution of a script if a command or pipeline has an error
set -e
# Treat unset variables as an error when substituting
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-none-linux-gnu-

# If outdir specified, use specified dir, else use /tmp/aeld
if [ $# -lt 1 ]
then
	echo "Using default directory ${OUTDIR} for output"
else
	OUTDIR=$1
	echo "Using passed directory ${OUTDIR} for output"
fi

# Create output dir if it doesn't exist
mkdir -p ${OUTDIR}

# Enter outdir and look for linux kernel source tree directory, git clone if it doesn't exist
cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
	echo "CLONING GIT LINUX STABLE VERSION ${KERNEL_VERSION} IN ${OUTDIR}"
	git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi
# Look for linux kernel boot image, build kernel image manually if it doesn't exist
if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
	cd linux-stable
	echo "Checking out version ${KERNEL_VERSION}"
	git checkout ${KERNEL_VERSION}

	# Deep clean kernel source tree: Remove all generated files + config + various backup files
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper 
    
	# Create a architecture-specific Linux kernel .config file to configure “virtual” arm dev board simulated in QEMU
	# Uses config option specified in defconfig or default configs specified in Kconfig
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig

	# Build the kernel image for booting with QEMU
	make -j4 ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} all

	# Build necessary kernel modules
	# make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} modules

	#Build kernel devicetree
	make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi

echo "Adding the Image in outdir"
# Copy recursively (-r) all image files to outdir
cp -r  ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}

echo "Creating the staging directory for the root filesystem"
cd "$OUTDIR"
# Remove exisiting root filesystem so that we can manually recreate one
if [ -d "${OUTDIR}/rootfs" ]
then
	echo "Deleting rootfs directory at ${OUTDIR}/rootfs and starting over"
	sudo rm  -rf ${OUTDIR}/rootfs
fi

# Create root file system folder tree with base directories
mkdir "${OUTDIR}/rootfs" && cd "${OUTDIR}/rootfs"
# mkdir -p: make parent directories if they do not exist 
mkdir -p bin dev etc home lib lib64 proc sbin sys tmp usr var
mkdir -p usr/bin usr/lib usr/sbin
mkdir -p var/log

# BusyBox combines tiny versions of many common UNIX utilities into a single small executable
# If busybox directory does not exist, setup busybox
cd "${OUTDIR}"
if [ ! -d "${OUTDIR}/busybox" ]
then
	git clone git://busybox.net/busybox.git
	cd busybox
	git checkout ${BUSYBOX_VERSION}
	# Deep clean the busybox source tree by deleting all files created by configuring or building 
	make distclean
	# Configure busybox using default config for busybox
	make defconfig
else
    cd busybox
fi

# Make (compile) and install busybox in rootfs dir
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make CONFIG_PREFIX=${OUTDIR}/rootfs ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} install

# Add library dependencies to rootfs
echo "Library dependencies"
TOOLCHAIN_SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)

cd "${OUTDIR}/rootfs"
# Find the program interpreter and copy into the lib dir
# Reads grep result from standard input and extract out filename of interpreter using sed 's/pattern/replacement/'
# sed substitues (s) the entire output ([any char any times] program interpreter: (interpreter filename: saved for reference) [any char any times]) with (interpreter filename: saved for reference)  
PROG_INTERPRETER=$(${CROSS_COMPILE}readelf -a bin/busybox | grep "program interpreter" | sed 's/.*program interpreter: \(.*\)].*/\1/')
cp ${TOOLCHAIN_SYSROOT}/${PROG_INTERPRETER} ${OUTDIR}/rootfs/lib

# Find the shared libraries and copy into lib64 dir
# Reads grep result from standard input and extract out filenames of interpreter using sed
# sed substitues (s) the entire output ([any char any times] Shared library: [(library filename: saved for reference)] [any char any times]) with (library filename: saved for reference)  
SHARED_LIBS=$(${CROSS_COMPILE}readelf -a bin/busybox | grep "Shared library" | sed 's/.*Shared library: \[\(.*\)\].*/\1/')

# Read list of libraries line by line, IFS is used to set field separator as empty string and backslash escaping is disabled (-r)
# Passes list of libraries to standard input of while loop command (<<<)
# Reference: https://bash.cyberciti.biz/guide/While_loop#:~:text=The%20while%20loop%20syntax,-The%20syntax%20is&text=IFS%20is%20used%20to%20set,loop%20for%20reading%20text%20files.
while IFS= read -r shared_lib; do
    	cp ${TOOLCHAIN_SYSROOT}/lib64/${shared_lib} ${OUTDIR}/rootfs/lib64
done <<< "$SHARED_LIBS"

# Make device nodes
# Null device (virtual black hole that discards everything written to it) with mode 666 (-m) so all users can read and write but cannot execute, major 1 minor 3
# Console device (system console ) with mode 600 (-m) so only user can read and write, and all cannot execute, major 5 minor 1
# Type (c): special file is a character-oriented device
sudo mknod -m 666 dev/null c 1 3
sudo mknod -m 600 dev/console c 5 1

# Clean and build the writer utility
cd "${FINDER_APP_DIR}"
echo "Removing the old writer utility and compiling as a ARM64 application"
make clean
make CROSS_COMPILE=${CROSS_COMPILE}


# Find all finder related scripts and executables and copy to the /home directory of rootfs
find . -name "*finder*" | xargs cp -t ${OUTDIR}/rootfs/home
find . -name "*writer*" | xargs cp -t ${OUTDIR}/rootfs/home
cp -r conf/ -t ${OUTDIR}/rootfs/home
cp autorun-qemu.sh -t ${OUTDIR}/rootfs/home

# Chown recursively on the root directory to make all contents (*) owned by user root of group root
# User account doesn’t exist on the system to be created
cd "${OUTDIR}/rootfs"
sudo chown -R root:root *

# Find all files in rootfs and convert into cpio format before zipping into initramfs.cpio.gz
cd "${OUTDIR}/rootfs"
find . | cpio -H newc -ov --owner root:root > ${OUTDIR}/initramfs.cpio
cd ${OUTDIR}
gzip -f initramfs.cpio
