This player is part of the jiku project: http://www.jiku.org/

For a research paper about this player: http://www.comp.nus.edu.sg/~ooiwt/papers/pcm12-zoomable.pdf
For a more detailed description, please refer to my master thesis: http://www.roman10.net/src/thesis.pdf

I've stopped the development for this player. The master branch is the one that is stable and ready to use, while other branches contain code for experiments purposes only.

How to use the app?
1. connect your phone to a computer, "adb install jiku-player.apk" to install the app to your phone.

How to use the app?
1. Create a directory "/sdcard/multitest" and put all videos there.
2. Start the app, click a video, and a menu will pop up. Choose "Dump dependency" to generate the dependency files. 
Use "adb logcat -v time" to see how the dependency file generation goes. It might take some time. 
3. After dependency file generation is done, click the video again. Choose "View with Andzop" to watch the video. 
4. Switch between "Full Mode" and "Auto Mode".
4.a Full mode demonstrates selective decoding; auto mode is more close to real usage. 
4.b Press menu key, select "Change Mode", then select the mode you want.
4.c When you're at "Full Mode", you can adjust the ROI size by the "Update ROI size" menu. 

How to build the app?
1. At jiku-player/jni/ffmpeg folder, update NDK variable path, and then execute the following command,
./build_android.sh
2. At jiku-player/jni folder, execute the following command,
ndk-build
3. At eclipse, build the project.

The desktop version
There's also an equivalent desktop version available under andzop/jni/andzop_desktop/ folder, it's for debugging purpose. Note that the code might be obsolete compared with the code for android. 


TODOs:
1. migrate to latest ffmpeg
2. improve the zoomable interface
  
