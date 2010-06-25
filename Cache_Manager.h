/* Assume a 64 bit bus size */

#ifndef Cache_Manager_H
#define Cache_Manager_H

#include <sys/time.h>
#include <iomanip>
#include <math.h>
#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <set>
#include <string>
#include <utility>
#include "Driver.h"
#include "Probability_Graph.h"

#define BLOCK_SIZE 512 // bytes

#define DOUBLE_ZERO 0.0000000000001

/* define all times in microseconds */ 
/* simulate a vendors hardware */

#define RAM_LATENCY 0.05
#define SSD_LATENCY 100 
#define RAID_LATENCY 10000
#define TAPE_LATENCY 10000000

/* define bandwidth in megabytes / second */

#define RAM_BITRATE 80
#define SSD_BITRATE 50
#define RAID_BITRATE  100
#define TAPE_BITRATE 6 


/* conigurable delay variables */

#define t_disk 1000 // storage disk/drive latency
#define t_cpu  100 // inter-access CPU time of application ( 1000 system calls per second average )
#define t_hit  50 // time to read a several kilobytes from cache
#define t_driver 500 // time to allocate new pages ???????????????????????????

/* formulas for cost/benefit analysis in microseconds */

#define prefetch_horizon (t_disk/(t_cpu + t_hit + t_driver) ) // number of simultaneous prefetches to pipeline
#define prefetch_ttl t_disk + t_cpu  //in microseconds 


/* Tunable variables for Weigted Moving Averages */

#define gamma 0.25 // for the weighted moving cache and prefetch hit ratio

using namespace std;


/* global variables */
int lookahead_window;
int assoc_count;
Probability_Graph *graph;

/* represent a storage disk */
struct Disk
{
	double access_delay; //microseconds
	double locality_min;
	double locality_max;
};


struct Timestamp
{
	double long time;
	void stamp()
	{
		timeval now;
       	        gettimeofday(&now, 0);
		/* seconds since Jan 1970 */
		time =  now.tv_sec + (now.tv_usec*.000001);	
	}
	void prefetchStamp()
	{
		time = time - (double)t_disk*0.000001;
	}
};

bool operator<( const Timestamp& lhs, const Timestamp& rhs)
{ return ( lhs.time < rhs.time ); }

struct Page
{
	Timestamp timestamp;
	SystemCall *file;
	int block_num;
		
	bool operator==(const Page &other) const {
    	if( (*this).file->file.compare(other.file->file) == 0 && (*this).block_num == other.block_num )
		return true;
	else
		return false;
  	}	

	bool operator!=(const Page &other) const {
    	if( *(*this).file != *other.file || (*this).block_num != other.block_num )
		return true;
	else
		return false;
  	}

}; 

struct pageComparison {
  bool operator() (const Page& lhs, const Page& rhs) const
  {	 

	 /* for the same file name */
	 if( lhs.file->file == rhs.file->file )
	 {
		if( lhs.block_num < rhs.block_num )
			return true;
		else
			return false;	
	 }
	 /* for different file names */
	 else
		return (lhs.timestamp.time < rhs.timestamp.time);
  }

};

struct Cache
{
	set< Page, pageComparison> buffer;
	long capacity; // pages
	long pages_available; // pages
	int hit_count, miss_count;
	double last_hit_ratio;
	double delta_ratio;

	/* check to see if BLOCK is in our buffer */
	bool isCached( Page page)
	{
		if(buffer.count( page ))
			return true;
		else
			return false;
	}

