#include "CEvtRcv.h"
#include "ace/SOCK_Stream.h"
#include "device_packet_header.h"
#include "device_packet_event.h"
#include "input.h"
#include "ace/Date_Time.h"

#define DEVM_DEVICE_PACKET_TYPE_MASK				(0x80008000) 
#define DEVM_DEVICE_PACKET_TYPE_SCREEN_CAPTURE		(0x00000000)
#define DEVM_DEVICE_PACKET_TYPE_EVENT_CAPTURE		(0x80008000)
#define DEVM_DEVICE_PACKET_TYPE_CONTROL				(0x00008000)
#define DEVM_DEVICE_PACKET_TYPE_RESERVED1			(0x80000000)

static const ACE_TString write_buffer_format[] = {
	ACE_TEXT("eventname : "),			/* [0]1 */
	ACE_TEXT("eventcount : "),			/* [1]1 */
	ACE_TEXT("eventtime : "),			/* [2]n */
	ACE_TEXT("eventid : "),				/* [3]n */
	ACE_TEXT("eventnamebyid : "),		/* [4]n */
	ACE_TEXT("eventvaluebyid : "),		/* [5]n */
};

unsigned int CEvtRcv::_sEventSequence = 0;
FILE* CEvtRcv::_sFpEvt = NULL;

CEvtRcv::CEvtRcv(ACE_SOCK_Stream* p)
	:_pStream(p)
{
	ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) CEvtRcv Constructor\n")));
}

CEvtRcv::~CEvtRcv()
{
	ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) CEvtRcv() Destructor\n")));
}

_u32 CEvtRcv::parseHeader(device_packet_header* _header, char* pBuffer)
{
	char* p = pBuffer;
	ACE_OS::memcpy(&_header->tv, p, sizeof(_header->tv));
	p += sizeof(_header->tv);
	ACE_OS::memcpy(&_header->type, p, sizeof(_header->type));
	p += sizeof(_header->type);

	_u32 len = 0;
	ACE_OS::memcpy(&len, p, sizeof(_u32));
	return len;
}

_u32 CEvtRcv::parseEvtPacket(device_packet_event* _event, char* pBuffer)
{
	_u8* p = reinterpret_cast<_u8*>(pBuffer);
	_u32 len = 0;
	_event->type = *(_u16*)p; p += sizeof(_event->type); len += sizeof(_event->type);
	_event->count = *(_u32*)p; p += sizeof(_event->count); len += sizeof(_event->count);
	_event->dev_id = *(_u32*)p; p += sizeof(_event->dev_id); len += sizeof(_event->dev_id);

	timeval _tv; _s32 _id; _u16 _code; _u32 _value;
	for (_u32 i = 0; i < _event->count; ++i)
	{
		_tv = *(timeval*)p; p += sizeof(_tv); len += sizeof(_tv);
		_id = *(_s32*)p; p += sizeof(_id); len += sizeof(_id);
		_code = *(_u16*)p; p += sizeof(_code); len += sizeof(_code);
		_value = *(_u32*)p; p += sizeof(_value); len += sizeof(_value);

		_event->tv.push_back(_tv);
		_event->id.push_back(_id);
		_event->code.push_back(_code);
		_event->value.push_back(_value);
	}
	return len;
}

void CEvtRcv::unix_timeval_to_win32_systime(const timeval& in, LPSYSTEMTIME st)
{
	LONGLONG ll;
	FILETIME ft;

	//#define USING_TIMEVAL_TO_SYSTIME_GMT_9
#ifdef USING_TIMEVAL_TO_SYSTIME_GMT_9
	{
		ll = Int32x32To64(in.tv_sec + (60 * 60 * 9), 10000000) + 116444736000000000LL;
	}
#else /*USING_TIMEVAL_TO_SYSTIME_GMT_9*/
		{
			ll = Int32x32To64(in.tv_sec, 10000000) + 116444736000000000LL;
		}
#endif /*USING_TIMEVAL_TO_SYSTIME_GMT_9*/

		ft.dwLowDateTime = (DWORD)ll;
		ft.dwHighDateTime = ll >> 32;

		FileTimeToSystemTime(&ft, st);
		st->wMilliseconds = (WORD)(in.tv_usec / 1000);
}

