#ifndef FS_Simulator_H
#define FS_Simulator_H

#include <iostream>
#include <string.h>
#include <vector>
#include "Driver.h"
#include "Cache_Manager.h"


using namespace std;

class FS_Simulator
{

	private :
	SystemCall *currentCall;
	Cache_Manager *cache_manager;

	public :
	FS_Simulator(Cache_Manager*);

	/* receive a request from the Driver */
	void rcvRequest(SystemCall*);

	/* send a request to the cache manager */
	bool  sendRequest(SystemCall*);

};


/* constructor */
FS_Simulator::FS_Simulator(Cache_Manager *tmp)
{
	cache_manager = tmp; 	
}



/* receive a request from the Driver */
void FS_Simulator::rcvRequest(SystemCall *call)
{
	currentCall = call;

}

bool FS_Simulator::sendRequest(SystemCall *request)
{
	/* send the request to the cache manager to prefetch/cache/or deny request */
	bool result = cache_manager->allocate(request);

	cache_manager->cacheToString();
	cout << endl << endl;
	return result;
}

#endif