	/* insert a page into the buffer */
	pair<bool, Page> insert ( Page page )
	{
		pair<bool, Page> return_val;
		/* insert the page into the cache */
		pair< set<Page>::iterator, bool> result;
		result = buffer.insert(page);
		return_val.first = result.second;
		return_val.second = *result.first;
		return return_val;
	}	
	/* get the weighted cache hit ratio */
	double update_hit_ratio()
	{
		cout << "Updating Last Hit Ratio..." << endl;
		if( hit_count == 0)
			return 0;
		else {
			double current = ((double)hit_count / (hit_count + miss_count));
			last_hit_ratio =  ((double)(1 - gamma)*last_hit_ratio) + (double)gamma*current;
		}
	}
	double get_current_hit_ratio()
	{
		if( hit_count == 0 )
			return 0;
		else {
			double current = ((double)hit_count / (hit_count + miss_count));
			return ((double)(1 - gamma)*last_hit_ratio) + (double)gamma*current;
		}
	}
	double get_last_hit_ratio()
	{
		return last_hit_ratio;
	}
};

struct Prefetch
{
	set< Page, pageComparison > buffer;
	long capacity; // pages
	long pages_available; // pages
	int hit_count, miss_count;
	double last_hit_ratio;
	bool isPrefetched( Page page)
	{
		if( buffer.count(page))
			return true;
		else
			return false;
		
	}
	/* insert a page into the buffer */
	pair<bool, Page> insert ( Page page )
	{
		pair<bool, Page> return_val;
		/* insert the page into the cache */
		pair< set<Page>::iterator, bool> result;
		result = buffer.insert(page);
		return_val.first = result.second;
		return_val.second = *result.first;
		return return_val;
	}	

	/* get the weighted cache hit ratio */
	double update_hit_ratio()
	{
		if( hit_count == 0 )
			return 0;
		else {
			double current = ((double)hit_count / (hit_count + miss_count));
			last_hit_ratio =  ((double)(1 - gamma)*last_hit_ratio) + (double)gamma*current;
		}
	}
	double get_current_hit_ratio()
	{
		if( hit_count == 0 )
			return 0;
		else {
			double current = ((double)hit_count / (hit_count + miss_count));
			return ((double)(1 - gamma)*last_hit_ratio) + (double)gamma*current;
		}
	}
	double get_last_hit_ratio()
	{
		return last_hit_ratio;
	}
};

/* window to keep a short ( lookahead period ) history of system calls for dynamic graph updates */
struct CallWindow
{
	/* duplicates not allowed here */
	set <SystemCall*, systemCallComparison > calls;
	void insert( SystemCall *call)
	{
		/* make this SystemCall an Association object */
		Association *assoc = new Association;
		assoc->call = call;
		assoc->strength = 1;
		Node *check = graph->find( call );
		/* append the SystemCall to the end of the set [b/c ordered temporally] */
		if ( check == NULL )
		{
			/* add it to our graph */
			Node *new_node = new Node;	
			new_node->call = call;
			new_node->total_strength = 0;
			graph->nodes.push_back( *new_node ); 
			if( calls.size() > 1 )
			{
				
				/* update everything inside the association window that comes before it */
				for( set<SystemCall*>::iterator it = calls.begin(); it != calls.end(); it++)
				{
					/* add the association for this new call to each node in the window */
					Node *ptr = graph->find( *it );
					ptr->window.push_back( *assoc );
					ptr->total_strength++;
					assoc_count+= ptr->window.size();
					graph->remove_dups(ptr->window);
				}
			}
			
			/* insert it into our window */
			calls.insert ( call );
		}
		/* the system call already exists in our graph */
		else
		{
			if( calls.size() > 1 )
			{
				/* update everything in our association graph that comes before it */
				for( set<SystemCall*>::iterator it = calls.begin(); it != calls.end(); it++)
				{
					/* add the association for this new call to each node in the window */
					Node *ptr = graph->find( *it );
					if( *ptr->call != *assoc->call )
					{
						ptr->window.push_back( *assoc );
						ptr->total_strength++;
					}
					/* remove duplicate associations */
					graph->remove_dups( ptr->window );
				}
			}
			/* keep the data from the old System Call */
			Node new_node;
			new_node.call = call;
			new_node.total_strength = check->total_strength;
			/* add the stuff from the association window */
			new_node.window.swap( check->window );
			/* remove the old node from the graph */
			for( vector<Node>::iterator it = graph->nodes.begin(); it != graph->nodes.end(); it++)
			{
				/* if the node's System Call == the system call being inserted */
				if( (*it).call->file == call->file ) 
				{
					graph->nodes.erase(it);
					break;
				}
			}				
			/* add the new one to graph and call window */
			graph->nodes.push_back(new_node);
			calls.insert(call);
			
		}
		/* trim the size of the window to lookahead period */ 
		set<SystemCall*>::iterator start = calls.begin();
		set<SystemCall*>::iterator end = calls.end();
		--end;
		if( calls.size() > 1 )
		{
			while( (*(*end)) - (*(*start)) > (lookahead_window*0.000001) ) 
			{
				calls.erase( start );
				start = calls.begin();
			}
		}  

	} // end insert
};


