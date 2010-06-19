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

#define PAGE_SIZE 1024 // bytes

/* define all times in microseconds */ 
/* simulate a vendors hardware */

#define RAM_LATENCY 0.05
#define SSD_LATENCY 100 
#define HD_LATENCY 10000
#define TAPE_LATENCY 10000000

/* define bandwidth in megabytes / second */

#define RAM_BITRATE 80
#define SSD_BITRATE 50
#define HD_BITRATE  70
#define TAPE_BITRATE 6 


/* conigurable delay variables */

#define t_disk 10000 // storage disk/drive latency
#define t_cpu  1000 // inter-access CPU time of application ( 1000 system calls per second average )
#define t_hit  50 // time to read a several kilobytes from cache
#define t_driver 500 // time to allocate new pages ???????????????????????????

/* formulas for cost/benefit analysis in microseconds */

#define prefetch_horizon (t_disk/(t_cpu + t_hit + t_driver) ) // number of simultaneous prefetches to pipeline
#define prefetch_ttl (t_disk - prefetch_horizon*(t_cpu + t_hit + t_driver) + prefetch_horizon)*(t_cpu + t_hit + t_driver)  //in microseconds 


/* Tunable variables for Weigted Moving Averages */

#define gamma 0.25 // for the weighted moving cache and prefetch hit ratio



/* global variables */
int lookahead_window;
int assoc_count;
Probability_Graph *graph;

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
};

bool operator<( const Timestamp& lhs, const Timestamp& rhs)
{ return ( lhs.time < rhs.time ); }

struct Page
{
	Timestamp timestamp;
	SystemCall *file;
}; 

struct pageComparison {
  bool operator() (const Page& lhs, const Page& rhs) const
  {	 return  (lhs.timestamp)<(rhs.timestamp); }

};

struct Cache
{
	set< Page, pageComparison > buffer;
	long capacity; // pages
	long pages_available; // pages
	int hit_count, miss_count;
	double hit_ratio;
	double delta_ratio;

	/* check to see if SystemCall is in our buffer */
	bool isCached( SystemCall *call)
	{
		for( set<Page>::iterator it = buffer.begin(); it != buffer.end(); it++ )
		{
			if( (*(*it).file) == *call )
				return true;
		}
		return false;
	}

	/* insert a page into the buffer */
	bool insert ( Page page )
	{
		bool result = isCached( page.file );
		if( result )
			return false;
		else
		{
			/* insert the page into the cache */
			buffer.insert(page);
			/* update the total association_count variable */
			Node *ptr = graph->find( page.file );
			return true;
		}
	}	
	/* get the weighted cache hit ratio */
	double get_current_hit_ratio()
	{
		double current = (hit_count) / (hit_count + miss_count);
		hit_ratio = (1 - gamma )*hit_ratio + gamma*current;
	}
	double get_last_hit_ratio()
	{
		return hit_ratio;
	}
};

struct Prefetch
{
	set< Page, pageComparison > buffer;
	long capacity; // pages
	long pages_available; // pages
	int hit_count, miss_count;
	double hit_ratio;
	bool isPrefetched( SystemCall *call)
	{
		for( set<Page>::iterator it = buffer.begin(); it != buffer.end(); it++ )
		{
			if( (*(*it).file) == *call )
				return true;
		}
		return false;
	}
	/* insert a page into the buffer */
	bool insert ( Page page )
	{
		bool result = isPrefetched( page.file );
		if( result )
			return false;
		else
		{	
			buffer.insert(page);
			return true;
		}
	}

