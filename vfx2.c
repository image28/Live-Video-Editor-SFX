/*
 *  V4L2 video capture example
 *
 *  This program can be used and distributed without restrictions.
 *
 *      This program is provided with the V4L2 API
 * see http://linuxtv.org/docs.php for more information
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <getopt.h>             /* getopt_long() */

#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <SDL.h>
#include <png.h>
#include <SDL2/SDL_image.h>

#include <linux/videodev2.h>

#define CLEAR(x) memset(&(x), 0, sizeof(x))

#ifndef V4L2_PIX_FMT_H264
#define V4L2_PIX_FMT_H264     v4l2_fourcc('H', '2', '6', '4') /* H264 with start codes */
#endif

enum io_method {
        IO_METHOD_READ,
        IO_METHOD_MMAP,
        IO_METHOD_USERPTR,
};

struct buffer {
        void   *start;
        size_t  length;
};

static char            *dev_name;
static enum io_method   io = IO_METHOD_MMAP;
static int              fd = -1;
struct buffer          *buffers;
static unsigned int     n_buffers;
static int              out_buf;
static int              force_format;
static int              frame_count = 200;
static int              frame_number = 0;
//static int WIDTH=0;
//static int HEIGHT=0;
static int first=1;

	int frame=0;
		int subtract=0;
	int capBackground=0;
	int haveBackground=0;
	int startRecord=0;

			char *map = NULL;
	char *background = NULL;
	char **videoArray;
	char **mapPointer;

#define WIDTH 960
#define HEIGHT 720
#define  VARIATION 34 // 36
#define FRAMES 10
#define CORRECTION 40 // 48
#define		p(x) printf("%s",x);
//static char 	*map;
#define OUTFILE "screenshot-"

SDL_Surface *screen;
SDL_Surface *offscreen;
SDL_Event event;

void errno_exit(const char *s)
{
        fprintf(stderr, "%s error %d, %s\n", s, errno, strerror(errno));
        exit(EXIT_FAILURE);
}

static int xioctl(int fh, int request, void *arg)
{
        int r;

        do {
                r = ioctl(fh, request, arg);
        } while (-1 == r && EINTR == errno);

        return r;
}

int record(char *map, char name[20])
{	       
	FILE *output;
	if ((output=fopen(name,"a")) == NULL)  printf("File open failed"); exit;
	fwrite ((unsigned char*) map, WIDTH * HEIGHT * 4, 1, output);
	fclose(output);
	return 0;
}

void screenshot(char *map, char name[20])
{
        FILE *screenshot;
        if ((screenshot=fopen(name,"w")) == NULL)  printf("File open failed"); exit;
        fwrite ((unsigned char*) map, WIDTH * HEIGHT * 4, 1, screenshot);
        fclose(screenshot);
}


/* Copies map to screen, then updates the screen */
void copytoscreen(char* tmap, int startRecord, int count) {
	      
	offscreen = SDL_CreateRGBSurfaceFrom((void *) tmap, WIDTH, HEIGHT, 24, WIDTH*3, 0x0000FF, 0x00FF00, 0xFF0000, 0x000000);

	if ( startRecord )
	{

   char temp[10];
		 char outfileCur[32768];
		sprintf(temp, "%d", count);
		strcpy(outfileCur,OUTFILE);
		strcat(outfileCur,temp);
		strcat(outfileCur,".png");
		if( png_save_surface(outfileCur, offscreen) < 0 ) exit(-1);
	}

	SDL_BlitSurface(offscreen, NULL, screen, NULL);
	SDL_UpdateRect(screen, 0, 0, 0, 0);
	SDL_FreeSurface(offscreen);
}


static int png_colortype_from_surface(SDL_Surface *surface)
{
	int colortype = PNG_COLOR_MASK_COLOR; /* grayscale not supported */

	if (surface->format->palette)
		colortype |= PNG_COLOR_MASK_PALETTE;
	else if (surface->format->Amask)
		colortype |= PNG_COLOR_MASK_ALPHA;

	return colortype;
}


void png_user_warn(png_structp ctx, png_const_charp str)
{
	fprintf(stderr, "libpng: warning: %s\n", str);
}


void png_user_error(png_structp ctx, png_const_charp str)
{
	fprintf(stderr, "libpng: error: %s\n", str);
}


