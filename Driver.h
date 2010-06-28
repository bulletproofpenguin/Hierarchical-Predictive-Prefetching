#ifndef Driver_H
#define Driver_H

#include <iostream>
#include <string>
#include <vector>
#include <set>
#include <math.h>
#include <iomanip>

using namespace std;

/* System Call Struct */
struct SystemCall{

  string callType;
  int    streamID;
  string file;
  short  hourTime;
  short  minuteTime;
  int  secondTime;
  long   microSecondTime;
  long   bytes;

  /* used to calculate locality of reference */
  double access_latency;
  double stability;

  /* overloaded = operator */
  const SystemCall& operator=(const SystemCall &rhs)
	{
		 if (this != &rhs)    
		 {  
			(*this).callType = rhs.callType;
			(*this).streamID = rhs.streamID;
			(*this).file = rhs.file;
			(*this).hourTime = rhs.hourTime;
			(*this).minuteTime = rhs.minuteTime;
			(*this).secondTime = rhs.secondTime;
			(*this).microSecondTime = rhs.microSecondTime;	
			(*this).bytes = rhs.bytes;
		 }
		
     		return *this;
		
	}

   /* overloaded == operator */

   bool operator==(const SystemCall &other) const {
    if( (*this).file.compare( other.file ) == 0 && (*this).callType.compare("open") == 0 && other.callType.compare("open") == 0 )
		return true;
	else
		return false;
  }

   /* overloaded != operator */
   bool operator!=(const SystemCall &other) const {
    return !(*this == other);
  }
};


/* first based on time */
bool operator<(const SystemCall &lhs, const SystemCall &rhs)
{
		if( lhs.hourTime < rhs.hourTime)
			return true;
		else if( lhs.hourTime > rhs.hourTime)
			return false;
		else
		{
			if( lhs.minuteTime < rhs.minuteTime )
				return true;
			else if( lhs.minuteTime > rhs.minuteTime)
			return false;
			else
			{
				if( lhs.secondTime < rhs.secondTime ) 
					return true;
				else if( lhs.secondTime > rhs.secondTime )
					return false;
				else
				{
					if( lhs.microSecondTime < rhs.microSecondTime)
						return true;
					else
						return false;
			
				}
			}
		}
}


/* System Call Comparison Struct */

struct systemCallComparison {
  bool operator() (const SystemCall* lhs, const SystemCall* rhs) const
  {	 return  *lhs<*rhs; }

};

  

void systemCallToString(SystemCall call)
{
	cout << "Call: " << call.callType << endl;
	cout << "StreamID: " << call.streamID << endl;
	cout << "File: " << call.file << endl;
	cout << "Hour: " << call.hourTime << endl;
	cout << "Minute: " << call.minuteTime << endl;
	cout << "Second: " << call.secondTime << endl;
	cout << "Microsecond: " << call.microSecondTime << endl;
	cout << "Bytes: " << call.bytes << endl ;
}


/* Parser that will create the System Calls from the trace data */
class TraceLoader
{
	private:
	string traceFile;
	public:
	vector<SystemCall*> calls;
	/* constructors */
	TraceLoader(string trfile)
	{ traceFile = trfile; }
	/* function to load trace data */
	string getData();
	/* function to parse the trace data to create SystemCall structs */
	void parse(string);
	
	void parse_seers(string);
	
	
};

/* returns difference in seconds */
long double operator-(const SystemCall &lhs, const SystemCall &rhs)
{

	long double leftSeconds = 0000000.000000;
	
	long double rightSeconds = 0000000.000000;

	leftSeconds += (lhs.hourTime)*3600;
	leftSeconds += (lhs.minuteTime)*60;
	leftSeconds += lhs.secondTime;
	leftSeconds += (double)(lhs.microSecondTime)*0.000001;
	
	rightSeconds += (rhs.hourTime)*3600;
	rightSeconds += (rhs.minuteTime)*60;
	rightSeconds += rhs.secondTime;
	rightSeconds += (double)(rhs.microSecondTime)*0.000001;
	if(leftSeconds >= rightSeconds)
		return ( leftSeconds - rightSeconds );
	else
	{
		rightSeconds = 86400.000000 - rightSeconds;
		return ( leftSeconds + rightSeconds);
	} 	


}

SystemCall operator+(SystemCall lhs, double addTime) // adding seconds
{	
	int hours = addTime / 3600;
	int minutes = (addTime - (hours*3600) )/60;
	int seconds = ( addTime - (hours*3600) - (minutes*60) );
	addTime -= seconds;
	addTime *= 1000000; 
	lhs.hourTime = ( lhs.hourTime + hours )%24;

	

	
	if( ((lhs.microSecondTime + (int)addTime)%1000000) < lhs.microSecondTime)
	{
		
		lhs.secondTime = (++lhs.secondTime)%60;
		if( ((lhs.secondTime + seconds)%60) < lhs.secondTime )
		{
			/* increment minuteTime */
			lhs.minuteTime = (++lhs.minuteTime)%60;
			/* increment the hourTime */
			if(  ((lhs.minuteTime + minutes )%60) < lhs.minuteTime )
				lhs.hourTime = (++lhs.hourTime)%24;
			
		}
	}
	lhs.microSecondTime = (lhs.microSecondTime + (int)addTime)%1000000;
	lhs.secondTime = (lhs.secondTime + seconds)%60;
	lhs.minuteTime = (lhs.minuteTime + minutes)%60;	
	return lhs;
	 
}
#endif