void CEvtRcv::OnEventCapture(char* pBuffer, _u32 len, const timeval& tv)
{
	device_packet_event e;
	_u32 lenGet = parseEvtPacket(&e, pBuffer);
	ACE_ASSERT(lenGet == len);

	ACE_TString evtType = recognize_event(tv, e);
	ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) %s event\n"),evtType.c_str()));

	write(tv, evtType == ACE_TEXT("KEY") ? true : false, evtType == ACE_TEXT("MULTITOUCH") ? true : false, evtType == ACE_TEXT("SWIPE") ? true : false, e);
	writeEventFile(e.count, e.dev_id, e.tv, e.type, e.code, e.value);
}

int CEvtRcv::svc()
{
	device_packet_header header;
	char buf[BUFSIZ];
	while (1){
		header.clear();
		ssize_t rcvSize = _pStream->recv_n(buf, sizeof(header) + sizeof(_u32));
		if (rcvSize <= 0) {
			ACE_DEBUG((LM_ERROR, ACE_TEXT("(%P|%t) Connection closed while receiving header\n")));
			return -1;
		}
		ACE_ASSERT(rcvSize == (sizeof(header) + sizeof(_u32)));
		_u32 len = parseHeader(&header, buf);
		ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) Header %dbytes received, Len=%dbytes\n"), rcvSize,len));

		rcvSize = _pStream->recv_n(buf,len);
		if (rcvSize <= 0) {
			ACE_DEBUG((LM_ERROR, ACE_TEXT("(%P|%t) Connection closed while receiving body\n")));
			return -1;
		}
		ACE_ASSERT(rcvSize==len);
		ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) Body %dbytes received\n"),rcvSize));

		switch (header.type & DEVM_DEVICE_PACKET_TYPE_MASK)
		{
		case DEVM_DEVICE_PACKET_TYPE_SCREEN_CAPTURE:
			ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) Screen Capture\n")));
			break;
		case DEVM_DEVICE_PACKET_TYPE_EVENT_CAPTURE:
			ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) Event Capture\n")));
			OnEventCapture(buf, len,header.tv);
			break;
		case DEVM_DEVICE_PACKET_TYPE_CONTROL:
			ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) Control\n")));
			break;
		case DEVM_DEVICE_PACKET_TYPE_RESERVED1:
			ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) Reserved1\n")));
			break;
		default:
			break;
		}
	}
	return 0;
}

ACE_TString CEvtRcv::recognize_event(const timeval& tv, const device_packet_event& e)
{
	ACE_TString ret;

	bool is_multitouch, is_swipe, is_key;
	is_multitouch = is_swipe = is_key = false;

	switch (e.type)
	{
	case EV_ABS: /* touch event */
	{
		{
			_s32 prev = -1;
			for (auto i = e.id.begin(); i != e.id.end(); ++i){
				if (*i != -1 && prev != -1 && *i != prev) {
					is_multitouch = true;
				}
				prev = *i;
			}
			if (is_multitouch){
				ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) EVENT: MultiTouch\n")));
				ret = ACE_TEXT("MULTITOUCH");
			}
			else{
				/* swipe는 velocity로 처리해야 하지만 현재는 우선 쉽게... */

				int first_x = -1, first_y = -1;
				int last_x = -1, last_y = -1;
				int x = -1, y = -1;
				for (_u32 i = 0; i < e.count; ++i) {
					switch (e.code[i]) {
					case ABS_MT_POSITION_X: x = e.value[i]; break;
					case ABS_MT_POSITION_Y: y = e.value[i]; break;
					}
					if (first_x == -1)
						first_x = x;
					if (first_y == -1)
						first_y = y;

					last_x = x;
					last_y = y;

					/* 우선은 임시로 이렇게 처리하는 것임, 나중에 바꿔야 함. */
					static const int velocity = 50;
					if (abs(first_x - last_x) > velocity ||
						abs(first_y - last_y) > velocity)
						is_swipe = true;
				}
				if (is_swipe) {
					ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) EVENT: Swipe\n")));
					ret = ACE_TEXT("SWIPE");
				}
				else {
					ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) EVENT: Tap\n")));
					ret = ACE_TEXT("TAP");
				}
			}
		}
		break;
	}
	case EV_KEY: /* key는 하나의 이벤트만 들어옴 */
	{
		if (e.count != 1) {
			/* 이 경우 문제 있는 것임 */
			return ACE_TEXT("");
		}
		is_key = true;
		ACE_DEBUG((LM_DEBUG, ACE_TEXT("(%P|%t) EVENT: GPIO Key\n")));
		ret = ACE_TEXT("KEY");
		break;
	}
	default:
		/* discard */
		return ACE_TEXT("");
	}
	return ret;
}