class Cache_Manager
{

	private:
	
	/* general variables */
	Timestamp last_clock_time;

	/* top level storage */
	bool prefetching;
	double minimum_chance;
	long   total_pages;
	Cache cache;
	Prefetch prefetched;
	CallWindow call_window;

	/* rest of the storage */
	Disk disk_stack[4];	

	public:
	/* constructor - param: true -> prefetching / false -> no prefetching */
	Cache_Manager(string, bool, long, double, int);
	/* default constructor */
	Cache_Manager();
	/* allocate memory to a file */
	bool allocate(SystemCall*); 
	bool lruAllocate( SystemCall*, bool);
	bool prefetchAllocate(Page);
	/* print cache to screen */
	void cacheToString();
	/* prefetching function */
	void prefetch(SystemCall*);
	/* function to update hit ratios */
	void updateHitRatios();
	/* utility functions to check for pipelining availability */
	void pipeline(Node*);
	bool matrix_check(Node*, int, int);

	/* resize prefetch and cache buffers according to current coniditions */
	void repartitionBuffers();
			
};


/* constructors */
Cache_Manager::Cache_Manager()
{
	prefetching = false;
}

Cache_Manager::Cache_Manager(string trace_file, bool tmp, long size_in_bytes, double minChance, int lookahead)
{
	/* initialize parameters */
	prefetching  = tmp;
	minimum_chance = minChance;
	lookahead_window = lookahead;

	/* initialize variables */
	cache.hit_count = 0;
	cache.miss_count = 0;
	cache.last_hit_ratio = 0;
	prefetched.hit_count = 0;
	prefetched.miss_count = 0;
	prefetched.last_hit_ratio = 0;
	assoc_count = 0;

	/* initialize clock */
	last_clock_time.stamp();	

	/* load our trace data */
	TraceLoader traceLoader(trace_file);
	string data = traceLoader.getData();
	traceLoader.parse(data); // fill calls set

	/* load our probability graph */
	graph = new Probability_Graph(traceLoader.calls, lookahead_window);
	graph->createNodes();
	graph->loadAssociations();

	total_pages = size_in_bytes/BLOCK_SIZE;	
	/* initialize buffers */
	if( prefetching ) {
		prefetched.capacity = prefetch_horizon;
		prefetched.pages_available = prefetched.capacity;
		cache.capacity = total_pages - prefetched.capacity;
		cache.pages_available = cache.capacity;
	}
	else {
		cache.capacity = total_pages;
		cache.pages_available = cache.capacity;
	}

	/* initialize disks */
	Disk RAM;
	RAM.access_delay = RAM_LATENCY;

	Disk SSD;
	SSD.access_delay = SSD_LATENCY;

	Disk RAID;
	RAID.access_delay = RAID_LATENCY;
	
	Disk TAPE;
	TAPE.access_delay = TAPE_LATENCY;
}