int png_save_surface(char *filename, SDL_Surface *surf)
{
	FILE *fp;
	png_structp png_ptr;
	png_infop info_ptr;
	int i, colortype;
	png_bytep *row_pointers;

	/* Opening output file */
	fp = fopen(filename, "wb");
	if (fp == NULL) {
		perror("fopen error");
		return -1;
	}

	/* Initializing png structures and callbacks */
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
		NULL, png_user_error, png_user_warn);
	if (png_ptr == NULL) {
		printf("png_create_write_struct error!\n");
		return -1;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct(&png_ptr, (png_infopp)NULL);
		printf("png_create_info_struct error!\n");
		exit(-1);
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(fp);
		exit(-1);
	}

	png_init_io(png_ptr, fp);

	colortype = png_colortype_from_surface(surf);
	png_set_IHDR(png_ptr, info_ptr, surf->w, surf->h, 8, colortype,	PNG_INTERLACE_NONE,
		PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	/* Writing the image */
	png_write_info(png_ptr, info_ptr);
	png_set_packing(png_ptr);

	row_pointers = (png_bytep*) malloc(sizeof(png_bytep)*surf->h);
	for (i = 0; i < surf->h; i++)
		row_pointers[i] = (png_bytep)(Uint8 *)surf->pixels + i*surf->pitch;
	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, info_ptr);

	/* Cleaning out... */
	free(row_pointers);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);

	return 0;
}



