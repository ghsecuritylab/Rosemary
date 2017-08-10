//------------------------------------------------------------------------------
//
//	Revision History and 
//              Simple Library Package Architecture & Build Order
//


//------------------------------------------------------------------------------
 -. Revision History ( libnxdvr.so )
   2014.12.08 ( v1.0.13 )
     Mp4Muxer library improvement. ( Support wmp12, ubuntu default player )
     Support Mp4 Subtitle. ( Userdata field )

   2014.08.25 ( v1.0.12 )
     Support MP4 Container. ( bug fixed )
     Bug fixed nxmp4encsol. ( Start / Stop Bug )

   2014.07.14 ( v1.0.11 )
     Support Real Time Protocol(RTP).

   2014.06.16 ( v1.0.10 )
     Support HDMI output.
     Add build number.
   
   2014.05.23
     Add GUI Solution.  

   2014.04.21 
     Stable Filter solution. ( Various Bug Fixed )
	   Broken data bug fixed (Text Overlay / TS Stream)
     Add Micom Senario.

   2014.02.18
     Modify Writing Time Handling. ( bug fixed )
     Add Rate control paramter handling. ( bug fixed )
     Add Message Queue. (apps)
     Modify FileWriter. ( bug fixed - thread base )
     
   2014.02.11
     Add simple MP4 encoding library and test application.
     Suuport Motion Detection.
     Support MP3 Audio Codec.

   2014.01.17
     Support TS Container.
     Support HLS Component.
     Support 3D Image effect. ( 3D Scaler )
	 
   2013.12.02
     First Release.


//------------------------------------------------------------------------------
-. Directory Architecture

Solution
 |
 +-+-- apps    --+-- nxdvrmonitor : simple wireless network monitor program ( Support SoftAP & Station Mode )
   |             |
   |             +-- nxdvrsol     : simple blackbox encoding application
   |             |
   |             +-- nxguisol     : simple GUI based blackbox / player application
   |             |
   |             +-- nxhlssol     : simple HLS test application
   |             |
   |             +-- nxmp3encsol  : simple MP3 encoding test application
   |             |
   |             +-- nxmp4encsol  : simple MP4 encoding test application
   |             |
   |             +-- nxrtpsol     : simple RTP test application
   |             |
   |             +-- nxtranscodingsol : simple Trascoding test application ( Filter base )
   |
   +-- bin                        : build result & resource files
   |
   +-- build                      : library & application build script
   |
   +-- include                    : include files
   |
   +-- lib                        : private static library, library build result
   |
   +-- src     --+-- libnxdvr     : blackbox encoding manager library
                 |
                 +-- libnxfilters : base filter components
                 |
                 +-- libnxhls     : HLS manager library
                 |
                 +-- libnxmp3manager : simple MP3 encoding manager library
                 |
                 +-- libnxmp4manager : simple MP4 encoding manager library
                 |
                 +-- libnxrtp     : simple HLS manager library
                 |
                 +-- libnxtranscoding : simple Transcoding manager library
                 |
                 +-- live555      : Base library for RTP Filter


//------------------------------------------------------------------------------
-. Prefix definition. ( in build.env )

 [ARCHDIR]    : Architecture Directory of Android Source.
 [ROOTFS]     : Your rootfs directory.

//------------------------------------------------------------------------------
-. Build Prepare. 

 Step 1. Build system library.
      [ARCHDIR]/library/src/libion
      [ARCHDIR]/library/src/libnxmalloc
      [ARCHDIR]/library/src/libnxv4l2
      [ARCHDIR]/library/src/libnxvpu
      [ARCHDIR]/library/src/libnxgraphictools
      [ARCHDIR]/library/src/libnxnmeaparser
      [ARCHDIR]/library/src/libnxadc
      [ARCHDIR]/library/src/libnxgpio

      $ cd [ARCHDIR]/library/src/[Each library directory]
      $ make
      $ make install

 Step 2. Build driver module. ( after building kernel )

      $ cd [ARCHDIR]/modules/coda960
      $ make ARCH=arm


