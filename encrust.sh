ffmpeg -f video4linux2 -r 20 -s 640x480  -i /dev/video0 \
-f alsa -ac 1 -i hw:2,0 \
-f avi -vcodec rawvideo -pix_fmt bgr24 pipe:1 2>/dev/null \
| ./bin/encrust ./settings.xml pipe:0 pipe:1 \
| ffmpeg -i pipe:0 \
-vcodec libx264 -pix_fmt yuv420p -preset faster -tune zerolatency -threads 0 -b 512k -bt 512k -f flv \
-acodec libfaac -ac 1 -ab 64k -ar 22050 \
pipe:1 | ffplay pipe:0;
