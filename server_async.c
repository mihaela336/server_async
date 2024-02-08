#undef UNICODE
#include <windows.h>
#include <stdio.h>
#include <tchar.h>

#define BUFSIZE 4096

DWORD WINAPI InstanceThread(LPVOID);
VOID GetAnswerToRequest(LPTSTR, LPTSTR, LPDWORD);

int main(VOID)
{
	BOOL fConnected;
	DWORD dwThreadID;
	HANDLE hPipe, hThread;
	LPTSTR lpszPipename = TEXT("\\\\.\\pipe\\mynamedpipe");
	SECURITY_ATTRIBUTES m_pSecAttrib;
	SECURITY_DESCRIPTOR* m_pSecDesc;

	m_pSecDesc = (SECURITY_DESCRIPTOR*)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
	InitializeSecurityDescriptor(m_pSecDesc, SECURITY_DESCRIPTOR_REVISION);
	SetSecurityDescriptorDacl(m_pSecDesc, TRUE, (PACL)NULL, FALSE);

	m_pSecAttrib.nLength = sizeof(SECURITY_ATTRIBUTES);
	m_pSecAttrib.bInheritHandle = TRUE;
	m_pSecAttrib.lpSecurityDescriptor = m_pSecDesc;

	for (;;)
	{
		hPipe = CreateNamedPipe(
			lpszPipename,
			PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, // added FILE_FLAG_OVERLAPPED
			PIPE_TYPE_MESSAGE |
			PIPE_READMODE_MESSAGE |
			PIPE_WAIT,
			PIPE_UNLIMITED_INSTANCES,
			BUFSIZE,
			BUFSIZE,
			0,
			&m_pSecAttrib);

		if (hPipe == INVALID_HANDLE_VALUE)
		{
			printf("Create pipe a esuat");
			return 0;
		}

		fConnected = ConnectNamedPipe(hPipe, NULL) ?
			TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

		if (fConnected)
		{
			hThread = CreateThread(
				NULL,
				0,
				InstanceThread,
				(LPVOID)hPipe,
				0,
				&dwThreadID);

			if (hThread == NULL)
			{
				printf("Failed to create thread!");
				return 0;
			}
			else
			{
				CloseHandle(hThread);
			}
		}
		else
		{
			CloseHandle(hPipe);
		}
	}

	return 1;
}

DWORD WINAPI InstanceThread(LPVOID lpvParam)
{
	TCHAR chRequest[BUFSIZE];
	TCHAR chReply[BUFSIZE];
	DWORD cbBytesRead, cbReplyBytes, cbWritten;
	BOOL fSuccess;
	HANDLE hPipe;

	hPipe = (HANDLE)lpvParam;

	while (1)
	{
		OVERLAPPED overlapped = { 0 };
		overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL); // create event for overlapped IO

		fSuccess = ReadFile(
			hPipe,
			chRequest,
			BUFSIZE * sizeof(TCHAR),
			&cbBytesRead,
			&overlapped); // read asynchronously

		if (!fSuccess && GetLastError() != ERROR_IO_PENDING)
			break;

		DWORD dwResult = WaitForSingleObject(overlapped.hEvent, INFINITE); // wait for completion

		if (dwResult != WAIT_OBJECT_0)
			break;

		GetAnswerToRequest(chRequest, chReply, &cbReplyBytes);

		fSuccess = WriteFile(
			hPipe,
			chReply,
			cbReplyBytes,
			&cbWritten,
			NULL);

		if (!fSuccess || cbReplyBytes != cbWritten)
			break;
	}

	FlushFileBuffers(hPipe);
	DisconnectNamedPipe(hPipe);
	CloseHandle(hPipe);
	return 1;
}

VOID GetAnswerToRequest(LPTSTR chRequest, LPTSTR chReply, LPDWORD pchBytes)
{
	printf(TEXT("Received message from client: %s\n"), chRequest);
	strncpy(chReply, TEXT("Server received: "), BUFSIZE);
	strcat(chReply, chRequest);
	*pchBytes = (lstrlen(chReply) + 1) * sizeof(TCHAR);
}