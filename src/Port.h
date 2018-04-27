/*
 * Port.h
 *
 *  Created on: 26 Feb 2018
 *      Author: manupa
 */

#ifndef PORT_H_
#define PORT_H_

#include "DFGEdge.h"
#include "DFGNode.h"
#include <string>
#include <vector>
#include <map>

#define INIT_CONG_COST 100000


namespace CGRAXMLCompile {

class PE;
class Module;
class HeuristicMapper;

enum PortType{IN,OUT,INT};

class Port {
public:
	Port(std::string name, PortType pType, Module* mod);
	std::string getName(){return name;}
	std::string getFullName();

	void clear();
	PE* findParentPE();
	Module* getMod(){return mod;}
	PortType getType(){return pType;}

	void setNode(DFGNode* node, HeuristicMapper* hm = NULL);

	DFGNode* getNode(){return this->node;}

	int  getCongCost();
	void increastCongCost();
	void increaseUse(HeuristicMapper* hm = NULL);
	void decreaseUse(DFGNode* extnode, HeuristicMapper* hm = NULL);
	void increaseConflictedUse(HeuristicMapper* hm = NULL);
	int getHistoryCost(){return history_cost;}

private:

	std::string name;
	Module* mod;
	PortType pType;
	DFGNode* node=NULL;

	int base_cost = INIT_CONG_COST;
	int history_cost = 0;
	int number_signals = 0;

	bool operator==(const Port &other){
		return (this->name.compare(other.name)==0);
	}



};

} /* namespace CGRAXMLCompile */

#endif /* PORT_H_ */
