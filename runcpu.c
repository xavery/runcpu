/* runcpu.c : run a process given its commandline without showing its window
 * and set its affinity mask to the one given as a parameter of the program. */

#include <windows.h>
#include <string.h>
#include <limits.h>
#include <stdio.h>
#include <strsafe.h>

#define ErrorMsgBox(msg,title) \
do \
{ \
MessageBox(NULL,msg,title,MB_OK | MB_ICONSTOP); \
} while(0)

/* per http://alter.org.ua/en/docs/win/args/, since winapi is too cool to have
* its own CommandLineToArgvA despite having CommandLineToArgvW. modified
* to return when it recognizes the start of the process commandline, e.g.
* --. the location of the process commandline is written to *cmdLineStart.
* this avoids unnecessary copying of a string that only gets passed directly
* into CreateProcess() anyway. also, it's not necessary to append the extra
* quotation marks, which are essential for proper process execution. */
PCHAR* CommandLineToArgvA(PCHAR CmdLine, int* _argc, PCHAR *cmdLineStart)
{
  PCHAR* argv;
  PCHAR  _argv;
  ULONG   len;
  ULONG   argc;
  CHAR   a;
  ULONG   i, j;

  BOOLEAN  in_QM;
  BOOLEAN  in_TEXT;
  BOOLEAN  in_SPACE;

  len = strlen(CmdLine);
  i = ((len+2)/2)*sizeof(PVOID) + sizeof(PVOID);

  argv = (PCHAR*)HeapAlloc(GetProcessHeap(),0,i + (len+2)*sizeof(CHAR));
  _argv = (PCHAR)(((PUCHAR)argv)+i);

  argc = 0;
  argv[argc] = _argv;
  in_QM = FALSE;
  in_TEXT = FALSE;
  in_SPACE = TRUE;
  i = 0;
  j = 0;

  while( a = CmdLine[i] )
  {
    if(in_QM)
    {
      if(a == '\"')
      {
        in_QM = FALSE;
      }
      else
      {
        _argv[j] = a;
        j++;
      }
    }
    else
    {
      /* check for the "--" sequence. if it starts here, skip the white
       * space and write the beginning of the actual commandline to
       * *cmdLineStart. */
      if(a == '-' && CmdLine[i+1] == '-')
      {
        ++i;
        while((a = CmdLine[++i]) == ' ' || a == '\t' || a == '\n'
              || a == '\r'); /* skip whitespace */
        *cmdLineStart = CmdLine + i;
        break;
      }
      switch(a)
      {
      case '\"':
        in_QM = TRUE;
        in_TEXT = TRUE;
        if(in_SPACE)
        {
          argv[argc] = _argv+j;
          argc++;
        }
        in_SPACE = FALSE;
        break;
      case ' ':
      case '\t':
      case '\n':
      case '\r':
        if(in_TEXT)
        {
          _argv[j] = '\0';
          j++;
        }
        in_TEXT = FALSE;
        in_SPACE = TRUE;
        break;
      default:
        in_TEXT = TRUE;
        if(in_SPACE)
        {
          argv[argc] = _argv+j;
          argc++;
        }
        _argv[j] = a;
        j++;
        in_SPACE = FALSE;
        break;
      }
    }
    i++;
  }
  _argv[j] = '\0';
  argv[argc] = NULL;

  (*_argc) = argc;
  return argv;
}

/* return a human-readable explanation of GetLastError() made with
 * FormatMessage() and formatted according to WIN_STRERROR_TEMPLATE defined
 * above. the returned value must be freed with HeapFree() and GetProcessHeap()
 * when it's no longer needed. */
char* win_strerror(const char* function)
{
  LPTSTR lpMsgBuf, lpDisplayBuf;
  DWORD dw = GetLastError();

  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                FORMAT_MESSAGE_IGNORE_INSERTS, NULL, dw, MAKELANGID(LANG_NEUTRAL,
                    SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, NULL);

  lpDisplayBuf = (LPTSTR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
                                   (lstrlen((LPCSTR)lpMsgBuf) + lstrlen((LPCTSTR)function) + 40)
                                   * sizeof(TCHAR));
  StringCchPrintf((LPTSTR)lpDisplayBuf, LocalSize(lpDisplayBuf) /
                  sizeof(TCHAR), "%s failed with %d : %s", function, dw, lpMsgBuf);

  LocalFree(lpMsgBuf);
  return (char*)lpDisplayBuf;
}

