#ifndef _APPINTERFACE_H_
#define _APPINTERFACE_H_

#define CUSTOM_NAME	L"CustomDevice"
#define CREATE_FILE_NAME	L"\\\\.\\" CUSTOM_NAME

// 응용프로그램과 통신하는데 사용되는 명령어를 정의한다
#define IOCTL_TABLEHOOKER_GET_LOG		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_TABLEHOOKER_CLR_LOG		CTL_CODE(FILE_DEVICE_UNKNOWN, 0x801, METHOD_BUFFERED, FILE_ANY_ACCESS)

// --------------- 로그를 관리하기 위해서 정의한다 -----------------
#define MAX_BUFFER_SIZE	(0x10000)	// 64KBytes를 최대값으로 사용한다
#define MAX_LOG_COUNT	1000		// 로그엔트리의 최대수를 정의한다

typedef struct _READ_REQUEST_DATA {
	ULONG_PTR		Offset;
	ULONG			Length;
} READ_REQUEST_DATA, *PREAD_REQUEST_DATA;

typedef struct _READ_STATUS_DATA {
	NTSTATUS		ntStatus;
	ULONG_PTR		Information;
	ULONG			DataLength;
	unsigned char	DataBuffer[MAX_BUFFER_SIZE];
} READ_STATUS_DATA, *PREAD_STATUS_DATA;

typedef struct _RECORD_LIST_APP {
	ULONG							Length;
	ULONG							SequenceNumber;
	unsigned char					MajorFunction;
	BOOLEAN							bIsStatus;
	union
	{
		READ_REQUEST_DATA			ReadRequest;
		READ_STATUS_DATA			ReadStatus;
	}extra;
} RECORD_LIST_APP, *PRECORD_LIST_APP;

#endif //_APPINTERFACE_H_