bool Cache_Manager::allocate( SystemCall *file)
{

	cout << file->file << endl;
	/* update hit ratios for weighted moving averages */
	updateHitRatios();

	/* LRU Management */
	if( !prefetching )
	{
		return lruAllocate( file, false );
	}
	/* LRU with prefetching */
	else
	{
		/* add the call to our call window and dynamically update the probability graph */
		call_window.insert( file );
		/* prefetch calls that are in the lookahead window */
		prefetch( file );
		
					
		/* delete all blocks with this file name from the prefetch buffer */
		bool found = false;
		bool isLoaded = false;
		bool isPrefetched = false;
		Timestamp now;
		now.stamp();
		for( set<Page>::iterator it = prefetched.buffer.begin(); it != prefetched.buffer.end(); it++)
		{
			if( *file ==  *(*it).file)
			{
				prefetched.buffer.erase(it);
				prefetched.pages_available++;
				found = true;

				if( (now.time - (*it).timestamp.time) >= (double)t_disk*0.000001 )
					isLoaded = true;

			}
			/* this works because all the cached blocks for a file are sequential */
			if( found && !(*file ==  *(*it).file)  )
				break;
		}
		if( found && isLoaded )
		{
			prefetched.hit_count += ceil(file->bytes/BLOCK_SIZE);
			isPrefetched = true;
		}
		else
			prefetched.miss_count += ceil(file->bytes/BLOCK_SIZE);
			
		
		/* put the file into the cache because it has been called */		
		lruAllocate( file, isPrefetched );
						
	} 

}
/* Precondition : the SystemCall is not in the Cache buffer */
bool Cache_Manager::lruAllocate( SystemCall *file, bool isPrefetched )
{
	
	/* get the number of pages required by the file */
	long pages_required =  ceil( ((double)(file->bytes)/ BLOCK_SIZE) );
	/* result of insert operation */
	pair<bool, Page> result;
	
	/* NOT ENOUGH MEMORY */
	if( pages_required > cache.pages_available )
	{
		/* employ LRU management */
		for( int i = 0; i < pages_required; i++ )
		{
			Timestamp now;
			now.stamp();
			/* create our fake page to be inserted at the tail of the set */
			Page new_page;
			new_page.file = file;
			if( !isPrefetched )
				new_page.timestamp.stamp();
			else
				new_page.timestamp.prefetchStamp();
			new_page.block_num = i+1;
			
			/* insert a page into free cache memory ( at the tail ) */
			if( cache.pages_available )
			{
				result = cache.insert( new_page ); // unless it already exists
				if( !result.first && (now.time - result.second.timestamp.time ) >= (double)t_disk*0.000001)
					cache.hit_count++;
				else
					cache.miss_count++;
				cache.pages_available = cache.capacity - cache.buffer.size();		
			}
			
			/* deallocated memory at the head of the set and add a page */
			else
			{
				if( !prefetching )
				{
					/* deallocate LRU page */
					set<Page>::iterator it = cache.buffer.begin();
					cache.buffer.erase( it );
					/* insert a page */
					result = cache.insert( new_page );
					/* make sure t_disk time has elapsed before it appears in the buffer */
					if( !result.first && (now.time - result.second.timestamp.time ) >= (double)t_disk*0.000001 )
					{
						
						cache.hit_count++;
					}
					else
						cache.miss_count++;
					cache.pages_available = cache.capacity - cache.buffer.size();
				}
				else
				{
					repartitionBuffers();
					if( cache.pages_available ) {
						result = cache.insert( new_page );
						/* make sure t_disk time has elapsed */
						if( !result.first && (now.time - result.second.timestamp.time ) >= (double)t_disk*0.000001 )
							cache.hit_count++;
						else
							cache.miss_count++;
						cache.pages_available = cache.capacity - cache.buffer.size();
					}		
				}
					
			}
		} 
		return true;
	}
	/* enough memory is available */
	else
	{
		for( int i = 0; i < pages_required; i++ )
		{
			Timestamp now;
			now.stamp();
			/* create a fake page to be inserted with a timestamp */
			Page new_page;
			if( !isPrefetched )
				new_page.timestamp.stamp();
			else
				new_page.timestamp.prefetchStamp();
			new_page.block_num = i+1;
			/* make the file pointer point at the system call in the parameter of this function */
			new_page.file = file;
			result = cache.insert( new_page );
			/* make sure t_disk time has elapsed */
			if(!result.first && (now.time - result.second.timestamp.time ) >= (double)t_disk*0.000001)
				cache.hit_count++;
			else
				cache.miss_count++;
			cache.pages_available = cache.capacity - cache.buffer.size();
		}
		return true;
	}
}