int WINAPI WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpszCmd,
                   int nCmd)
{
  STARTUPINFO sProcStartupInfo;
  PROCESS_INFORMATION sProcInformation;
  DWORD dwNewProcessAffinity;
  int argc, i, rv = 1;
  char **argv, *szProcCmdLine, *szLastErrorMsg;
  LPCTSTR lpProcWorkingDir = NULL;

  argv = CommandLineToArgvA(GetCommandLine(),&argc,&szProcCmdLine);

  if(argc < 2)
  {
    ErrorMsgBox("Usage : [-a affinity] [-d working_dir] -- <cmdline>",
                "Improper parameters");
    goto beach;
  }

  for(i=1; i<argc; ++i)
  {
    if(strcmp(argv[i],"-a") == 0)
    {
      ++i;
      if(i >= argc)
      {
        ErrorMsgBox("No data after -a.","Improper parameters");
        goto beach;
      }
      else if(sscanf_s(argv[i],"%8x",&dwNewProcessAffinity) != 1)
      {
        ErrorMsgBox("Improper affinity specification",
                    "Improper parameters");
        goto beach;
      }
    }
    else if(strcmp(argv[i],"-d") == 0)
    {
      ++i;
      if(i >= argc)
      {
        ErrorMsgBox("No data after -d.","Improper parameters");
        goto beach;
      }
      else lpProcWorkingDir = argv[i];
    }
  }

  /* prepare the STARTUPINFO structure. in our case, it is desired to hide
   * the window of the launched process. */
  ZeroMemory(&sProcStartupInfo, sizeof(sProcStartupInfo));
  sProcStartupInfo.cb = sizeof(sProcStartupInfo);
  sProcStartupInfo.dwFlags = STARTF_USESHOWWINDOW;
  sProcStartupInfo.wShowWindow = SW_HIDE;

  ZeroMemory(&sProcInformation, sizeof(sProcInformation));

  /* create the process by specifying its commandline. the process is created
   * as a suspended one. it is later resumed with ResumeThread() after setting
   * its affinity. therefore, the process will not start its execution until
   * the desired affinity is set. */
  if(CreateProcess(NULL, szProcCmdLine, NULL, NULL, FALSE,
                   CREATE_SUSPENDED, NULL, lpProcWorkingDir, &sProcStartupInfo,
                   &sProcInformation) == 0)
  {
    szLastErrorMsg = win_strerror("CreateProcess()");
    ErrorMsgBox(szLastErrorMsg, "WinAPI call failed");
    HeapFree(GetProcessHeap(),0,szLastErrorMsg);
    return 1;
  }

  /* set the affinity mask for the process. */
  if(SetProcessAffinityMask(sProcInformation.hProcess, dwNewProcessAffinity)
      == 0)
  {
    szLastErrorMsg = win_strerror("SetProcessAffinityMask()");
    ErrorMsgBox(szLastErrorMsg, "WinAPI call failed");
    HeapFree(GetProcessHeap(),0,szLastErrorMsg);

    if(TerminateProcess(sProcInformation.hProcess, 255) == 0)
    {
      szLastErrorMsg = win_strerror("TerminateProcess()");
      ErrorMsgBox(szLastErrorMsg, "WinAPI call failed");
      HeapFree(GetProcessHeap(),0,szLastErrorMsg);
    }
    goto beach2;
  }

  /* and resume (actually, start) its execution. */
  if(ResumeThread(sProcInformation.hThread) == -1)
  {
    szLastErrorMsg = win_strerror("ResumeThread()");
    ErrorMsgBox(szLastErrorMsg, "WinAPI call failed");
    HeapFree(GetProcessHeap(),0,szLastErrorMsg);

    if(TerminateProcess(sProcInformation.hProcess, 255) == 0)
    {
      szLastErrorMsg = win_strerror("TerminateProcess()");
      ErrorMsgBox(szLastErrorMsg, "WinAPI call failed");
      HeapFree(GetProcessHeap(),0,szLastErrorMsg);
    }
    goto beach2;
  }

  /* process launched successfully. */
  rv = 0;

beach2:
  /* close handles to the created process and its main thread, and exit. */
  CloseHandle(sProcInformation.hProcess);
  CloseHandle(sProcInformation.hThread);

beach:
  HeapFree(GetProcessHeap(),0,argv);
  return rv;
}