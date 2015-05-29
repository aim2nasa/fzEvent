#ifndef ___SURFACE_GETTER_H
#define ___SURFACE_GETTER_H

#include "../common/fz_common.h"
#include "../common/fz_trace.h"

#include "fbinfo.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <zlib.h>

//#define ELAPSED_TIME_DEBUG
#ifdef ELAPSED_TIME_DEBUG
	#define START_TIMER___	gettimeofday(&start_time, 0);
	#define STOP_TIMER___	gettimeofday(&end_time, 0);
	#define GET_ELAPSED_TIME___(x) \
	{\
			timersub(&end_time, &start_time, &total_time);\
			printf("*** "x": %ld.%06ld\n", total_time.tv_sec, total_time.tv_usec);\
	}
#else
	#define START_TIMER___
	#define STOP_TIMER___
	#define GET_ELAPSED_TIME___(x)
#endif

namespace {

	static void dump_file(const char* name, const void* p, const fz::_u32 len)
	{
		char filename[512];
		sprintf(filename, "/mnt/sdcard/%s_dump", name);
		FILE* fp = fopen(filename, "w");
		if(!fp)
			return;
		fwrite(p, 1, len, fp);
		fclose(fp);
	}
}

FZ_BEGIN


#define DDMS_RAWIMAGE_VERSION 1

class surface_getter {
	public:
		int init() {
			screencap_command = "screencap";
			is_get_surface_info = true;
			raw_buffer = 0;
			resize_buffer = 0;
			compress_buffer = 0;

			is_resize_option = true;
			resize_w = -1;
			resize_h = -1;
			is_compress_option = true;
			compress_level = 1;
			out_pixel_per_bytes = 3;
		}
		int destroy() {
			safe_free(raw_buffer);
			safe_free(resize_buffer);
			safe_free(compress_buffer);
		}

		/* 현재 이 옵션은 사용하지 않도록 한다. get_buffer 수정 필요함 */
		inline void get_buffer_option_resize(bool is_resize, int dest_w, int dest_h){
			is_resize_option = is_resize;
			resize_w = dest_w;
			resize_h = dest_h;
		}

		/* 현재 이 옵션은 사용하지 않도록 한다. get_buffer 수정 필요함 */
		inline void get_buffer_option_compress(bool is_compress, int level = 1){
			is_compress_option = is_compress;
			compress_level = level;
		}