/* Precondition : the SystemCall is not in the prefetch buffer */
bool Cache_Manager::prefetchAllocate( Page page)
{
	/* result of insert operation */
	pair<bool, Page> result;
	if( !cache.isCached( page ) )
	{
		/* not a cache miss because we are prefetching */

		/* IF THERE IS A PAGE AVAILABLE */
		if( prefetched.pages_available )
		{
			result = prefetched.insert( page );
			prefetched.pages_available = prefetched.capacity - prefetched.buffer.size(); 
		}
		else
		{
			/* create a Timestamp for the current time */
			set<Page>::iterator it = prefetched.buffer.begin(); // oldest page
			Timestamp present_time;
			present_time.stamp();
			/* create time elapsed for oldest prefetched item */
			double long time_elapsed = present_time.time - (*it).timestamp.time; //seconds
			/* if the prefetch time has expired, eject the prefetch */
			if( (time_elapsed)*1000000 > prefetch_ttl && prefetched.buffer.size() > 0)
			{
				prefetched.buffer.erase( it );
				/* insert a page - NO NEED TO CHANGE PAGES_AVAILABLE*/
				result = prefetched.insert( page );
				prefetched.pages_available = prefetched.capacity - prefetched.buffer.size();
			}
			
			/* use LRU management */
			else
			{
				repartitionBuffers();
				if( prefetched.pages_available ) {
					result = prefetched.insert( page);
					prefetched.pages_available = prefetched.capacity - prefetched.buffer.size();
				}

			}

		}		
	
	}
	/* ELSE DO NOTHING */
	
}

void Cache_Manager::prefetch ( SystemCall *file)
{	
	
	/* see if the file exists as a node in the graph */
	Node *ptr = graph->find( file );
	if( ptr == NULL )
	{
		cout << "File Not Found In Graph For Prefetching! " << endl;
	}
	else	
	{
		/* try to pipeline the prefetches*/
		pipeline( ptr );
		if( ptr->window.size() > 0 )
		{
			for( int i = 0; i < ptr->window.size(); i++)
			{
				/* allocate space to the prefetched data if the strength is above minimum_chance parameter and not prefetched or cached */
				if( (double)(ptr->window[i].strength/(double)ptr->total_strength) >= minimum_chance)
				{
					for( int j = 0; j < ceil( (double)(ptr->window[i].call)->bytes/BLOCK_SIZE); j++)
					{
						Page new_page;
						new_page.timestamp.stamp();
						new_page.file = ptr->window[i].call;
						new_page.block_num = j+1;				
						prefetchAllocate( new_page );
					}
				}	
			} 
		} 
		
	}
}

void Cache_Manager::updateHitRatios()
{
	/* update the hit ratios every 100 microseconds */
	Timestamp now;
	now.stamp(); 
	if(  now.time - last_clock_time.time > 0.0001  )
	{
		cache.update_hit_ratio();
		prefetched.update_hit_ratio();
	}
	last_clock_time.stamp();	
}