//------------------------------------------------------------------------------
-. Build Application. (Blackbox Example)

 1. Default Mathod. 
 Step 1. Build Base Filter.

      $ cd [ARCHDIR]/Solution/BlackBoxSolution/src/libfilters
      $ make

 Step 2. Build each manager libraries. 
      [ARCHDIR]/Solution/BlackBoxSolution/src/libnxdvr
      [ARCHDIR]/Solution/BlackBoxSolution/src/libnxhls
      [ARCHDIR]/Solution/BlackBoxSolution/src/libnxmp4manager
      [ARCHDIR]/Solution/BlackBoxSolution/src/libnxrtp
      [ARCHDIR]/Solution/BlackBoxSolution/src/libnxtranscoding

      $ cd [ARCHDIR]/Solution/BlackBoxSolution/src/[Each library directory]
      $ make
      $ make install

 Step 3. Build each application
      [ARCHDIR]/Solution/BlackBoxSolution/apps/nxdvrsol
      [ARCHDIR]/Solution/BlackBoxSolution/apps/nxguisol  <-- depend on libnxdvr.so / libnxmovieplayer.so
      [ARCHDIR]/Solution/BlackBoxSolution/apps/nxhlssol
      [ARCHDIR]/Solution/BlackBoxSolution/apps/nxmp4encsol
      [ARCHDIR]/Solution/BlackBoxSolution/apps/nxrtpsol
      [ARCHDIR]/Solution/BlackBoxSolution/apps/nxtranscodingsol

      $ cd [ARCHDIR]/Solution/BlackBoxSolution/apps/[Each application directory]
      $ make
      $ make install

 2. Another Mathod. (using script)

      $ cd [ARCHDIR]/Solution/BlackBoxSoltion/build/
      
      $ ./clean-blackbox.sh;./build-blackbox.sh
      $ ./clean-gui.sh;./build-gui.sh
      $ ./clean-hls.sh;./build-hls.sh
      $ ./clean-mp4.sh;./build-mp4.sh
      $ ./clean-rtp.sh;./build-rtp.sh
      $ ./clean-transcoding.sh;./build-transcoding.sh


//------------------------------------------------------------------------------
-. Run Sequence.

 Step 1. Prepare root file system ( [ARCHDIR]/fs/buildroot/ )
	 buildroot configuration : br.2013.11.cortex_a9_glibc_gst_sdl_dfb_wifi.config
	  
 Step 2. Copy system shared libraries.

      $ cp -a [ARCHDIR]/library/lib/*.so [ROOTDIR]/usr/lib
      $ cp -a [ARCHDIR]/library/lib/ratecontrol/libnxvidrc.so [ROOTDIR]/usr/lib

 Step 3. Copy solution shared libraries.

      $ cp -a [ARCHDIR]/Solution/BlackBoxSoltion/lib/*.so [ROOTDIR]/usr/lib
 
 Step 4. Copy Encoder module driver.

      $ cp -a [ARCHDIR]/modules/code960/nx_vpu.ko [ROOTDIR]/root

 Step 5. Copy Application & Resouce file to root file system.

      $ cp -a [ARCHDIR]/Solution/BlackBoxSoltion/bin * [ROOTDIR]/root
 
 Step 6. System Booting.

 Step 7. Load Encoder Driver. ( at Target board )

      $ insmod /root/nx_vpu.ko

 Step 8. Run applications. ( at Target board )

      $ /root/[Application Name]

//------------------------------------------------------------------------------
-. Etc.

 1. Support HLS.

 Step 1. Make Temperary Directory. (at Target Board )

      $ mkdir /tmp/www
      $ ln -s /tmp/www /www

 Step 2. Running http demon. (at Target Board )

      $ httpd

 Step 3. Create "index.html" for http demon test. (optional)

      $ vi /www/index.html

      <html><body><head><title>Test Web Page</title></head>This page is http demon test page.</body></html>

 Step 4. Running Application with HLS. ( nxdvrsol, nxhlssol, nxguisol )

      $ /root/nxdvrsol -n 1

 Step 5. Connection 
      Connection Address : http://[Target Board IP]/test.m3u8

      If it is not connected, check your http deomon. ( Connect index.html )


 2. Support RTP.

 Step 1. Running Application with RTP. ( nxdvrsol, nxrtpsol ) 

      $ /root/nxrtpsol -n 2

 Step 2. Connection ( Identify Terminal Message )
      Connection Address : rtsp://[Target Board IP]/video0


 3. UI Base Blackbox Solution.

 Step 1. Need more libraries for Blackbox GUI Solution.

      $ cp -a [ARCHDIR]/library/lib/libnxmovplayer.so [ROOTDIR]/usr/lib
      $ cp -a [ARCHDIR]/library/lib/gstreamer-0.10/libgst*.so [ROOTDIR]/usr/lib/gstreamer-0.10/
      
      If it is not exist, build nxmovieplayer.
      $ cd [ARCHDIR]/library/src/libnxmovideplayer
      $ make;make install

 Step 2. Running BlackBox GUI Solution.

      $ /root/nxguisol
