# Sparse screen decoder fixture

`sparse-screen.h264` is a generated 64 × 64 solid-red Annex B H.264 stream:
one IDR followed by two P-frames, no B-frames, with access-unit delimiters.
It contains no captured phone content. Generated with FFmpeg 7.1/libx264:

```sh
ffmpeg -f lavfi -i 'color=c=red:s=64x64:r=30' -frames:v 3 \
  -c:v libx264 -profile:v baseline -preset ultrafast -tune zerolatency \
  -x264-params 'aud=1:keyint=60:scenecut=0' -f h264 sparse-screen.h264
```

FFmpeg is needed only to regenerate the fixture, not to build/run the tests.
The Windows decoder test submits each access unit separately and requires its
picture immediately, without submitting a future frame or draining at EOS.
This models a phone screen that stops producing frames after a UI change.
The test failed at frame 0 before low-latency mode was enabled, and passed for
all three frames after the fix on the development Windows host.
