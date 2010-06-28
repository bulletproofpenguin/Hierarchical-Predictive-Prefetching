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
	int lookaheadWindow;
	int  size;

	public :
	vector<Node> nodes;
	/* default constructor */
	Probability_Graph();
	/* constructor with a vector of SystemCalls */
	Probability_Graph(int);
	
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
Probability_Graph::Probability_Graph(int tmp2)
{
	lookaheadWindow = tmp2;
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
/* Precondition : will only find Nodes that are 'open' calls */
Node* Probability_Graph::find ( SystemCall *file) {
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