void process_image(void *p, int size)
{
/*	printf("size: %d %d\n",size,WIDTH*HEIGHT*3);
	int up,down,left,right;
	int x,y,z,d;

	
	if ( first )
	{
		background=malloc(WIDTH*HEIGHT*3*FRAMES);

	videoArray=malloc(HEIGHT*FRAMES*sizeof(char*));
	mapPointer=malloc(HEIGHT*sizeof(char*));

	for(x=0; x<HEIGHT*FRAMES; x++)
{
	videoArray[x]=background+(x*WIDTH*3);


}
first=0;
printf("Allocated Pointers Buffers\n");
}

		for(x=0; x<HEIGHT*FRAMES; x++)
{
	if ( x < HEIGHT )
{
mapPointer[x]=p+(x*WIDTH*3);
}
}
	
if ( capBackground )
	{
		printf("Capturing background\n");
		if ( capBackground < FRAMES+1 )
		{
		printf("Writing frame %d to background list\n", capBackground);
		memcpy(background+((capBackground-1)*WIDTH*HEIGHT*3),p,WIDTH*HEIGHT*3);

		capBackground++;
	}
	}

	if ( capBackground-1 == FRAMES )
{
	haveBackground=1;

		capBackground=0;
}
                
                int found=0;

	if ( ( subtract ) && ( haveBackground ) )
	{
		
printf("Subtracting background\n");
y=0;x=0;
			for(y=0; y<HEIGHT*FRAMES; y++)
{
				for(x=0; x<WIDTH*3; x=x+3)
		{
			up=0; down=0; left=0; right=0; found=0;

					// http://csharphelper.com/blog/2014/10/quickly-convert-an-image-to-grayscale-in-c/
					// greyScale=(use_average ? (byte)((r + g + b) / 3) : (byte)(0.3 * r + 0.5 * g + 0.2 * b));

			
									if ( ( mapPointer[y%HEIGHT][x] == (char)0 ) && ( mapPointer[y%HEIGHT][(x+1)] == (char)255 ) && ( mapPointer[y%HEIGHT][(x+2)] == (char)0 ) )
						{
found=1;
}


			if ( ! found )
{
left=0; right=0; up=0; down=0;
			//// Check left above
			if ( ( x-CORRECTION*3 >= 0 ) && ( y-CORRECTION >= 0 ) )
			{
				d=y-CORRECTION;
			for(z=x-CORRECTION*3; z < x/8; z++)
			{
		
			//                                                         11111122 00011100
			// fuzzy comparision of 8 byte blocks 1 1 65566 2 2 131132 RGBRGBRG RGBRGBRG = !!!---!! 
			// Create map point with min varation and max varation
			if ((( (off_t)videoArray[d][z] > mapPointer[d%HEIGHT][z]-VARIATION/2 ) && ( (off_t)videoArray[d][z] < mapPointer[d%HEIGHT][z]+VARIATION/2 )))
			{
											left=1;
							z=x;
							d=y;
			}else{
						// This will need to be adapted to suit changes e=(z-(x-CORRECTIONS*3?))*8
						if ( ( mapPointer[d%HEIGHT][z] == (char)0 ) && ( mapPointer[d%HEIGHT][(z+1)] == (char)255 ) && ( mapPointer[d%HEIGHT][(z+2)] == (char)0 ) )
						{
							left=1;
							z=x;
							d=y;
							//printf("Left is true ");
						}
					
					d++;
			}
			}

			// Check Right above
			if ( ( x+(CORRECTION*3) < WIDTH*3 ) && ( y-CORRECTION >= 0 ) )
			{
				d=y-CORRECTION;
			for(z=x; z < x+CORRECTION*3; z=z+3)
			{

			if ((( videoArray[d][z] > mapPointer[d%HEIGHT][z]-VARIATION/2 ) && ( videoArray[d][z] < mapPointer[d%HEIGHT][z]+VARIATION/2 )) &&(( videoArray[d][z+1] > mapPointer[y%HEIGHT][z+1]-VARIATION/2 ) && ( videoArray[y][z+1] < mapPointer[d%HEIGHT][z+1]+VARIATION/2 )) && (( videoArray[d][z+2] > mapPointer[d%HEIGHT][z+2]-VARIATION/2 ) && ( videoArray[d][z+2] < mapPointer[d%HEIGHT][z+2]+VARIATION/2 )) )
			{

							right=1;
							z=x+CORRECTION*3;
							d=y;
							//printf("Right is true ");
						
			}else if (( mapPointer[d%HEIGHT][z] == (char)0 ) && ( mapPointer[d%HEIGHT][(z+1)] == (char)255 ) && ( mapPointer[d%HEIGHT][(z+2)] == (char)0 ) ){
				right=1;
				z=x+CORRECTION*3;
				d=y;
			}
		
		d++;
}
}

			// Check Bellow Left (INCORRECT) Will never be true
			if ( ( y+CORRECTION < HEIGHT*FRAMES) && ( x-CORRECTION*3 >= 0 ) )
			{
d=x-CORRECTION*3;
			for(z=y; z < y+CORRECTION; z++)
			{
	
					
								if ((( videoArray[z][d] > mapPointer[z%HEIGHT][d]-VARIATION/2 ) && ( videoArray[z][d] < mapPointer[z%HEIGHT][d]+VARIATION/2 )) && (( videoArray[z][d+1] > mapPointer[z%HEIGHT][d+1]-VARIATION/2 ) && ( videoArray[z][d+1] < mapPointer[z%HEIGHT][d+1]+VARIATION/2 )) && (( videoArray[z][d+2] > mapPointer[z%HEIGHT][d+2]-VARIATION/2 ) && ( videoArray[z][d+2] < mapPointer[z%HEIGHT][d+2]+VARIATION/2 )) )
			{
											up=1;
							z=y+CORRECTION;
							d=x;
			}else{
						if ( ( mapPointer[z%HEIGHT][d] == (char)0 ) && ( mapPointer[z%HEIGHT][d+1] == (char)255 ) && ( mapPointer[z%HEIGHT][d+2] == (char)0 ) )
						{
							up=1;
							z=y+CORRECTION;
							d=x;
//printf("Above is true ");
						}
			
			d=d+3;
			}
		}

			// Check Bellow Right
			if ( ( y+CORRECTION < HEIGHT*FRAMES) && ( x+(CORRECTION*3) < WIDTH*3 ) )
			{
				d=x;
			for(z=y; z < y+CORRECTION; z++)
			{
				

			if ((( videoArray[z][d] > mapPointer[z%HEIGHT][d]-VARIATION/2 ) && ( videoArray[z][d] < mapPointer[z%HEIGHT][d]+VARIATION/2 )) && (( videoArray[z][d+1] > mapPointer[z%HEIGHT][d+1]-VARIATION/2 ) && ( videoArray[z][d+1] < mapPointer[z%HEIGHT][d+1]+VARIATION/2 )) && (( videoArray[z][d+2] > mapPointer[z%HEIGHT][d+2]-VARIATION/2 ) && ( videoArray[z][d+2] < mapPointer[z%HEIGHT][d+2]+VARIATION/2 )) )
			{

							down=1;
							z=y+CORRECTION;
							d=x+CORRECTION*3;
//printf("Bellow is true\n");
						}else if ( ( mapPointer[z%HEIGHT][d] == (char)0 ) && ( mapPointer[z%HEIGHT][d+1] == (char)255 ) && ( mapPointer[z%HEIGHT][d+2] == (char)0 ) )
						{
							down=1;
							z=y+CORRECTION;
							d=x+CORRECTION*3;
						}
			
			d=d+3;
			}
		}

						if ( ( right ) && ( left ) && ( up ) && ( down ) )
			{
				//printf(".");
						//printf("True\n");

						found=1;
			}
		}
		
		
			
			if ( ! found )
{
left=0; right=0; up=0; down=0;
			//// Check left
			if ( x-CORRECTION*3 >= 0 )
			{
			for(z=x-CORRECTION*3; z < x; z=z+3)
			{

			if ((( videoArray[y][z] > mapPointer[y%HEIGHT][z]-VARIATION/2 ) && ( videoArray[y][z] < mapPointer[y%HEIGHT][z]+VARIATION/2 )) &&(( videoArray[y][z+1] > mapPointer[y%HEIGHT][z+1]-VARIATION/2 ) && ( videoArray[y][z+1] < mapPointer[y%HEIGHT][z+1]+VARIATION/2 )) && (( videoArray[y][z+2] > mapPointer[y%HEIGHT][z+2]-VARIATION/2 ) && ( videoArray[y][z+2] < mapPointer[y%HEIGHT][z+2]+VARIATION/2 )) )
			{
									left=1;
							z=x;
			}else{
						if ( ( mapPointer[y%HEIGHT][z] == (char)0 ) && ( mapPointer[y%HEIGHT][(z+1)] == (char)255 ) && ( mapPointer[y%HEIGHT][(z+2)] == (char)0 ) )
						{
							left=1;
							z=x;
							//printf("Left is true ");
						}
			}
			}

			// Check Right
			if ( x+(CORRECTION*3) < WIDTH*3 )
			{
			for(z=x; z < x+CORRECTION*3; z=z+3)
			{

			if ((( videoArray[y][z] > mapPointer[y%HEIGHT][z]-VARIATION/2 ) && ( videoArray[y][z] < mapPointer[y%HEIGHT][z]+VARIATION/2 )) &&(( videoArray[y][z+1] > mapPointer[y%HEIGHT][z+1]-VARIATION/2 ) && ( videoArray[y][z+1] < mapPointer[y%HEIGHT][z+1]+VARIATION/2 )) && (( videoArray[y][z+2] > mapPointer[y%HEIGHT][z+2]-VARIATION/2 ) && ( videoArray[y][z+2] < mapPointer[y%HEIGHT][z+2]+VARIATION/2 )) )
			{

							right=1;
							z=x+CORRECTION*3;
							//printf("Right is true ");
						
			}else if (( mapPointer[y%HEIGHT][z] == (char)0 ) && ( mapPointer[y%HEIGHT][(z+1)] == (char)255 ) && ( mapPointer[y%HEIGHT][(z+2)] == (char)0 ) ){
				right=1;
				z=x+CORRECTION*3;
			}
}
}

			// Check Above
			if ( y-CORRECTION >= 0 )
			{

			for(z=y-CORRECTION; z < y; z++)
			{

			if ((( videoArray[z][x] > mapPointer[z%HEIGHT][x]-VARIATION/2 ) && ( videoArray[z][x] < mapPointer[z%HEIGHT][x]+VARIATION/2 )) && (( videoArray[z][x+1] > mapPointer[z%HEIGHT][x+1]-VARIATION/2 ) && ( videoArray[z][x+1] < mapPointer[z%HEIGHT][x+1]+VARIATION/2 )) && (( videoArray[z][x+2] > mapPointer[z%HEIGHT][x+2]-VARIATION/2 ) && ( videoArray[z][x+2] < mapPointer[z%HEIGHT][x+2]+VARIATION/2 )) )
			{
											up=1;
							z=y;
			}
				else		if ( ( mapPointer[z%HEIGHT][x] == (char)0 ) && ( mapPointer[z%HEIGHT][x+1] == (char)255 ) && ( mapPointer[z%HEIGHT][x+2] == (char)0 ) ){
						{
							up=1;
							z=y;
//printf("Above is true ");
						}
			}
			}

			// Check Bellow
			if ( y+CORRECTION < HEIGHT*FRAMES)
			{
			for(z=y; z < y+CORRECTION; z++)
			{

			if ((( videoArray[z][x] > mapPointer[z%HEIGHT][x]-VARIATION/2 ) && ( videoArray[z][x] < mapPointer[z%HEIGHT][x]+VARIATION/2 )) && (( videoArray[z][x+1] > mapPointer[z%HEIGHT][x+1]-VARIATION/2 ) && ( videoArray[z][x+1] < mapPointer[z%HEIGHT][x+1]+VARIATION/2 )) && (( videoArray[z][x+2] > mapPointer[z%HEIGHT][x+2]-VARIATION/2 ) && ( videoArray[z][x+2] < mapPointer[z%HEIGHT][x+2]+VARIATION/2 )) )
			{

							down=1;
							z=y+CORRECTION;
//printf("Bellow is true\n");
						}else if ( ( mapPointer[z%HEIGHT][x] == (char)0 ) && ( mapPointer[z%HEIGHT][x+1] == (char)255 ) && ( mapPointer[z%HEIGHT][x+2] == (char)0 ) )
						{
							down=1;
							z=y+CORRECTION;
						}
			}
			}

			//printf("%d %d %d %d\n",left,right,up,down);

						if ( ( right ) && ( left ) && ( up ) && ( down ) )
			{
				//printf(".");
						//printf("True\n");

						found=1;
			}
		
	}
	
				//if ( found )
//{
		
		
		//left=0; right=0; up=0; down=0;
			////// Check left
			//if ( x-CORRECTION*3 >= 0 )
			//{
			//for(z=x-CORRECTION*3; z < x; z=z+3)
			//{

						//if ( ! ( ( mapPointer[y%HEIGHT][z] == (char)0 ) && ( mapPointer[y%HEIGHT][(z+1)] == (char)255 ) && ( mapPointer[y%HEIGHT][(z+2)] == (char)0 ) ) )
						//{
							//left=1;
							//z=x;
							////printf("Left is true ");
						//}
			//}
			//}

			//// Check Right
			//if ( x+(CORRECTION*3) < WIDTH*3 )
			//{
			//for(z=x; z < x+CORRECTION*3; z=z+3)
			//{

			//if (! ( (( videoArray[y][z] > mapPointer[y%HEIGHT][z]-VARIATION/2 ) && ( videoArray[y][z] < mapPointer[y%HEIGHT][z]+VARIATION/2 )) &&(( videoArray[y][z+1] > mapPointer[y%HEIGHT][z+1]-VARIATION/2 ) && ( videoArray[y][z+1] < mapPointer[y%HEIGHT][z+1]+VARIATION/2 )) && (( videoArray[y][z+2] > mapPointer[y%HEIGHT][z+2]-VARIATION/2 ) && ( videoArray[y][z+2] < mapPointer[y%HEIGHT][z+2]+VARIATION/2 )) ) )
			//{

							//right=1;
							//z=x+CORRECTION*3;
							////printf("Right is true ");
						
			//}else if ( ! (( mapPointer[y%HEIGHT][z] == (char)0 ) && ( mapPointer[y%HEIGHT][(z+1)] == (char)255 ) && ( mapPointer[y%HEIGHT][(z+2)] == (char)0 ) ) ){
				//right=1;
				//z=x+CORRECTION*3;
			//}
//}
//}

			//// Check Above
			//if ( y-CORRECTION >= 0 )
			//{

			//for(z=y-CORRECTION; z < y; z++)
			//{

						//if ( ! ( ( mapPointer[z%HEIGHT][x] == (char)0 ) && ( mapPointer[z%HEIGHT][x+1] == (char)255 ) && ( mapPointer[z%HEIGHT][x+2] == (char)0 ) ) )
						//{
							//up=1;
							//z=y;
////printf("Above is true ");
						//}
			//}
			//}

			//// Check Bellow
			//if ( y+CORRECTION < HEIGHT*FRAMES)
			//{
			//for(z=y; z < y+CORRECTION; z++)
			//{

			//if (! ((( videoArray[z][x] > mapPointer[z%HEIGHT][x]-VARIATION/2 ) && ( videoArray[z][x] < mapPointer[z%HEIGHT][x]+VARIATION/2 )) && (( videoArray[z][x+1] > mapPointer[z%HEIGHT][x+1]-VARIATION/2 ) && ( videoArray[z][x+1] < mapPointer[z%HEIGHT][x+1]+VARIATION/2 )) && (( videoArray[z][x+2] > mapPointer[z%HEIGHT][x+2]-VARIATION/2 ) && ( videoArray[z][x+2] < mapPointer[z%HEIGHT][x+2]+VARIATION/2 )) ) )
			//{

							//down=1;
							//z=y+CORRECTION;
////printf("Bellow is true\n");
						//}else if ( ! ( ( mapPointer[z%HEIGHT][x] == (char)0 ) && ( mapPointer[z%HEIGHT][x+1] == (char)255 ) && ( mapPointer[z%HEIGHT][x+2] == (char)0 ) ) )
						//{
							//down=1;
							//z=y+CORRECTION;
						//}
			//}
			
		
						//if ( ( right ) && ( left ) && ( up ) && ( down ) )
			//{
				////printf(".");
						////printf("True\n");

						//found=0;
			//}
		//}
//}
			if ( found )
			{
										mapPointer[y%HEIGHT][x] = (char)0;
						mapPointer[y%HEIGHT][x+1] = (char)255;
						mapPointer[y%HEIGHT][x+2] = (char)0;
			}

			}
}
	
}

	if (startRecord )
	{
		frame++;
	}

//printf(" displaying frame\n");
	copytoscreen(p,startRecord,frame);
*/
}

