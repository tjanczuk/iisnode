#ifndef __CNODESTACKTRACE_H__
#define __CNODESTACKTRACE_H__

#include <DbgHelp.h>
#include <Windows.h>
#include <iostream>

using namespace std;

class StackTraceWin 
{
public:
	StackTraceWin() 
	{
		// Initializes the symbols for the process.
		// Defer symbol load until they're needed, use undecorated names, and
		// get line numbers.
		SymSetOptions(SYMOPT_DEFERRED_LOADS |
			SYMOPT_UNDNAME |
			SYMOPT_LOAD_LINES);
		if (!SymInitialize(GetCurrentProcess(), NULL, TRUE)) {
			// TODO(awong): Handle error: SymInitialize can fail with
			// ERROR_INVALID_PARAMETER.
			// When it fails, we should not call debugbreak since it kills the current
			// process (prevents future tests from running or kills the browser
			// process).
			cout << "SymInitialize failed: " << dec << GetLastError() << endl;
		} 
		else
		{
			// When walking our own stack, use CaptureStackBackTrace().
			_count = CaptureStackBackTrace(0, sizeof(_trace), _trace, NULL);
		}
	}


	// For the given trace, attempts to resolve the symbols, and output a trace
	// to the ostream os.  The format for each line of the backtrace is:
	//
	//    <tab>SymbolName[0xAddress+Offset] (FileName:LineNo)
	//
	// This function should only be called if Init() has been called.  We do not
	// LOG(FATAL) here because this code is called might be triggered by a
	// LOG(FATAL) itself.

	template <class Stream>
	void OutputTraceToStream(const void* const* trace, size_t count, Stream* os) 
	{
		for (size_t i = 0; (i < count) && os->good(); ++i) {
			const int kMaxNameLength = 256;
			DWORD_PTR frame = reinterpret_cast<DWORD_PTR>(trace[i]);

			// Code adapted from MSDN example:
			// http://msdn.microsoft.com/en-us/library/ms680578(VS.85).aspx
			ULONG64 buffer[
				(sizeof(SYMBOL_INFO) +
					kMaxNameLength * sizeof(wchar_t) +
					sizeof(ULONG64) - 1) /
					sizeof(ULONG64)];
				memset(buffer, 0, sizeof(buffer));

			// Initialize symbol information retrieval structures.
			DWORD64 sym_displacement = 0;
			PSYMBOL_INFO symbol = reinterpret_cast<PSYMBOL_INFO>(&buffer[0]);
			symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
			symbol->MaxNameLen = kMaxNameLength - 1;
			BOOL has_symbol = SymFromAddr(GetCurrentProcess(), frame,
				&sym_displacement, symbol);

			// Attempt to retrieve line number information.
			DWORD line_displacement = 0;
			IMAGEHLP_LINE64 line = {};
			line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
			BOOL has_line = SymGetLineFromAddr64(GetCurrentProcess(), frame,
				&line_displacement, &line);

			// Output the backtrace line.
			(*os) << "\t";
			if (has_symbol) {
				(*os) << symbol->Name << " [0x" << trace[i] << "+"
					<< sym_displacement << "]";
			} else {
				// If there is no symbol informtion, add a spacer.
				(*os) << "(No symbol) [0x" << trace[i] << "]";
			}
			if (has_line) {
				(*os) << " (" << line.FileName << ":" << line.LineNumber << ")";
			}
			(*os) << "\n";
		}
	}

	template <class Stream>
	void OutputToStream(Stream* os) 
	{
		(*os) << "Backtrace:\n";
		OutputTraceToStream(_trace, _count, os);
	}

private:
	// From http://msdn.microsoft.com/en-us/library/bb204633.aspx,
	// the sum of FramesToSkip and FramesToCapture must be less than 63,
	// so set it to 62. Even if on POSIX it could be a larger value, it usually
	// doesn't give much more information.
	static const int kMaxTraces = 62;

	void* _trace[kMaxTraces];
	int _count;
};

#endif