# rigol-viewer
Rigol Viewer is a program to visualize your Rigol® DS1000(D) series oscilloscope from your Linux desktop. 
You can capture the screen and create PNG graphics from:

-    Channel1
-    Channel2
-    FFT
-    Logic Analyser 16bit


The connection between Rigol Digital Oscilloscope and the Linux system use the USB Test and Measurement Class (USBTMC) Protocol. If a USB-measuring instrument with USBTMC support is connected to a Linux system, the kernel module will create an entry /dev/usbtmc[0..9].

You should get an entry in the Kernel-Log.

xx@xxxxx:~$ dmesg

[ 8311.126918] usb 1-1.5: new full-speed USB device number 6 using ehci-pci

[ 8311.221803] usb 1-1.5: New USB device found, idVendor=1ab1, idProduct=0588

[ 8311.221808] usb 1-1.5: New USB device strings: Mfr=1, Product=2, SerialNumber=3

[ 8311.221811] usb 1-1.5: Product: DS1000 SERIES

[ 8311.221813] usb 1-1.5: Manufacturer: Rigol Technologies

[ 8311.221815] usb 1-1.5: SerialNumber: DS1EC132300216

The idVEndor and idProduct is nessary for the access rights. Create a file /etc/udev/rules.d/97-rigol-ds1052.rules with the following content:

xx@xxxxx:~$ cd /etc/udev/rules.d/

xx@xxxxx:/etc/udev/rules.d$ sudo nano 97-rigol-ds1052.rules


//USBTMC instruments

//Rigol Technologies DS1502D series

SUBSYSTEMS=="usb", ACTION=="add", ATTRS{idVendor}=="1ab1", ATTRS{idProduct}=="0588", GROUP="xx", MODE="0660"

//Devices

KERNEL=="usbtmc/*", MODE="0660", GROUP="xx"

KERNEL=="usbtmc[0-9]*", MODE="0660", GROUP="xx"


Replace the entries "xx" with the local group for the access to the USBTMC measuring instrument. Disconnect and connect the oscilloscope again.

INSTALL
The program is written written in C and uses GTK-2.

References:

[1] http://www.cibomahto.com/2010/04/controlling-a-rigol-oscilloscope-using-linux-and-python/

[2] http://www.pittnerovi.com/jiri/hobby/electronics/rigol/rigol.c