int read_frame(void)
{
        struct v4l2_buffer buf;
        unsigned int i;
        int length=0;

        switch (io) {
        case IO_METHOD_READ:
                if (-1 == read(fd, buffers[0].start, buffers[0].length)) {
                        switch (errno) {
                        case EAGAIN:
                                return 0;

                        case EIO:
                                /* Could ignore EIO, see spec. */

                                /* fall through */

                        default:
                                errno_exit("read");
                        }
                }

                process_image(buffers[0].start, buffers[0].length);
                break;

        case IO_METHOD_MMAP:
                CLEAR(buf);

                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_MMAP;

                if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
                        switch (errno) {
                        case EAGAIN:
                                return 0;

                        case EIO:
                                /* Could ignore EIO, see spec. */

                                /* fall through */

                        default:
                                errno_exit("VIDIOC_DQBUF");
                        }
                }

                assert(buf.index < n_buffers);

                process_image(buffers[buf.index].start, buf.bytesused);
				length=buf.bytesused;
                if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
                        errno_exit("VIDIOC_QBUF");
                break;

        case IO_METHOD_USERPTR:
                CLEAR(buf);

                buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory = V4L2_MEMORY_USERPTR;

                if (-1 == xioctl(fd, VIDIOC_DQBUF, &buf)) {
                        switch (errno) {
                        case EAGAIN:
                                return 0;

                        case EIO:
                                /* Could ignore EIO, see spec. */

                                /* fall through */

                        default:
                                errno_exit("VIDIOC_DQBUF");
                        }
                }

                for (i = 0; i < n_buffers; ++i)
                        if (buf.m.userptr == (unsigned long)buffers[i].start
                            && buf.length == buffers[i].length)
                                break;

                assert(i < n_buffers);

                process_image((void *)buf.m.userptr, buf.bytesused);
length=buf.bytesused;
                if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
                        errno_exit("VIDIOC_QBUF");
                break;
        }

        return -1;
}