ACE_TString CEvtRcv::get_label(const struct label *labels, int value)
{
	while (labels->name && value != labels->value)
		labels++;
	if (!labels->name)
		return ACE_TEXT("");
	else
		return ACE_TEXT(labels->name);
}

void CEvtRcv::write(const timeval& tv, bool is_key, bool is_multitouch, bool is_swipe, const device_packet_event& e)
{
	ACE_Time_Value atv(tv);
	ACE_Date_Time dt(atv);
	ACE_TCHAR filename[512];
	ACE_OS::sprintf(filename, ACE_TEXT("%s%s_%04d%02d%02d_%02d%02d%02d_%03d.txt"),ACE_TEXT("SCP"),ACE_TEXT("_DEVID"),
		dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second(), dt.microsec());

	FILE* write_fp = ACE_OS::fopen(filename,ACE_TEXT("w"));

	/* sequence id */
	ACE_OS::fprintf(write_fp, ACE_TEXT("seq: %d\n"),_sEventSequence++);

	/* start time */
	/* header의 system time(timeval)으로 교체 */
	ACE_OS::fprintf(write_fp, ACE_TEXT("time: %ld%03ld\n"),tv.tv_sec, tv.tv_usec / 1000);

	if (is_key) {
		ACE_OS::fprintf(write_fp, (write_buffer_format[0] + ACE_TEXT("KEY\n")).c_str());
	}
	else {
		if (is_multitouch)
			ACE_OS::fprintf(write_fp, (write_buffer_format[0] + ACE_TEXT("MULTITOUCH\n")).c_str());
		else {
			if (is_swipe)
				ACE_OS::fprintf(write_fp, (write_buffer_format[0] + ACE_TEXT("SWIPE\n")).c_str());
			else
				ACE_OS::fprintf(write_fp, (write_buffer_format[0] + ACE_TEXT("TAP\n")).c_str());
		}
	}

	ACE_OS::fprintf(write_fp, (write_buffer_format[1] + ACE_TEXT("%d\n")).c_str(), e.count);

	ACE_OS::fprintf(write_fp, write_buffer_format[2].c_str());
	for (_u32 i = 0; i < e.count; ++i) { /* time */
		ACE_OS::fprintf(write_fp, ACE_TEXT("%ld%03ld"), e.tv[i].tv_sec, e.tv[i].tv_usec / 1000);
		if (i + 1 != e.count)
			ACE_OS::fprintf(write_fp, ACE_TEXT(", "), e.id[i]);
	}
	ACE_OS::fprintf(write_fp, ACE_TEXT("\n"));

	ACE_OS::fprintf(write_fp, write_buffer_format[3].c_str());
	for (_u32 i = 0; i < e.count; ++i) { /* id */
		ACE_OS::fprintf(write_fp, ACE_TEXT("%d"), e.id[i]);
		if (i + 1 != e.count)
			ACE_OS::fprintf(write_fp, ACE_TEXT(", "), e.id[i]);
	}
	ACE_OS::fprintf(write_fp, L"\n");

	ACE_TString code_label;
	ACE_OS::fprintf(write_fp, write_buffer_format[4].c_str());
	for (_u32 i = 0; i < e.count; ++i) { /* code */
		if (e.code[i] == 0) {/* SYN_REPORT */
			code_label = get_label(syn_labels, e.code[i]);
		}
		else {
			switch (e.type)
			{
			case EV_ABS:
				code_label = get_label(abs_labels, e.code[i]);
				break;
			case EV_KEY:
				code_label = get_label(key_labels, e.code[i]);
				break;
			}
		}
		ACE_OS::fprintf(write_fp, ACE_TEXT("%s"), code_label.c_str());
		if (i + 1 != e.count)
			ACE_OS::fprintf(write_fp, ACE_TEXT(", "), e.id[i]);
	}
	ACE_OS::fprintf(write_fp, ACE_TEXT("\n"));
	
	ACE_TString value_label;
	ACE_OS::fprintf(write_fp, write_buffer_format[5].c_str());
	for (_u32 i = 0; i < e.count; ++i) { /* value */
		switch (e.type)
		{
		case EV_ABS:
			value_label = ACE_TEXT("");
			break;
		case EV_KEY:
			value_label = get_label(key_value_labels, e.value[i]);
			break;
		}
		if (value_label == ACE_TEXT(""))
			ACE_OS::fprintf(write_fp, ACE_TEXT("%d"), e.value[i]);
		else
			ACE_OS::fprintf(write_fp, ACE_TEXT("%s"), value_label.c_str());

		if (i + 1 != e.count)
			ACE_OS::fprintf(write_fp, ACE_TEXT(", "), e.id[i]);
	}
	ACE_OS::fprintf(write_fp, ACE_TEXT("\n"));

	ACE_OS::fclose(write_fp);
}

