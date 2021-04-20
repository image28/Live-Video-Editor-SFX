all: vfx2 
vfx2:vfx2.c
	gcc vfx2.c -o vfx2 `sdl2-config --libs --cflags` -DWIDTH=960 -DHEIGHT=720
clean:
	rm -f ./*~ && rm -f ./{play,view,tv,record,edit,edit-multi}