		int get_buffer(_u8** p, _u32* out_length, _u16* w, _u16* h) {
#ifdef ELAPSED_TIME_DEBUG
			struct timeval start_time;
			struct timeval end_time;
			struct timeval total_time;
#endif
			struct timeval tick;
			int ret;

			*p = 0;

			gettimeofday(&tick, 0);
			/* getbuffer phase */
			START_TIMER___;
			fz::TRACE(FZ_LOG_INFO, "GET BUFFER PHASE\n");
			ret = _get_surface_direct_access();
			ret = _get_surface_screencap_access();
			STOP_TIMER___;
			GET_ELAPSED_TIME___("GET BUFFER PHASE");

			int resize_buffer_size;
			_u32 compress_buffer_size;

			/* resizing phase (never use!!) */
			START_TIMER___;
			fz::TRACE(FZ_LOG_INFO, "RESIZING PHASE\n");
			if(likely(is_resize_option)) {
				/*if(!(resize_w >= fbi.width || resize_h >= fbi.height)) {*/
				static int threshold = 589824; /* 1024 * 576 */
				if((fbi.width * fbi.height) > threshold) {
					if(resize_w == -1 || resize_h == -1) {
						resize_w = fbi.width; resize_h = fbi.height;
						float ratio = fbi.width / (float)fbi.height ;
						while(resize_w * resize_h > threshold) {
								resize_h--;
								resize_w = (int)((float)resize_h * ratio);
						}
					}

					resize_buffer_size = resize_w*resize_h*out_pixel_per_bytes;
					if(unlikely(!resize_buffer)) 
						resize_buffer = (_u8*)malloc(resize_buffer_size);
					ret = resize_raw_image(resize_buffer);
				}
				else /* 이미지만 24비트로 변경 */ {
					if(fbi.bpp == 32) {
						resize_w = fbi.width;
						resize_h = fbi.height;

						resize_buffer_size = resize_w*resize_h*out_pixel_per_bytes;
						if(unlikely(!resize_buffer)) 
							resize_buffer = (_u8*)malloc(resize_buffer_size);

						ret = conv_image_bpp(raw_buffer, resize_buffer, resize_w, resize_h, 
								fbi.bpp, out_pixel_per_bytes*8);
					}
					else {
						fz::TRACE(FZ_LOG_ERROR, "NO SUPPORTED RAW IMAGE!\n");
						return -1;
					}
				}
				compress_buffer_size = resize_w*resize_h*out_pixel_per_bytes;
			}
#if 0 // test only
			printf("--- dump resize_buffer ---\n");
			for(i = 0 ; i < 100 ; ++i)
			{
				printf("%02X ", *(unsigned char*)(resize_buffer+i));
			}
			printf("\n");
#endif
			STOP_TIMER___;
			GET_ELAPSED_TIME___("RESIZING PAHSE");

			//dump_file("resize", resize_buffer, resize_buffer_size);

			compress_buffer_size += (compress_buffer_size+12) * 0.1f;
			/* compress phase (never use!!) */
			START_TIMER___;
			fz::TRACE(FZ_LOG_INFO, "COMPRESS PHASE\n");
			if(likely(is_compress_option)) {
				if(unlikely(!compress_buffer)) 
					compress_buffer = (_u8*)malloc(compress_buffer_size);
				ret = compress2(compress_buffer, (uLongf*)&compress_buffer_size, 
						resize_buffer, resize_buffer_size, compress_level);
				if(ret != Z_OK) {
					switch(ret) {
						case Z_MEM_ERROR: fz::TRACE(FZ_LOG_ERROR, "Compress Failure (Z_MEM_ERROR)\n"); break;
						case Z_BUF_ERROR: fz::TRACE(FZ_LOG_ERROR, "Compress Failure (Z_BUF_ERROR)\n"); break;
						default: fz::TRACE(FZ_LOG_ERROR, "Compress Failure (UNKNOWN:%d)\n", ret); break;
					}
				}
			}
#if 0 // test only
			printf("--- dump compress_buffer ---\n");
			for(i = 0 ; i < 100 ; ++i)
			{
				printf("%02X ", *(unsigned char*)(compress_buffer+i));
			}
			printf("\n");
#endif
			STOP_TIMER___;
			GET_ELAPSED_TIME___("COMPRESS PHASE");

			//dump_file("compress", compress_buffer, compress_buffer_size);

			/* 옵션으로 어느 버퍼를 선택할지 지정해야 하나 현재는 하지 않도록 함 */

			*out_length = compress_buffer_size 
					/*+ sizeof(*w) + sizeof(*h) + sizeof(tick.tv_sec), sizeof(tick.tv_usec)*/
					;
			*p = (_u8*)malloc(*out_length);
			//memcpy(*p, &tick.tv_sec, sizeof(tick.tv_sec));		/* 4 */
			//memcpy(&p, &tick.tv_usec, sizeof(tick.tv_usec));		/* 4 */
			//memcpy(*p, w, sizeof(*w));							/* 2 */
			//memcpy(*p, h, sizeof(*h));							/* 2 */
			memcpy(*p, compress_buffer, compress_buffer_size);	/* v */
			*w = resize_w;
			*h = resize_h;


			return 0;
		}
	private:
		int resize_raw_image(_u8* dest)
		{
			_u32 pixel_per_bytes = fbi.bpp / 8; 
			_u32 x_ratio = (_u32)((fbi.width<<16)/resize_w) + 1;
			_u32 y_ratio = (_u32)((fbi.height<<16)/resize_h) + 1;

			_u32 i, j;
			_u32 x2, y2;
			for(i = 0 ; i < resize_h ; ++i) {
				y2 = ((i * y_ratio)>>16) ;
				for(j = 0 ; j < resize_w ; ++j) {
					x2 = ((j * x_ratio)>>16) ;

					dest[(i*resize_w*out_pixel_per_bytes)+(j*out_pixel_per_bytes)+0] = 
						raw_buffer[(y2*fbi.width*pixel_per_bytes)+(x2*pixel_per_bytes)+2];

					dest[(i*resize_w*out_pixel_per_bytes)+(j*out_pixel_per_bytes)+1] = 
						raw_buffer[(y2*fbi.width*pixel_per_bytes)+(x2*pixel_per_bytes)+1];

					dest[(i*resize_w*out_pixel_per_bytes)+(j*out_pixel_per_bytes)+2] = 
						raw_buffer[(y2*fbi.width*pixel_per_bytes)+(x2*pixel_per_bytes)+0];
				}
			}

			return 0;
		}
		int conv_image_bpp(_u8* src, _u8* dest, const _u16 w, const _u16 h, _u32 src_bpp, _u32 dest_bpp) {
			_u32 i, j;
			src_bpp /= 8; /* maybe 32 */
			dest_bpp /= 8; /* maybe 24 */

			for(i = 0 ; i < h ; ++i) {
				for(j = 0 ; j < w ; ++j) {
					dest[(i * w * dest_bpp) + (j * dest_bpp) + 0] = src[(i * w * src_bpp) + (j * src_bpp) + 2];
					dest[(i * w * dest_bpp) + (j * dest_bpp) + 1] = src[(i * w * src_bpp) + (j * src_bpp) + 1];
					dest[(i * w * dest_bpp) + (j * dest_bpp) + 2] = src[(i * w * src_bpp) + (j * src_bpp) + 0];
				}
			}

			return 0;
		}