void mainloop(void)
{
		//char my_video_dev[100]  = "/dev/video1";

int done=0;

int length=0;

    

   //map = mmap(0,WIDTH*HEIGHT*3,PROT_READ|PROT_WRITE,MAP_SHARED,fd,buffers[0].offset);
   //if (map == NULL) {
	//printf("Error: Mmap returned NULL\n");
	 //return(-5);
   //} 




        unsigned int count;

        count = frame_count;

   // --------------------------------------------------------------------------
   // Set up out video output
   // --------------------------------------------------------------------------

   SDL_Init(SDL_INIT_VIDEO);
   screen = SDL_SetVideoMode(WIDTH, HEIGHT, 24, SDL_SWSURFACE);
   if ( screen == NULL ) {
	printf(stderr, "Couldn't set 640x480x8 video mode: %s\n",
	SDL_GetError());
	exit(1);
   }
   SDL_WM_SetCaption("VFX2", NULL);

   while ( ! done ) {
	while (SDL_PollEvent(&event))
	{
	   switch(event.type)
	   {
		          case SDL_KEYDOWN:
                switch( event.key.keysym.sym ){
	      case SDLK_ESCAPE: done =1; break;
	      case SDLK_b: capBackground=1; break;
	      case SDLK_s:
	      subtract=!subtract;
	      break;
	      case SDLK_r:
	      startRecord=!startRecord;
	      break;
                    default:
                        break;
                }
				break;
	      case SDL_QUIT: done = 1; break;
	   }
	}
	

	
                for (;;) {
                        fd_set fds;
                        struct timeval tv;
                        int r;

                        FD_ZERO(&fds);
                        FD_SET(fd, &fds);

                        /* Timeout. */
                        tv.tv_sec = 20;
                        tv.tv_usec = 0;

                        r = select(fd + 1, &fds, NULL, NULL, &tv);

                        if (-1 == r) {
                                if (EINTR == errno)
                                        continue;
                                errno_exit("select");
                        }

                        if (0 == r) {
                                fprintf(stderr, "select timeout\n");
                                //exit(EXIT_FAILURE);
                        }

                        if (read_frame())
                                break;
                        /* EAGAIN - continue select loop. */
                }
                
	

   }




   //return EXIT_SUCCESS;
}

