/* To produce proper traces use the following Unix command and opts :   strace -tt -e trace=open -o trace.txt ./Driver [args] */ 

#include <iomanip>
#include <stdlib.h>
#include <fstream>
#include <stdio.h>
#include <algorithm>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "Driver.h"
#include "FS_Simulator.h"
#include "Cache_Manager.h"
#include "Probability_Graph.h"


#define MAX_TRACE_CALLS 10000

using namespace std;

/* used for the sorting function call */
bool myfunction (SystemCall *first, SystemCall *second) 
{ 
	if( first->hourTime < second->hourTime)
		return true; 
	else if( first->hourTime > second->hourTime)
		return false;
	else
	{
		if( first->minuteTime < second->minuteTime )
			return true;
		else if( first->minuteTime > second->minuteTime)
			return false;
		else
		{
			if(first->secondTime < second->secondTime )
				return true;
			else if( first->secondTime > second->secondTime)
				return false;
			else
			{
				if( first->microSecondTime < second->microSecondTime)
					return true;
				else
					return false;
			}
		}

	} 

}

string TraceLoader::getData()
{

	/* create an input stream to collect the trace data into a single string array */
	ifstream in( traceFile.c_str() );
	string data = "", buffer;
	while( ! in.eof() )
	{
		getline( in, buffer);
		data += buffer;
		data += "\n";
	}	
	return data;
}

/* load System Calls into calls set */
void TraceLoader::parse(string traceData)
{
	/* break down the long string into single lines using a tokenizer */
	char * pch;
	pch = strtok ((char *)traceData.c_str(), "\r\n");

	/* create a string vector to hold each line in the trace */
	vector<string> sysCalls;

	while( pch != NULL)
	{
		sysCalls.push_back(&pch[0]);
		pch = strtok (NULL, "\r\n");
	}

	/* use this data to build our SystemCall struct Array */
	for( int i = 0; i < sysCalls.size(); i++)
	{
		string currLine = sysCalls.at(i);
		pch = strtok ((char *)currLine.c_str(), "=:, ()\"");
		
		vector<string> callFields;
		while( pch != NULL )
		{
			callFields.push_back( &pch[0] );
			pch = strtok (NULL, "=:, ()\"");
		}
		
		/* with these fields create our struct */		
		SystemCall *newCall = new SystemCall;
		newCall->callType = callFields[4]; 
		/* make sure it is not the quit call that we add */
		if( callFields[4] == "+++" || callFields[4] == "---")
			continue;
		newCall->file = callFields[5];
		newCall->streamID = atoi( callFields[ callFields.size() - 1].c_str() );

		/* get total size in bytes bytes and inode number */
		int open_stat, f_stat;
		open_stat = open( newCall->file.c_str(), O_RDONLY);
		struct stat buff;
		if(open_stat >= -1)
			f_stat = fstat(open_stat, &buff);
		
		if(f_stat >= 0)
			newCall->bytes = buff.st_size;
		else
			newCall->bytes = 512;

		newCall->hourTime = atoi( callFields[1].c_str() );
		newCall->minuteTime = atoi( callFields[2].c_str() );
		/* break up the seconds and the milliseconds */
		int index = callFields[3].find_first_of(".");
		newCall->secondTime = atoi( callFields[3].substr(0, index).c_str() );
		newCall->microSecondTime = atoi ( callFields[3].substr( index + 1, callFields[3].length() ).c_str() );
		
		calls.push_back(newCall);		 
	}	
}