	/* get the weighted cache hit ratio */
	double get_current_hit_ratio()
	{
		double current = (hit_count) / (hit_count + miss_count);
		hit_ratio = (1 - gamma)*hit_ratio + gamma*current;
	}
	double get_last_hit_ratio ()
	{
		return hit_ratio;
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
			new_node->call = (call);
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
	bool prefetching;
	double minimum_chance;
	long   total_pages;
	Cache cache;
	Prefetch prefetched;
	CallWindow call_window;
	
	public:
	/* constructor - param: true -> prefetching / false -> no prefetching */
	Cache_Manager(string, bool, long, double, int);
	/* default constructor */
	Cache_Manager();
	/* allocate memory to a file */
	bool allocate(SystemCall*); 
	bool lruAllocate( SystemCall*);
	bool prefetchAllocate( SystemCall*);
	/* print cache to screen */
	void cacheToString();
	/* prefetching function */
	void prefetch(SystemCall*);

	/* utility functions to check for pipelining availability */
	void pipeline(Node*);
	bool matrix_check(Node*, int, int);
			
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
	cache.hit_ratio = 0;
	prefetched.hit_count = 0;
	prefetched.miss_count = 0;
	prefetched.hit_ratio = 0;
	assoc_count = 0;
	
	/* load our trace data */
	TraceLoader traceLoader(trace_file);
	string data = traceLoader.getData();
	traceLoader.parse(data); // fill calls set

	/* load our probability graph */
	graph = new Probability_Graph(traceLoader.calls, lookahead_window);
	graph->createNodes();
	cout << "Nodes created...." << endl;
	graph->loadAssociations();
	cout << "Associations created..." << endl;


	/* allocate half buffer space to prefetch buffer half to cache buffer initially */
	total_pages = size_in_bytes/PAGE_SIZE;
	cache.capacity = total_pages/2;
	prefetched.capacity = total_pages/2;

	/* initialize buffers */
	cache.pages_available = cache.capacity;
	prefetched.pages_available = prefetched.capacity;
}


bool Cache_Manager::allocate( SystemCall *file)
{
	/* LRU Management */
	if( !prefetching )
	{
		if ( cache.isCached( file ) )
		{
			cache.hit_count++;
			return true;
			
		}
		else
		{
			cache.miss_count++;
			return lruAllocate( file );
		}
	}
	/* LRU with prefetching */
	else
	{
		/* add the call to our call window and dynamically update the probability graph */
		call_window.insert( file );
		/* prefetch calls that are in the lookahead window */
		prefetch( file );
		
		if ( cache.isCached( file ) )
		{
			cache.hit_count++;
			return true;
		}
		else
		{
			cache.miss_count++;
			if( prefetched.isPrefetched( file) )
			{
				prefetched.hit_count++;
				/* place in the cache -> we know it is not already cached */
				Page new_page;
				new_page.file = file;
				new_page.timestamp.stamp();
				/* cache allocation function */
				lruAllocate( file );
					
				/* delete from the prefetch buffer */
				for( set<Page>::iterator it = prefetched.buffer.begin(); it != prefetched.buffer.end(); it++)
				{
					if( *file ==  *(*it).file )
					{
						prefetched.buffer.erase(it);
						prefetched.pages_available++;
					}
				}				
				
				return true;
			}
			else
			{
				prefetched.miss_count++;
				return lruAllocate( file );
			}
		}	
	}
	
}

bool Cache_Manager::lruAllocate( SystemCall *file )
{
	
	/* get the number of pages required by the file */
	long pages_required =  ceil( ((double)(file->bytes)/ PAGE_SIZE) );
	/* result of insert operation */
	bool result;
	/* see if enough memory is available */
	if( pages_required > cache.pages_available )
	{
		/* employ LRU management */
		for( int i = 0; i < pages_required; i++ )
		{
			/* create our fake page to be inserted at the tail of the set */
			Page new_page;
			new_page.file = file;
			new_page.timestamp.stamp();
			/* insert a page into free cache memory ( at the tail ) */
			if( cache.pages_available )
			{
				result = cache.insert( new_page ); // unless it already exists
				if( !result )
					return false;
				else
					cache.pages_available--;		
			}
			/* deallocated memory at the head of the set and add a page */
			else
			{
				/* deallocate LRU page */
				set<Page>::iterator it = cache.buffer.begin();
				cache.buffer.erase( it );
				/* insert a page */
				result = cache.insert( new_page );
				/* then the page is already in the cache and we have a cache hit */
				if(!result)
				{
					return false;
					cache.pages_available++;
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
			/* create a fake page to be inserted with a timestamp */
			Page new_page;
			new_page.timestamp.stamp();
			/* make the file pointer point at the system call in the parameter of this function */
			new_page.file = file;
			result = cache.insert( new_page );
			if(!result)
			{ return false; }
			else
				cache.pages_available--;
		}
		return true;
	}
}

/* Precondition : the SystemCall is not in the prefetch buffer */
bool Cache_Manager::prefetchAllocate( SystemCall *file )
{
	
	/* get the number of pages required by the file */
	long pages_required =  ceil( ((double)(file->bytes)/ PAGE_SIZE) );
	/* result of insert operation */
	bool result = true;
	/* see if enough memory is available */
	if( pages_required > prefetched.pages_available )
	{
		/* add pages to available memory */
		for( int i = 0; i < pages_required; i++ )
		{
			/* create our fake page to be inserted at the tail of the set */
			Page new_page;
			new_page.file = file;
			new_page.timestamp.stamp();
			/* insert a page into free prefetch buffer memory */
			if( prefetched.pages_available )
			{
				result = prefetched.insert( new_page ); // unless it already exists
				if( !result )
					return false;
				else
					prefetched.pages_available--;		
			}
			/* deallocated memory according to prefetch time to live policy (PTTL) */
			else
			{
				/* create a Timestamp for the current time */
				set<Page>::iterator it = prefetched.buffer.begin(); // oldest page
				Timestamp present_time;
				present_time.stamp();
				/* create time elapsed for oldest prefetched item */
				double long time_elapsed = present_time.time - (*it).timestamp.time; //seconds
				/* if the prefetch time has expired, eject the prefetch */
				if( (time_elapsed)*1000000 > prefetch_ttl )
				{
					cout << endl << " ...prefetch buffer full...lru prefetch ejected ... " << endl;
					prefetched.buffer.erase( it );
					/* insert a page */
					result = prefetched.insert( new_page );
				}
				/* run a cost/benefit analysis to find least valuable buffer space */
				else
				{
					/* use cache management policies defined in Table of Paper */
					
					/* find Theta and Delta */
					double Theta = prefetched.get_current_hit_ratio() - prefetched.get_last_hit_ratio();
					double Delta = cache.get_current_hit_ratio() - cache.get_last_hit_ratio();
					
					if( Theta == 0.0 && Delta == 0.0) {
						prefetched.capacity = prefetch_horizon*( assoc_count/graph->nodes.size())*prefetched.hit_ratio;
						prefetched.pages_available = prefetched.capacity - prefetched.buffer.size();
						cache.capacity = total_pages - prefetched.capacity;
						cache.pages_available = cache.capacity - cache.buffer.size();
					}
					else if( Delta == 0 && Theta > 0 ){
						prefetched.capacity++;
						prefetched.pages_available++;
						cache.capacity--;
						cache.pages_available--;
					}
					else if( Delta < 0 && Theta == 0) {
						prefetched.capacity--;
						prefetched.pages_available--;
						cache.capacity++;
						cache.pages_available++;
					}
					else if( Delta == 0 && Theta < 0 )  {
						prefetched.capacity++;
						prefetched.pages_available++;
						cache.capacity--;
						cache.pages_available--;
					}
					else if( Delta < 0 && Theta < 0 ) {
						prefetched.capacity--;
						prefetched.pages_available--;
						cache.capacity++;
						cache.pages_available++;
					}
						
				}
				/* then the page is already in the cache and we have a prefetch hit */
				if(!result)
				{
					return false;
					prefetched.pages_available++;
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
			/* create a fake page to be inserted with a timestamp */
			Page new_page;
			new_page.timestamp.stamp();
			/* make the file pointer point at the system call in the parameter of this function */
			new_page.file = file;
			result = prefetched.insert( new_page );
			if(!result)
			{ return false; }
			else
				prefetched.pages_available--;
		}
		return true;
	}
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
				if( (double)(ptr->window[i].strength/(double)ptr->total_strength) >= minimum_chance && !prefetched.isPrefetched( ptr->window[i].call ) && !cache.isCached( ptr->window[i].call))
				{
					/* dynamically resize the prefetch buffer according to average node size * prefetch horizon */
					if( assoc_count >= 100 && assoc_count % 2 == 0 ) //every 100 new nodes
					{ 
						prefetched.capacity = (double)((double)( assoc_count / graph->nodes.size() ) * prefetch_horizon );
						prefetched.pages_available = prefetched.capacity - prefetched.buffer.size();
						/* also change the cache buffer size to accomodate any empty space */
						cache.capacity = total_pages - prefetched.capacity;
						cache.pages_available = cache.capacity - cache.buffer.size();
					
					}
						prefetchAllocate( ptr->window[i].call );
				}	
			} 
		} 
		
	}
}


void Cache_Manager::pipeline(Node* node)
{
	cout << "Checking if "<< node->call->file <<" is Pipelinable..." << endl;

	/* check to make node has associations */
	if( !node->window.size())
		return;
	
	/******************* STEP 1 : FIND INDICES THAT ARE PROBABLE FOR THE TRIANGULAR MATRIX REPRESENTATIO ************************/
	cout << "STARTING STEP 1... prefetch horizon = " << prefetch_horizon <<endl;
	int start, end;
	for( int i = 0; i < node->window.size() - 1; i++)
	{
		start = 0, end = 0;
		cout<<"    checking association: " << node->window[i].call->file << endl;
		cout<<"         STRENGTH: " << node->window[i].strength << endl;
		if( node->window[i].strength > 3 )
		{
			cout << "Found an Association with strength > 3..." << endl;
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
			cout << " percent of Total Strength: " << setprecision(5) << ((double)cumulative_strength/node->total_strength) << endl;
			if( end - start + 1 >= prefetch_horizon && ((double)cumulative_strength/node->total_strength) >= 0.5)
			{
				/************************ STEP 2 : CHECK FOR UPPER TRIANGULAR MATRIX FORM *******************************
				/* do a matrix check */
				cout << ".... starting a matrix check....." << endl;
				cout << "     Start Index : " << start << endl;
				cout << "     End Index : " << end << endl;
				if( matrix_check(node, start, end ) )
				{
						/* pipeline prefetch */
						cout << "Pipelining..." << endl;
						for( int j = start; j <= end; j++ )
						{
							if( !prefetched.isPrefetched( node->window[j].call ) )
							{
								prefetchAllocate( node->window[j].call );
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

void Cache_Manager::cacheToString()
{
	set<Page>::iterator it;
	//cout << "---------- Cache Buffer ---------- " << endl;
	//for( it = cache.buffer.begin(); it != cache.buffer.end(); it++)
	//{
		//cout << "Accessed : " << setprecision(18) <<(*it).timestamp.time << endl;
		//cout << "File : " << (*(*it).file).file << endl << endl;
	//}
	//cout << endl << "Prefetch Horizon : " << setprecision(5) << prefetch_horizon << endl; 
	//cout << "Cache Hit Ratio : " << setprecision(5) <<cache.get_current_hit_ratio() << endl;
	
	//cout << "---------- Prefetch Buffer ---------- " << endl;
	//cout << "Buffer Size : " << prefetched.capacity << endl;
	//cout << "Pages Available: " << prefetched.pages_available <<endl;
	//for( it = prefetched.buffer.begin(); it != prefetched.buffer.end(); it++)
	//{
		//cout << "Prefetched : " << setprecision(18) <<(*it).timestamp.time << endl;
		//cout << "File : " << (*(*it).file).file << endl << endl;
	//} 
}

#endif
