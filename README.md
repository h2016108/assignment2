2nd assignment
This is the block driver to create disk of 512kb on RAM. This disk is split into three primary partition and three logical partition of different file system. In our module we use the different "MBR table" for both MBR and BR, it contains the time at which disk has been created and original physical drive of the disk. The address of original physical drive,sec,MIN & hour are from 220, 221, 222, 223 respectively. 

Steps to be followed to execute the module.
1) download the compleate module.
2) write the "make" command on the terminal, it will generate object files.
3) insert the objects into the kernel using insmod myramdisk.ko
4) To see the disk partision use "fdisk -l".
5) To justify the logical and primary partitions type "parted" command then press Enter and then type "print all"this will display major partitions then type i to list all the disk created newly .press "q" and enter to come out of it.
6) to write to the disk use this command "cat > /dev/mydisk1...7". It is better if we work as super user (except extended partition).
7) To read the the memory type xxd /dev/mydisk1 | less.
8) to remove the kernel module use this command rmmod myramdisk.ko
9) To mount these new disk follow these steps:
	(i)   mkdir /mynewdisk
	(ii)  mount /dev/mydisk1 /mynewdisk
	(iii) df -H
   run these commands by using root acssess.