	private:
		int _get_surface_direct_access() {
			/* 현재는 미구현 */
			fz::TRACE(FZ_LOG_INFO, "Skip Direct Access\n");

			return -1;
		}
		int _get_surface_screencap_access() {
			int ret;
			FILE* fp;
			
			fp = popen(screencap_command, "r");
			if(!fp) {
				fz::TRACE(FZ_LOG_ERROR, "Capture open failure!\n");
				return -1;
			}

			ret = 0;
			int w, h, f;
			ret = fread(&w, 1, sizeof(w), fp);
			ret = fread(&h, 1, sizeof(h), fp);
			ret = fread(&f, 1, sizeof(f), fp);
			
			if(unlikely(is_get_surface_info)) {
				ret = _get_surface_info(w, h, f);
				if(ret) {
					fz::TRACE(FZ_LOG_ERROR, "Unknown format!\n");
					return -1;
				}
				is_get_surface_info = false;
				
				fz::TRACE(FZ_LOG_DEBUG, "w: %d h:%d, f:%d\n", w, h, f);
				fz::TRACE(FZ_LOG_DEBUG, "fbinfo.bpp = %d\n", fbi.bpp);
				fz::TRACE(FZ_LOG_DEBUG, "fbinfo.size = %d\n", fbi.size);
				fz::TRACE(FZ_LOG_DEBUG, "fbinfo.width = %d\n", fbi.width);
				fz::TRACE(FZ_LOG_DEBUG, "fbinfo.height =  %d\n", fbi.height);
				fz::TRACE(FZ_LOG_DEBUG, "fbinfo.red_offset = %d\n", fbi.red_offset);
				fz::TRACE(FZ_LOG_DEBUG, "fbinfo.red_length = %d\n", fbi.red_length);
				fz::TRACE(FZ_LOG_DEBUG, "fbinfo.green_offset = %d\n", fbi.green_offset);
				fz::TRACE(FZ_LOG_DEBUG, "fbinfo.green_length = %d\n", fbi.green_length);
				fz::TRACE(FZ_LOG_DEBUG, "fbinfo.blue_offset = %d\n", fbi.blue_offset);
				fz::TRACE(FZ_LOG_DEBUG, "fbinfo.blue_length = %d\n", fbi.blue_length); 
				fz::TRACE(FZ_LOG_DEBUG, "fbinfo.alpha_offset = %d\n", fbi.alpha_offset);
				fz::TRACE(FZ_LOG_DEBUG, "fbinfo.alpha_length %d\n", fbi.alpha_length);

			}

			if(unlikely(!raw_buffer))
				raw_buffer = (_u8*)malloc(fbi.width * fbi.height * (fbi.bpp/8));
			
			_get_raw_buffer(raw_buffer, fp);


			pclose(fp);
			return 0;
		}

