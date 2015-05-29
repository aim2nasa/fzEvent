#include "../common/fz_common.h"

#include "surface_getter.h"
#include "../common/fz_tcp_server.h"

#include <sys/time.h>

using namespace fz;

int main(int argc, char** argv)
{
	tcp_server tc;

	while( tc.init(10002) )
		usleep(100000);

	while( tc.wait_client() )
		usleep(100000);

	surface_getter sg;
	_u32 len;
	_u8* p = 0;
	_u16 w, h;
	//struct timeval capture_time;

	struct timeval s, e, r;	
	gettimeofday(&s, 0);
	sg.get_buffer(&p, &len, &w, &h); /* w, h는 차후에 삭제하도록 한다 */

//	printf("send time(%d bytes)...", sizeof(s));
	tc.send_buffer(&s, sizeof(s));
//	printf("Done.\n");

//	printf("send w(%d bytes)...", sizeof(w));
	tc.send_buffer(&w, sizeof(w));
//	printf("Done.\n");

//	printf("send h(%d bytes)...", sizeof(h));
	tc.send_buffer(&h, sizeof(h));
//	printf("Done.\n");

//	printf("send len(%d bytes)...", sizeof(len));
	tc.send_buffer(&len, sizeof(len));
//	printf("Done.\n");
	
//	printf("send buffer(%d bytes)...", len);
	tc.send_buffer(p, len);
//	printf("Done.\n");

	gettimeofday(&e, 0);
	timersub(&e, &s, &r);

	printf("*** total elapsed time : %ld.%06ld ***\n", r.tv_sec, r.tv_usec);
	printf("w: %d, h: %d, size: %d\n", w, h, len);

	tc.destroy();
#if 0// test only
	{
		FILE* fp = fopen("/mnt/sdcard/out.raw", "w");
		fwrite(&s.tv_sec, 1, sizeof(s.tv_sec), fp);
		fwrite(&s.tv_usec, 1, sizeof(s.tv_usec), fp);
		fwrite(&w, 1, sizeof(w), fp);
		fwrite(&h, 1, sizeof(h), fp);
		fwrite(p, 1, len, fp);
	
		fclose(fp);
	}
#endif

end:
	safe_free(p);
	fz::TRACE(FZ_LOG_INFO, "Done.");
	return 0;
}
