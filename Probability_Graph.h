/* precondition : the lookahead window must be specified in microseconds */
/* Assumption: any system call occuring at the same microsecond of time is considered to be equivalent */
#ifndef Probability_Graph_H
#define Probability_Graph_H

#include <iostream>
#include <string.h>
#include <vector>
#include <set>
#include <stdlib.h>
#include <utility>
#include "Driver.h"
#include <iomanip>

using namespace std;

/***** STRUCTURES *********/

struct Association
{

	SystemCall *call;
	int strength;

};

struct Node 
{
	/* a set of calls made at different times */
	SystemCall *call;
	vector<Association> window; //possible options in the lookahead period
	int total_strength;
};

/************************/


/***** COMPARISONS *****/
struct nodeComparison {
  bool operator() (const Node &lhs, const Node &rhs) const
  { return (*(lhs.call))<(*(rhs.call)); }
};
/******************** */


void nodeToString(Node node)
{
	systemCallToString(*node.call);
	cout << "Total Strength: " << node.total_strength << endl;
}



class Probability_Graph
{

	private :
	set<SystemCall*, systemCallComparison> calls;
	int lookaheadWindow;
	int  size;

	public :
	vector<Node> nodes;
	/* default constructor */
	Probability_Graph();
	/* constructor with a vector of SystemCalls */
	Probability_Graph(set<SystemCall*, systemCallComparison>&, int);
	
	/* create a vector of Nodes from the system call vector */
	void createNodes();
	/* create the graph with the given Node vector */
	void loadAssociations(); 
	/* create a set of nodes [nodeSet] and their respective associations */
	void remove_dups( vector<Node>& );
	void remove_dups( vector<Association>& );

	/* to find a Node in the graph */
	Node* find(SystemCall*);

	
};

/* constructor */
Probability_Graph::Probability_Graph()
{
}
Probability_Graph::Probability_Graph( set<SystemCall*, systemCallComparison> &tmp, int tmp2)
{
	calls = tmp;
	lookaheadWindow = tmp2;
}

void Probability_Graph::createNodes()
{
	for( set<SystemCall*>::iterator i = calls.begin(); i != calls.end(); i++)
	{
		Node node;
		SystemCall *call = *i;
		node.call = call;
		node.total_strength = 0;
		nodes.push_back(node);
	} 
}

/* remove duplicate associations and strengthen arcs as needed */
void Probability_Graph::remove_dups (vector<Association>& v) 
{
    
    if( !v.empty() )
    {
    	for( int i = 0; i < v.size() - 1; i++)
    	{	
		for( vector<Association>::iterator it = v.begin() + i + 1 ; it != v.end(); it++)
		{
			if( (*v.at(i).call) == (*(*it).call)   )
			{
				vector<Association>::iterator del = v.begin()+i;
				(*it).strength += (*del).strength;
				v.erase( del );
				if( i > 0 )
					i--;
				break;
			}	
		}	

    	}
    }
	
}

/* Precondition : the vector of Nodes are ordered temporally ( least to greatest ) to the microSecond */
void Probability_Graph::remove_dups (vector<Node>& v) 
{
    
   /* remove duplicate nodes and merge association vectors as needed needed */
    if( !v.empty() )
    {
    	for( int i = 0; i < v.size() - 1; i++)
    	{	
		for( vector<Node>::iterator it = v.begin() + i + 1 ; it != v.end(); it++)
		{
				if( (*v.at(i).call) == (*(*it).call) )
				{
					/* merge the two association vectors */
					for( int j = 0; j < (*it).window.size(); j++)
					{
						v.at(i).window.push_back( (*it).window[j] );
					}
					v.at(i).total_strength += (*it).total_strength;
					v.erase( it );
					--it;
				}	
		}	

    	}
    }

   
}


void Probability_Graph::loadAssociations()
{		
	for( int i = 0; i < nodes.size() ; i++ )
	{
		int count = 1;
		while( (i + count) < nodes.size() && *nodes.at(i).call < *nodes.at(i+count).call && *nodes.at(i+count).call < ( *nodes.at(i).call + (double)(lookaheadWindow*0.000001)))
		{
				
			/* create an association */
			Association assoc;
			/* copy all the information into a new system call to avoid deletion later */
			assoc.call = new SystemCall;
			assoc.call->callType = nodes.at(i+count).call->callType;
			assoc.call->file = nodes.at(i+count).call->file;
			assoc.call->hourTime = nodes.at(i+count).call->hourTime;
			assoc.call->minuteTime = nodes.at(i+count).call->minuteTime;
			assoc.call->secondTime = nodes.at(i+count).call->secondTime;
			assoc.call->microSecondTime = nodes.at(i+count).call->microSecondTime;
			assoc.call->bytes = nodes.at(i+count).call->bytes;
			assoc.call->streamID = nodes.at(i+count).call->streamID;
			assoc.strength=1;
			/* avoid associations that are the same file as this call */
			if( ! (*nodes.at(i+count).call == *nodes.at(i).call) )	
				nodes.at(i).window.push_back(assoc);			
			count++;		
		} // end while
		
		/* add total_strength for each node */
		
	} // end for

	/* remove duplicate nodes and merge associations */
	remove_dups( nodes );

	for( int i = 0; i < nodes.size() ; i++)
	{
		remove_dups( nodes.at(i).window );
		for(int j = 0; j < nodes.at(i).window.size(); j++)
			nodes.at(i).total_strength += nodes.at(i).window[j].strength;
	}
	
}

/* Precondition : will only find Nodes that are 'open' calls */
Node* Probability_Graph::find ( SystemCall *file)
{
	Node *ptr = NULL;
	
	/* iterate through the set of Nodes to see if this one is present */
	for( int i = 0; i < nodes.size(); i++ )
	{
		if( (*nodes.at(i).call) == *file ) // only works for open calls
			ptr = &nodes.at(i);
	}	
	/* otherwise return NULL */

	return ptr;
}



#endif