		void _get_raw_buffer(_u8* p, FILE* fp){
			int ret;
			static const _u32 buff_size = 1 * 1024;
 			_u8 buff[buff_size];

			int fb_size = fbi.size ;
			int total_read = 0;
			int read_size = buff_size;

			/* read_size가 고정일 경우 seg fault 발생되는 단말 있어 fbi.size 만큼만 읽도록 변경(갤럭시 줌) 
			 * */
			while( 0 < (ret = fread(buff, 1, read_size, fp)) ) {
				total_read += ret;
				memcpy(p, buff, ret);
				p+=ret;
				if(fb_size - total_read < buff_size)
					read_size = fb_size - total_read;
			}

			fz::TRACE(FZ_LOG_DEBUG, "RAW BUFFER: %d bytes read\n", total_read);
		}

		int _get_surface_info(const int width, const int height, const int format) {

			/* see hardware/hardware.h */
			switch(format)
			{
				case 1: /* RGBA_8888 */
					fbi.bpp = 32;
					fbi.size = width * height * 4;
					fbi.red_offset = 0;		fbi.red_length = 8;
					fbi.green_offset = 8;	fbi.green_length = 8;
					fbi.blue_offset = 16;	fbi.blue_length = 8;
					fbi.alpha_offset = 24;	fbi.alpha_length = 8;
					break;
				case 2: /* RGBX_8888 */
					fbi.bpp = 32;
					fbi.size = width * height * 4;
					fbi.red_offset = 0;		fbi.red_length = 8;
					fbi.green_offset = 8;	fbi.green_length = 8;
					fbi.blue_offset = 16;	fbi.blue_length = 8;
					fbi.alpha_offset = 24;	fbi.alpha_length = 0;
					break;
				case 3: /* RGB_888 */
					fbi.bpp = 24;
					fbi.size = width * height * 3;
					fbi.red_offset = 0;		fbi.red_length = 8;
					fbi.green_offset = 8;	fbi.green_length = 8;
					fbi.blue_offset = 16;	fbi.blue_length = 8;
					fbi.alpha_offset = 24;	fbi.alpha_length = 0;
					break;
				case 4: /* RGB_565 */
					fbi.bpp = 16;
					fbi.size = width * height * 2;
					fbi.red_offset = 11;	fbi.red_length = 5;
					fbi.green_offset = 5;	fbi.green_length = 6;
					fbi.blue_offset = 0;	fbi.blue_length = 5;
					fbi.alpha_offset = 0;	fbi.alpha_length = 0;
					break;
				case 5:
					fbi.bpp = 32;
					fbi.size = width * height * 4;
					fbi.red_offset = 16;	fbi.red_length = 8;
					fbi.green_offset = 8;	fbi.green_length = 8;
					fbi.blue_offset = 0;	fbi.blue_length = 8;
					fbi.alpha_offset = 24;	fbi.alpha_length = 8;
					break;
				default: /* unknown type */
					return -1;
			}
			fbi.version = DDMS_RAWIMAGE_VERSION;
			fbi.width = width;		
			fbi.height = height;

			return 0;
		}

	public: 
		surface_getter() {
			init();
		};
		~surface_getter() {
			/* release memory */
			destroy();
		}
	private:
		const char* screencap_command;
		bool is_get_surface_info;

		struct fbinfo fbi;

		_u8* raw_buffer;
		_u8* resize_buffer;
		_u8* compress_buffer;

		bool is_resize_option ;
		int resize_w;
		int resize_h;
		bool is_compress_option ;
		int compress_level;

		_u32 out_pixel_per_bytes;
};

FZ_END

#endif
