gst-switcher
============

testing gstreamer for switching and mixing streams.

edit uris[] at switcher1.c:283 so it points to some medias.

1 , 2 , 3 [return] should switch between them. (couldn't make setvbuf work with the io channel)

source "setenv" and do a "make graphs" after running to dump the pipeline.

not working yet:
  * pull based model
  * smooth transitions using interpolation controls
  * switching that doesn't break

to be done:
  * fix switching
  * add a real transition
  * add another channel so it can be remote controlled