/* load System Calls into calls set */
void TraceLoader::parse_seers(string traceData)
{
	/* break down the long string into single lines using a tokenizer */
	char * pch;
	pch = strtok ((char *)traceData.c_str(), "\r\n");

	/* create a string vector to hold each line in the trace */
	vector<string> sysCalls;

	while( pch != NULL)
	{
		sysCalls.push_back(&pch[0]);
		pch = strtok (NULL, "\r\n");
	}

	/* use this data to build our SystemCall struct Array */
	for( int i = 0; i < sysCalls.size(); i++)
	{
		string currLine = sysCalls.at(i);
		pch = strtok ((char *)currLine.c_str(), "=:, ()\"");
		
		vector<string> callFields;
		while( pch != NULL )
		{
			callFields.push_back( &pch[0] );
			pch = strtok (NULL, "=:, ()\"");
		}
		
		/* with these fields create our struct */		
		SystemCall *newCall = new SystemCall;
		newCall->callType = callFields[8]; 
		/* make sure it is not the quit call that we add */
		if( callFields[8] == "+++" || callFields[8] == "---")
			continue;
		newCall->file = callFields[9];
		newCall->streamID = atoi( callFields[ callFields.size() - 1].c_str() );

		/* get total size in bytes bytes and inode number */
		newCall->bytes = atoi( callFields[11].c_str());
		if( newCall->bytes == 0)
			newCall->bytes = 512;

		/* seconds and microseconds */
		int index = callFields[7].find_first_of(".");
		long first_part = atol( callFields[7].substr(0, index).c_str() );
		newCall->microSecondTime = atoi ( callFields[7].substr( index + 1, callFields[7].length() ).c_str() );
		
		int hours = first_part/3600;
		first_part -= hours*3600;

		int minutes = first_part/60;
		first_part -= minutes*60;

		newCall->hourTime = hours%24;
		newCall->minuteTime = minutes%60;
		newCall->secondTime = first_part;


		/* insert this into our set graph*/				
		calls.push_back(newCall);
		 
	}	
}

int main( int argc, char *argv[])
{
	/* parse the command line args */
	if( argc < 5 )
	{
		cout << "Error: need 6 args! ./Driver [test file] [cache-size] [minimum chance] [lookahead window] [prefetch option]";
		return 0;
	}
	
	string prefetch_arg = argv[5];
	
	/* prefetch / yes or no */
	bool prefetch_option = false;
	if( prefetch_arg.compare("true") == 0 )
		prefetch_option = true;	
	

	/* create our Cache_Manager */
	Cache_Manager cache_manager(prefetch_option, atoi(argv[2]), atof(argv[3]), atoi(argv[4]) );
	Cache_Manager *ptr = &cache_manager;
	
	/* create our FS_Simulator */
	FS_Simulator fs_sim(ptr);
	
	/* use TraceLoader to load our simulation data */
	TraceLoader test( argv[1] );
	string data = test.getData();
	test.parse(data); // produces a vector of SystemCalls ordered by time ( microseconds )

	/* Simulate Application system calls */

	Timestamp now;
	now.stamp();
	double long previous_time = now.time;
	SystemCall* previous_call = *test.calls.begin();
	vector<SystemCall*>::iterator it = test.calls.begin();
	++it;



	/* convert each to seconds */
	long double previous_call_time = 3600*previous_call->hourTime; 
	previous_call_time += 60*previous_call->minuteTime;
	previous_call_time += previous_call->secondTime;
	previous_call_time += (double)0.000001*(previous_call->microSecondTime);

	long double current_call_time = 3600*(*it)->hourTime; 
	current_call_time += 60*(*it)->minuteTime;
	current_call_time += (*it)->secondTime;
	current_call_time += (double)0.000001*((*it)->microSecondTime);

	long double elapsed_goal = current_call_time - previous_call_time;

	if(elapsed_goal < 0 )
		elapsed_goal = 0.05; //seconds

	if(elapsed_goal > 5)
		elapsed_goal = 0.05; // wait one second if it is like an hour or day or minute- we dont want to wait that long
	while(it != test.calls.end() )
	{
		now.stamp();
		if((now.time - previous_time) - elapsed_goal > -0.00001) {
			systemCallToString( **it );
			fs_sim.sendRequest( *it );
			previous_call = *it;
			previous_time = now.time;
			previous_call_time = 0.0;
			current_call_time = 0.0;
			/* convert each to seconds */
			previous_call_time += 3600*previous_call->hourTime; 
			previous_call_time += 60*previous_call->minuteTime;
			previous_call_time += previous_call->secondTime;
			previous_call_time += (double)0.000001*(previous_call->microSecondTime);
			it++;
			current_call_time += 3600*(*it)->hourTime; 
			current_call_time += 60*(*it)->minuteTime;
			current_call_time += (*it)->secondTime;
			current_call_time += (double)0.000001*((*it)->microSecondTime);

			elapsed_goal = current_call_time - previous_call_time;

			if(elapsed_goal < 0 )
				elapsed_goal = 0.05; //seconds

			if(elapsed_goal > 0.05)
				elapsed_goal = 0.05;
		}			
	} // end while

	

	
	return 0;
}