void stop_capturing(void)
{
        enum v4l2_buf_type type;

        switch (io) {
        case IO_METHOD_READ:
                /* Nothing to do. */
                break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
                type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                if (-1 == xioctl(fd, VIDIOC_STREAMOFF, &type))
                        errno_exit("VIDIOC_STREAMOFF");
                break;
        }
}

void start_capturing(void)
{
        unsigned int i;
        enum v4l2_buf_type type;

        switch (io) {
        case IO_METHOD_READ:
                /* Nothing to do. */
                break;

        case IO_METHOD_MMAP:
                for (i = 0; i < n_buffers; ++i) {
                        struct v4l2_buffer buf;

                        CLEAR(buf);
                        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                        buf.memory = V4L2_MEMORY_MMAP;
                        buf.index = i;

                        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
                                errno_exit("VIDIOC_QBUF");
                }
                type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
                        errno_exit("VIDIOC_STREAMON");
                break;

        case IO_METHOD_USERPTR:
                for (i = 0; i < n_buffers; ++i) {
                        struct v4l2_buffer buf;

                        CLEAR(buf);
                        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                        buf.memory = V4L2_MEMORY_USERPTR;
                        buf.index = i;
                        buf.m.userptr = (unsigned long)buffers[i].start;
                        buf.length = buffers[i].length;

                        if (-1 == xioctl(fd, VIDIOC_QBUF, &buf))
                                errno_exit("VIDIOC_QBUF");
                }
                type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                if (-1 == xioctl(fd, VIDIOC_STREAMON, &type))
                        errno_exit("VIDIOC_STREAMON");
                break;
        }
}

void uninit_device(void)
{
        unsigned int i;

        switch (io) {
        case IO_METHOD_READ:
                free(buffers[0].start);
                break;

        case IO_METHOD_MMAP:
                for (i = 0; i < n_buffers; ++i)
                        if (-1 == munmap(buffers[i].start, buffers[i].length))
                                errno_exit("munmap");
                break;

        case IO_METHOD_USERPTR:
                for (i = 0; i < n_buffers; ++i)
                        free(buffers[i].start);
                break;
        }

        free(buffers);
}