void CEvtRcv::makeEventFile()
{
	if (CEvtRcv::_sFpEvt) {
		ACE_ERROR((LM_ERROR, ACE_TEXT("(%P|%t) event file is already opened\n")));
		return;
	}

	ACE_Date_Time dt(ACE_OS::gettimeofday());
	ACE_TCHAR filename[512];
	ACE_OS::sprintf(filename,ACE_TEXT("%s%s_%04d%02d%02d_%02d%02d%02d_%03d_RESULT.txt"),ACE_TEXT("PATH"),ACE_TEXT("_ID"),
		dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second(), dt.microsec());

	_sFpEvt = ACE_OS::fopen(filename, ACE_TEXT("wb"));
}

void CEvtRcv::writeEventFile(const _u32 count, const _s32 dev_id, const std::vector<timeval>& tv,
	const _u16& type, const std::vector<_u16>& code, const std::vector<_u32>& value)
{
	ACE_ASSERT(CEvtRcv::_sFpEvt != NULL);

	size_t written = 0;
	for (_u32 i = 0; i < count; ++i) {
		written = ACE_OS::fwrite(&tv[i], 1, sizeof(tv[i]), _sFpEvt);
		ACE_ASSERT(written == sizeof(tv[i]));

		written = ACE_OS::fwrite(&dev_id, 1, sizeof(dev_id), _sFpEvt);
		ACE_ASSERT(written == sizeof(dev_id));

		if (code[i] == 0 && value[i] == 0) { /* SYN_REPORT */
			static const _u16 syn_report_ = 0;
			written = ACE_OS::fwrite(&syn_report_, 1, sizeof(syn_report_), _sFpEvt);
			ACE_ASSERT(written == sizeof(syn_report_));
		}
		else {
			written = ACE_OS::fwrite(&type, 1, sizeof(type), _sFpEvt);
			ACE_ASSERT(written == sizeof(type));
		}

		written = ACE_OS::fwrite(&code[i], 1, sizeof(code[i]), _sFpEvt);
		ACE_ASSERT(written == sizeof(code[i]));

		written = ACE_OS::fwrite(&value[i], 1, sizeof(value[i]), _sFpEvt);
		ACE_ASSERT(written == sizeof(value[i]));
	}

	static const _u32 tok_type = 0xffffffff;
	written = ACE_OS::fwrite(&tok_type, 1, sizeof(tok_type), _sFpEvt);
	ACE_ASSERT(written == sizeof(tok_type));

	written = ACE_OS::fwrite(&tok_type, 1, sizeof(tok_type), _sFpEvt);
	ACE_ASSERT(written == sizeof(tok_type));
}

void CEvtRcv::closeEventFile()
{
	ACE_ASSERT(CEvtRcv::_sFpEvt != NULL);

	static const DWORD end_file1 = 0xffffffff;
	static const DWORD end_file2 = 0x8fffffff;

	size_t written = 0;
	written = ACE_OS::fwrite(&end_file1, 1, sizeof(end_file1), _sFpEvt);
	ACE_ASSERT(written == sizeof(end_file1));

	written = ACE_OS::fwrite(&end_file2, 1, sizeof(end_file2), _sFpEvt);
	ACE_ASSERT(written == sizeof(end_file2));

	ACE_OS::fclose(_sFpEvt);
	CEvtRcv::_sFpEvt = NULL;
}