void Cache_Manager::pipeline(Node* node)
{
	cout << "Checking if "<< node->call->file <<" is Pipelinable..." << endl;

	/* check to make node has associations */
	if( !node->window.size())
		return;
	
	/******************* STEP 1 : FIND INDICES THAT ARE PROBABLE FOR THE TRIANGULAR MATRIX REPRESENTATIONS ************************/
	int start, end;
	for( int i = 0; i < node->window.size() - 1; i++)
	{
		start = 0, end = 0;
		if( node->window[i].strength > 5 )
		{
			start = i, end = i;
			int cumulative_strength = node->window[i].strength;
			for( int j = i + 1; j < node->window.size() && j < i + prefetch_horizon; j++)
			{	
				if( node->window[j].strength == node->window[i].strength)
				{
					end++;
					cumulative_strength += node->window[j].strength; 
				}
			}
			/* IF WE HAVE A POSSIBLE PIPELINING OPPORTUNITY */
			if( end - start + 1 >= prefetch_horizon && ((double)cumulative_strength/node->total_strength) >= 0.5)
			{
				/************************ STEP 2 : CHECK FOR UPPER TRIANGULAR MATRIX FORM *******************************
				/* do a matrix check */
				if( matrix_check(node, start, end ) )
				{
						/* pipeline prefetch */
						
						// start index end index of Node's assoc window //
						for( int j = start; j <= end; j++ )
						{
							cout << "Pipeline prefetching... " << node->window[j].call->file << endl;
							cout << "File Size : " << node->window[j].call->bytes << endl;
							//// now go to block level ////
							for( int k = 0; k < ceil( ((double)(node->window[j].call->bytes)/BLOCK_SIZE) ); k++)
							{ 
								/* create an empty page representing a block */
								Page new_page;
								new_page.timestamp.stamp();
								new_page.block_num = k+1;
								new_page.file = node->window[j].call;
								/* PREFETCH BLOCK */
								prefetchAllocate( new_page );
							}
						}
						/* reset variables */
						i = end;
						end = 0;
				}
				/* otherwise just continue to check */					
			}

		}
	}
}

bool Cache_Manager::matrix_check(Node* node, int start, int end)
{	

	/* create a  string that represents all files in our SystemCall's window */
	string pipeline_check = "";
	for( int j = 0; j < node->window.size(); j++)
		pipeline_check += " " + node->window[j].call->file;
		
	/* looking for a decreasing pattern */ 
	bool triangle_pattern = true;
	int row_counts[end - start + 1];
	/* load the array with binary*/
	int row_index = 0;
	for( int j = start; j <= end; j++)
	{
		row_counts[row_index] = 0;
		Node *tmp = graph->find( node->window[j].call );
		if( tmp->window.size() > 0 )
		{
			for ( int k = 0; k < tmp->window.size(); k++)
			{
				string test = tmp->window[k].call->file;
								
				if( pipeline_check.find_first_of( test ) != -1)
				{
					/* make sure the association strengths for these two are the same */
					int strength_l = 0;
					for( int l = 0; l < node->window.size(); l++) {
						if( node->window[l].call->file == test ) 
							strength_l = node->window[l].strength;
					}
					int strength_k = tmp->window[k].strength;
					if( strength_k == strength_l )
						row_counts[row_index]++;
				}
			}
		}
		/* check to see if we have broken the triangle pattern */
		if( row_index > 0 && row_counts[row_index] >= row_counts[row_index-1] )
		{
			triangle_pattern = false;
			break;
		}
		row_index++;	
	}
	
	return triangle_pattern;
}

