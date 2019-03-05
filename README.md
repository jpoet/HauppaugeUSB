
# MythTV Hauppauge HD-PVR2 / Colossus2 support

A wrapper around the Hauppauge HDPVR2/Colossus2 Linux "[driver](http://www.hauppauge.com/site/support/linux.html?#tabs-3)"

This is currently beta quality, based on an alpha quality driver.  That
being said, It is working for me.

This can be used at the command-line as well as an "External Recorder" for MythTV.

----
## News
The issue with interlaced fields being in the wrong order has been fixed.

AC3 audio codec now works via HDMI.

Note: Even when using HDMI for audio, the models with a S/PDIF input are required if you want AC3 surround sound.

----
## Installing

### Install dependancies
#### Fedora:
```
sudo dnf install make gcc gcc-c++ kernel-devel libstdc++-devel boost-devel libusbx-devel
```

#### Ubuntu
```
sudo apt-get install libboost-log-dev libboost-program-options-dev libusb-1.0-0-dev build-essential
```

#### MythTV
If you want to use this with MythTV, you will need
[fixes/29](https://github.com/MythTV/mythtv/tree/fixes/29) from 2018-03-01 or [master](https://github.com/MythTV/mythtv/tree/master) from 2018-02-25.  MythTV [fixes/30](https://github.com/MythTV/mythtv/tree/fixes/30) is recommended for the best experience.

### Grab the prepared "driver" from Hauppauge into the submodule hauppauge_hdpvr2
```
git submodule update --init
```

### Build it
The installation directory tree is currently hard-coded to be /opt/Hauppauge.
```
make
sudo make install
```
----
## Using it

### Permissions

hauppauge2 needs permission to use the device file(s).  Create a udev rules
file.  Something like:

```
nano /etc/udev/rules.d/99-Hauppauge.rules
```
And add appropriate rules:
```
# Device 1
SUBSYSTEMS=="usb",ATTRS{idVendor}=="2040",ATTR{serial}=="E505-00-00AF4321",MODE="0660",GROUP="video",SYMLINK+="hdpvr2_1",TAG+="systemd",RUN="/bin/sh -c 'echo -1 > /sys$devpath/power/autosuspend'"

# Device 2
SUBSYSTEMS=="usb",ATTRS{idVendor}=="2040",ATTR{serial}=="E585-00-00AF1234",MODE="0660",GROUP="video",SYMLINK+="colossus2-1",TAG+="systemd",RUN="/bin/sh -c 'echo -1 > /sys$devpath/power/autosuspend'"
```
At the least, you will need to adjust the serial numer(s) to match your
device(s).  Any user which wants to run hauppauge2 needs to be a member of
the GROUP specified.

### Running it

You can get the optional arguments with
```
/usr/sbin/hauppauge2 --help
```
A lot of the options don't work unless just the right combination is
selected.  The program does not currently protect you from choosing bad
combinations, because in many cases they *should* work, but have not been
implemented yet.

----
### Command line examples

#### List detected devices
```
$ /usr/sbin/hauppauge2 --list
[Bus: 5 Port: 1]  2040:0xe585 E585-00-00AF4321 Colossus 2
Number of possible configurations: 1  Device Class: 0  VendorID: 8256  ProductID: 58757
Manufacturer: Hauppauge
Serial: E585-00-00AF4321
Interfaces: 1 ||| Number of alternate settings: 1 | Interface Number: 0 | Number of endpoints: 6 | Descriptor Type: 5 | EP Address: 129 | Descriptor Type: 5 | EP Address: 132 | Descriptor Type: 5 | EP Address: 136 | Descriptor Type: 5 | EP Address: 1 | Descriptor Type: 5 | EP Address: 2 | Descriptor Type: 5 | EP Address: 134 |


[Bus: 3 Port: 4]  2040:0xe505 E505-00-00AF1234 HD PVR 2 Gaming Edition Plus w/SPDIF w/MIC
Number of possible configurations: 1  Device Class: 0  VendorID: 8256  ProductID: 58629
Manufacturer: Hauppauge
Serial: E505-00-00AF1234
Interfaces: 1 ||| Number of alternate settings: 1 | Interface Number: 0 | Number of endpoints: 6 | Descriptor Type: 5 | EP Address: 129 | Descriptor Type: 5 | EP Address: 132 | Descriptor Type: 5 | EP Address: 136 | Descriptor Type: 5 | EP Address: 1 | Descriptor Type: 5 | EP Address: 2 | Descriptor Type: 5 | EP Address: 134 |
```
#### Capture from HDMI video and S/PDIF audio with AC-3 codec
```
/usr/sbin/hauppauge2 -s E585-00-00AF4321 -a 1 -d 2 -o /tmp/test.ts
```
#### Capture from Component video and RCA AAC audio
```
/usr/sbin/hauppauge2 -s E505-00-00AF1234  -i 1 -a 1 -o /tmp/test.ts
```
#### Use a configuration file
The configuration file is just a list of option=value statements which
mimics using the longform on the command line.  A `sample.conf` is included
which you can copy and modify.
```
cp sample.conf Colossus1.conf
nano Colossus1.conf
/usr/sbin/hauppauge2 -c Colossus1.conf
```

----
## Using with MythTV

#### Configuration file
First step is to create an appropriate configuration file
```
cd /etc/Hauppauge
cp sample.conf hdpvr2-1.conf
nano hdpvr2-1.conf
```
At the minimum, you need to set the serial to the correct value for your
device.  You can use the list option to see what devices are detected:
```
/usr/sbin/hauppauge2 --list
```

#### Configure MythTV

Stop mythbackend, and run mythtv-setup to configure the new recorder:
```
systemctl stop mythbackend
mythtv-setup
```

##### 2. Capture Cards
1. Select "New Capture Card"
2. For the "card type", choose "External (blackbox) recorder"
3. For the "file path", use the full path of the hauppauge2 app and
give it the location of your configuration file.  Something like:
```
/usr/sbin/hauppauge2 -c /etc/Hauppauge/hdpvr2-1.conf
```
4. Set the "Tuning timeout" to at least 15000. The HD-PVR2 / Colossus2 can take over 5 seconds just to get ready to record.  Combine that with the time it takes your STB (Set Top Box) to change channels, and produce a 'steady' output, and it can easily take 15 seconds to "tune" a channel.

##### 3. Recording Profiles
A profile is not used.

##### 4. Video sources
If you don't already have a source for guide information setup, do so now.

##### 5. Input connections
1. Select the External Recorder you created under "2. Capture Cards".
2. Set the "Input name" to "MPEG2TS"
3. Set the "Display name" to whatever you like.  E.g. "Colossus2-1"
4. Select the appropriate video source
5. Set the External channel change command appropriately. This is whatever
script you have setup to control your STB (set top box).
6. Do **not** "Preset tuner to channel".
7. There is no reason to "Scan for channels".  These will be retrieved from
your video guide provider.
8. If you have not done so already, then "Fetch channels from listing
source."
9. If you wish, set the "Starting channel"

###### 10. Interactions between inputs
1. If you want to enable "multirec", then set Max recordings to 3
2. Check "Schedule as group" to enable the faster, optimized scheduler routines.

The rest of the options are optional, set them as you wish.

#### Changing channels

Myth's "External Recorder" is multirec capable, meaning that you can have
overlapping recordings.  If a new recording is on the same channel as
current recording, mythbackend will use the same instance of the hauppauge2
app to retrieve the data.  However, the hauppauge2 app does not currently
support changing channels, so you must use MythTV's "External Channel
Change" script capbilities.

Unfortunately, MythTV's "External Channel Change" scrip mechanism is **not**
multirec aware.  This means that mythbackend will call it, even if
hauppauge2 is already recording on the correct channel.  If possible, you
should craft your channel change script such that it *just* returns if it is
able to detect that it is already on the correct channel.

#### Logging

When mythbackend invokes this app, it will pass loglevel and logpath
arguments.  This app will pay attention to both, but the loglevel can be
overridden in the config file using the override-loglevel option. A good
loglevel for mythbackend is INFO, but that is quite verbose when used with
the Hauppauge "driver".  I suggest setting override-loglevel to NOTICE which
results in enough status information to verify that it is working.

##### Log rotation

You will probably want to add log rotation. The log file location will be
the same as the rest of the mythtv logs.  The log filenames will look like hauppauge2-<serial#>.log


#### Run it
If everything is configured correctly, you should now be able to restart
mythbackend and have it use this input.

----
## Troubleshooting

After a fresh reboot, the Colossus2 will on occasion drop off the USB bus, the
first time it is used.  When hauppauge2 is run, the first thing it sends
out to the log is the Bus and Port of the device.  This is allows you to
reset the USB bus for that device to get it back.  For example, if the first
line in the log is:
```
2018-02-06 15:39:50.822985 C [main] hauppauge2.cpp:266 (main) - Initializing [Bus: 5, Port: 4] E585-00-FFFF4321
```
You can usually get the device back by doing:
```
export BUS=5
echo "0"    | sudo dd of="/sys/bus/usb/devices/usb${BUS}/power/autosuspend_delay_ms"
echo "auto" | sudo dd of="/sys/bus/usb/devices/usb${BUS}/power/control"
echo "on"   | sudo dd of="/sys/bus/usb/devices/usb${BUS}/power/control"
```
After that, the Colossus2 seems to work reliably until the next reboot.
