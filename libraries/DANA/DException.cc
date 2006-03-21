///
/// DException.cc - implementation of DANA exceptions with rudimentary stack 
/// traces. See DException.h for advice regarding usage.
/// 
/// Author: Craig Bookwalter (craigb at jlab.org)
/// Date:   March 2006
///

#include "DException.h"

DException::DException(std::string msg) {
	void* traces[25];
	FILE* psOutput;
	FILE* addr2lineOutput;
	FILE* whichOutput;
	std::stringstream sstemp; 
	std::string path;
	std::string prim_loc = "";
	int nLevels;
	char temp_ch;
	char* myName = new(std::nothrow) char [NAME_MAX];	// NAME_MAX is defined in limits.h
	bool firstLine = true;
	bool newlineEncountered = false;
	nLevels = backtrace(traces, 25);

	// if nLevels == size of the array, it's possible that the depth of
	// the stack exceeds the size of the array, and you won't get a full trace.
	if (nLevels == 25) {}
		// Put in global error log message here. 
	
	// if myName == NULL, the new(nothrow) above failed to allocate memory.
	if (!myName) {}
		// Put in global error log message here
	
	// All of this output is subject to redirection to error logs.	
	if (msg != "")
		std::cout	<< "Exception (\"" << msg << "\") caught at:"
					<< std::endl;
	else
		std::cout 	<< "Exception caught at:" << std::endl;
	
	
	// First, we need to get the name of this executable. Apparently, there is no 
	// system call for this. So, the man page for "ps" says:
	// Print only the name of PID 42:
    // $> ps -p 42 -o comm=
    
	sstemp << "ps -p " << getpid() << " -o comm=" << std::endl;
	psOutput = popen(sstemp.str().c_str(), "r");
	for (int i=0; ((temp_ch = getc(psOutput)) != EOF) || i > NAME_MAX; i++) 
		myName[i] = temp_ch;
	myName[strlen(myName)-1] = '\0';
	pclose(psOutput);
	sstemp.str("");
	
	
	// Now, we have to figure out the path of the executable, using "which" and 
	// a few shenanigans with environment variables. Note that if you're doing
	// something funny on the command line, you'll likely fool this algorithm.
	
	sstemp << getenv("PATH");
	path = sstemp.str();
	sstemp.str("");
	sstemp << "PATH=.:" << path; 
	setenv("PATH",sstemp.str().c_str(),1);
	sstemp.str("");
	sstemp << "which " << myName << std::endl;
	whichOutput = popen(sstemp.str().c_str(), "r");
	for (int i=0; ((temp_ch = getc(whichOutput)) != EOF) || i > NAME_MAX; i++)
		myName[i] = temp_ch;
	
	myName[strlen(myName)-1] = '\0';
	pclose(whichOutput);
	sstemp.str("");


	// Next, we ask "addr2line" of the GNU binary utilities package for 
	// information about the names of the addresses we got from backtrace().

	sstemp << "addr2line -e " << myName << " ";	
		
	for (int i=0; i < nLevels; i++) 
		sstemp << traces[i] << " ";

	addr2lineOutput	= popen(sstemp.str().c_str(), "r");
	sstemp.str("");
	
	// Pretty up the output.
		
	for (int i=0; ((temp_ch = getc(addr2lineOutput)) != EOF); i++) {
		if (temp_ch == '?')
			break;
		if (newlineEncountered) {
			newlineEncountered = false;
			sstemp << '\t';
		}
		if (temp_ch == '\n') {
			// Skip the first line, because it always references this file.
			if (firstLine) {  
				firstLine = false;
				sstemp.str("");
				continue;
			}
			else if (prim_loc == "") { 
				prim_loc = sstemp.str();
				sstemp.str("");
				continue;
			}
			else 
				newlineEncountered = true;	
		}
		sstemp << temp_ch;
	}
	
	std::cout 	<< "\t" << prim_loc << std::endl;
	std::cout 	<< "referenced by: " << std::endl 
				<< "\t" << sstemp.str(); 
	std::cout.flush();
	
	delete[] myName; 
	pclose(addr2lineOutput);
	setenv("PATH", path.c_str(), 1);
}