void init_read(unsigned int buffer_size)
{
        buffers = calloc(1, sizeof(*buffers));

        if (!buffers) {
                fprintf(stderr, "Out of memory\n");
                exit(EXIT_FAILURE);
        }

        buffers[0].length = buffer_size;
        buffers[0].start = malloc(buffer_size);

        if (!buffers[0].start) {
                fprintf(stderr, "Out of memory\n");
                exit(EXIT_FAILURE);
        }
}

void init_mmap(void)
{
        struct v4l2_requestbuffers req;

        CLEAR(req);

        req.count = 4;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;

        if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
                if (EINVAL == errno) {
                        fprintf(stderr, "%s does not support "
                                 "memory mapping\n", dev_name);
                        exit(EXIT_FAILURE);
                } else {
                        errno_exit("VIDIOC_REQBUFS");
                }
        }

        if (req.count < 2) {
                fprintf(stderr, "Insufficient buffer memory on %s\n",
                         dev_name);
                exit(EXIT_FAILURE);
        }

        buffers = calloc(req.count, sizeof(*buffers));

        if (!buffers) {
                fprintf(stderr, "Out of memory\n");
                exit(EXIT_FAILURE);
        }

        for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
                struct v4l2_buffer buf;

                CLEAR(buf);

                buf.type        = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                buf.memory      = V4L2_MEMORY_MMAP;
                buf.index       = n_buffers;

                if (-1 == xioctl(fd, VIDIOC_QUERYBUF, &buf))
                        errno_exit("VIDIOC_QUERYBUF");

                buffers[n_buffers].length = buf.length;
                buffers[n_buffers].start =
                        mmap(NULL /* start anywhere */,
                              buf.length,
                              PROT_READ | PROT_WRITE /* required */,
                              MAP_SHARED /* recommended */,
                              fd, buf.m.offset);

                if (MAP_FAILED == buffers[n_buffers].start)
                        errno_exit("mmap");
        }
}

void init_userp(unsigned int buffer_size)
{
        struct v4l2_requestbuffers req;

        CLEAR(req);

        req.count  = 4;
        req.type   = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_USERPTR;

        if (-1 == xioctl(fd, VIDIOC_REQBUFS, &req)) {
                if (EINVAL == errno) {
                        fprintf(stderr, "%s does not support "
                                 "user pointer i/o\n", dev_name);
                        exit(EXIT_FAILURE);
                } else {
                        errno_exit("VIDIOC_REQBUFS");
                }
        }

        buffers = calloc(4, sizeof(*buffers));

        if (!buffers) {
                fprintf(stderr, "Out of memory\n");
                exit(EXIT_FAILURE);
        }

        for (n_buffers = 0; n_buffers < 4; ++n_buffers) {
                buffers[n_buffers].length = buffer_size;
                buffers[n_buffers].start = malloc(buffer_size);

                if (!buffers[n_buffers].start) {
                        fprintf(stderr, "Out of memory\n");
                        exit(EXIT_FAILURE);
                }
        }
}

void init_device(void)
{
        struct v4l2_capability cap;
        struct v4l2_cropcap cropcap;
        struct v4l2_crop crop;
        struct v4l2_format fmt;
        unsigned int min;
        
        if (-1 == xioctl(fd, VIDIOC_QUERYCAP, &cap)) {
                if (EINVAL == errno) {
                        fprintf(stderr, "%s is no V4L2 device\n",
                                 dev_name);
                        exit(EXIT_FAILURE);
                } else {
                        errno_exit("VIDIOC_QUERYCAP");
                }
        }

        if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
                fprintf(stderr, "%s is no video capture device\n",
                         dev_name);
                exit(EXIT_FAILURE);
        }

        switch (io) {
        case IO_METHOD_READ:
                if (!(cap.capabilities & V4L2_CAP_READWRITE)) {
                        fprintf(stderr, "%s does not support read i/o\n",
                                 dev_name);
                        exit(EXIT_FAILURE);
                }
                break;

        case IO_METHOD_MMAP:
        case IO_METHOD_USERPTR:
                if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
                        fprintf(stderr, "%s does not support streaming i/o\n",
                                 dev_name);
                        exit(EXIT_FAILURE);
                }
                break;
        }


        /* Select video input, video standard and tune here. */


        CLEAR(cropcap);

        cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

        if (0 == xioctl(fd, VIDIOC_CROPCAP, &cropcap)) {
                crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
                crop.c = cropcap.defrect; /* reset to default */

                if (-1 == xioctl(fd, VIDIOC_S_CROP, &crop)) {
                        switch (errno) {
                        case EINVAL:
                                /* Cropping not supported. */
                                break;
                        default:
                                /* Errors ignored. */
                                break;
                        }
                }
        } else {
                /* Errors ignored. */
        }


        CLEAR(fmt);

        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (force_format) {
	fprintf(stderr, "Set H264\r\n");
                fmt.fmt.pix.width       = 640; //replace
                fmt.fmt.pix.height      = 480; //replace
                fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_H264; //replace
                fmt.fmt.pix.field       = V4L2_FIELD_ANY;

                if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
                        errno_exit("VIDIOC_S_FMT");

                /* Note VIDIOC_S_FMT may change width and height. */
        } else {
 
	
			
			                fmt.fmt.pix.width       = WIDTH; //replace
                fmt.fmt.pix.height      = HEIGHT; //replace
			fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB24; //replace
			                fmt.fmt.pix.field       = V4L2_FIELD_ANY;
                /* Preserve original settings as set by v4l2-ctl for example */
                            if (-1 == xioctl(fd, VIDIOC_S_FMT, &fmt))
                        errno_exit("VIDIOC_S_FMT");
                //if (-1 == xioctl(fd, VIDIOC_G_FMT, &fmt))
                        //errno_exit("VIDIOC_G_FMT");

        }

        /* Buggy driver paranoia. */
        min = fmt.fmt.pix.width * 2;
        if (fmt.fmt.pix.bytesperline < min)
                fmt.fmt.pix.bytesperline = min;
        min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
        if (fmt.fmt.pix.sizeimage < min)
                fmt.fmt.pix.sizeimage = min;


        switch (io) {
        case IO_METHOD_READ:
                init_read(fmt.fmt.pix.sizeimage);
                break;

        case IO_METHOD_MMAP:
                init_mmap();
                break;

        case IO_METHOD_USERPTR:
                init_userp(fmt.fmt.pix.sizeimage);
                break;
        }
}