void Cache_Manager::repartitionBuffers()
{
	
	/* find Theta and Delta */
	double Delta = prefetched.get_current_hit_ratio() - prefetched.get_last_hit_ratio();
	double Theta = cache.get_current_hit_ratio() - cache.get_last_hit_ratio();
				

	/* Theta and Delta are not moving significantly -> reset to optimal values */
	if ( Delta > -DOUBLE_ZERO && Delta < DOUBLE_ZERO && Theta > -DOUBLE_ZERO && Theta < DOUBLE_ZERO) {
			
		int new_size =  (double)((double)( assoc_count / graph->nodes.size() )*prefetch_horizon )*prefetched.get_current_hit_ratio();
		if( new_size < (0.1)*(total_pages) && new_size > prefetch_horizon  ) 
	 		prefetched.capacity = new_size;
		else 
			prefetched.capacity = (0.1)*(total_pages);
			
		if( prefetched.capacity > prefetched.buffer.size() )
			prefetched.pages_available = prefetched.capacity - prefetched.buffer.size();
		else
		{
			prefetched.pages_available = 0;
			/* remove excesss pages that now belong to cache */
			while( prefetched.buffer.size() > prefetched.capacity) {
				set<Page>::iterator it = prefetched.buffer.begin();
				prefetched.buffer.erase( it );	
			}
			
		} 
		/* also change the cache buffer size to accomodate any empty space */
		cache.capacity = total_pages - prefetched.capacity;
		if( cache.capacity > cache.buffer.size() )
			cache.pages_available = cache.capacity - cache.buffer.size();
		else
		{
			/* remove excess pages that now belong to prefetch buffer */
			cache.pages_available = 0;
			while( prefetched.buffer.size() > prefetched.capacity) {
				set<Page>::iterator it = cache.buffer.begin();
				cache.buffer.erase( it );	
			} 
		}	
	}
	else if( Delta < Theta )  {
		if(prefetched.capacity > prefetch_horizon)
		{
			prefetched.capacity--;
			if ( prefetched.pages_available )
				prefetched.pages_available--;
			else {
				set<Page>::iterator iter = prefetched.buffer.begin();
				prefetched.buffer.erase( iter );
			}
			cache.capacity++;
			cache.pages_available++;
		}
		if( minimum_chance  < 0.9)
			minimum_chance += 0.1;				
	}
	else if( Delta > Theta ) {
		if( prefetched.capacity <= (0.1)*(total_pages) )
		{
			prefetched.capacity++;
			prefetched.pages_available++;
			cache.capacity--;
			if( cache.pages_available )
				cache.pages_available--;
			else {
				set<Page>::iterator iter = cache.buffer.begin();
				cache.buffer.erase( iter );
			}
		}	
		if( minimum_chance  > 0.3)
			minimum_chance -= 0.1;		

	}	
	/* ELSE DO NOTHING  */
	
	if( Delta > 0 && minimum_chance < 0.9)
		minimum_chance +=0.1;
	else if( Delta < 0 && minimum_chance > 0.3 )
		minimum_chance -=0.1;
	
}


void Cache_Manager::cacheToString()
{
	set<Page>::iterator it;
	//cout << "---------- Cache Buffer ---------- " << endl;
	//for( it = cache.buffer.begin(); it != cache.buffer.end(); it++)
	//{
		//cout << "Accessed : " << setprecision(18) <<(*it).timestamp.time << endl;
		//cout << "File : " << (*(*it).file).file << endl << endl;
	//} 
	cout << "--------------------------" << endl;
	cout << "Cache Hit Ratio : " << setprecision(15) <<cache.get_current_hit_ratio() << endl;
	cout << "Prefetch Hit Ratio : " << setprecision(15) <<prefetched.get_current_hit_ratio() << endl << endl;
	//cout << "---------- Prefetch Buffer ---------- " << endl;
	 cout << "Cache Capacity : " << cache.capacity << endl;
	 cout << "Cache Pages Available : " << cache.pages_available << endl;
	 //cout << "Prefetch Capacity : " << prefetched.capacity << endl; // in pages

	
	 double Delta = prefetched.get_current_hit_ratio() - prefetched.get_last_hit_ratio();
	 double Theta = cache.get_current_hit_ratio() - cache.get_last_hit_ratio();
	
	//cout << "Theta : " << setprecision(15) << Theta << endl;
	//cout << "Delta : " << setprecision(15) << Delta << endl;

	//cout << "Prefetch Pages Available: " << prefetched.pages_available <<endl;
	//for( it = prefetched.buffer.begin(); it != prefetched.buffer.end(); it++)
	//{
		//cout << "Prefetched : " << setprecision(18) <<(*it).timestamp.time << endl;
		//cout << "File : " << (*(*it).file).file << endl << endl;
	//} 
}

#endif