void close_device(void)
{
        if (-1 == close(fd))
                errno_exit("close");

        fd = -1;
}

void open_device(void)
{
        struct stat st;

        if (-1 == stat(dev_name, &st)) {
                fprintf(stderr, "Cannot identify '%s': %d, %s\n",
                         dev_name, errno, strerror(errno));
                exit(EXIT_FAILURE);
        }

        if (!S_ISCHR(st.st_mode)) {
                fprintf(stderr, "%s is no device\n", dev_name);
                exit(EXIT_FAILURE);
        }

        fd = open(dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);

        if (-1 == fd) {
                fprintf(stderr, "Cannot open '%s': %d, %s\n",
                         dev_name, errno, strerror(errno));
                exit(EXIT_FAILURE);
        }

}

void usage(FILE *fp, int argc, char **argv)
{
        fprintf(fp,
                 "Usage: %s [options]\n\n"
                 "Version 1.3\n"
                 "Options:\n"
                 "-d | --device name   Video device name [%s]\n"
                 "-h | --help          Print this message\n"
                 "-m | --mmap          Use memory mapped buffers [default]\n"
                 "-r | --read          Use read() calls\n"
                 "-u | --userp         Use application allocated buffers\n"
                 "-o | --output        Outputs stream to stdout\n"
                 "-f | --format        Force format to 640x480 YUYV\n"
                 "-c | --count         Number of frames to grab [%i]\n"
                 "",
                 argv[0], dev_name, frame_count);
}

static const char short_options[] = "d:hmruofc:";

static const struct option
long_options[] = {
        { "device", required_argument, NULL, 'd' },
        { "help",   no_argument,       NULL, 'h' },
        { "mmap",   no_argument,       NULL, 'm' },
        { "read",   no_argument,       NULL, 'r' },
        { "userp",  no_argument,       NULL, 'u' },
        { "output", no_argument,       NULL, 'o' },
        { "format", no_argument,       NULL, 'f' },
        { "count",  required_argument, NULL, 'c' },
        { 0, 0, 0, 0 }
};

int main(int argc, char **argv)
{
        dev_name = "/dev/video0";

        for (;;) {
                int idx;
                int c;

                c = getopt_long(argc, argv,
                                short_options, long_options, &idx);

                if (-1 == c)
                        break;

                switch (c) {
                case 0: /* getopt_long() flag */
                        break;

                case 'd':
                        dev_name = optarg;
                        break;

                case 'h':
                        usage(stdout, argc, argv);
                        exit(EXIT_SUCCESS);

                case 'm':
                        io = IO_METHOD_MMAP;
                        break;

                case 'r':
                        io = IO_METHOD_READ;
                        break;

                case 'u':
                        io = IO_METHOD_USERPTR;
                        break;

                case 'o':
                        out_buf++;
                        break;

                case 'f':
                        force_format++;
                        break;

                case 'c':
                        errno = 0;
                        frame_count = strtol(optarg, NULL, 0);
                        if (errno)
                                errno_exit(optarg);
                        break;

                default:
                        usage(stderr, argc, argv);
                        exit(EXIT_FAILURE);
		}
       } 

        open_device();
        init_device();
        start_capturing();
        mainloop();
        stop_capturing();
        uninit_device();
        close_device();
        fprintf(stderr, "\n");
        
        SDL_Quit();
        return 0;
}
