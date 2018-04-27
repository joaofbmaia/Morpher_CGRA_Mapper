/*
 * Port.cpp
 *
 *  Created on: 26 Feb 2018
 *      Author: manupa
 */

#include "Port.h"
#include "PE.h"
#include "Module.h"
#include "CGRA.h"
#include "PathFinderMapper.h"
#include "CGRA.h"
#include <stack>
#include <iostream>

namespace CGRAXMLCompile {

Port::Port(std::string name, PortType pType, Module* mod) {
	// TODO Auto-generated constructor stub
	this->name = name;
	this->pType = pType;
	this->mod = mod;
}

void Port::increaseUse(HeuristicMapper* hm) {

	if(PathFinderMapper* pfm = dynamic_cast<PathFinderMapper*>(hm)){
		for(DFGNode* node : (*pfm->getcongestedPortsPtr())[this]){
			if(this->node == node) continue;
			if(pfm->dfg->isMutexNodes(this->node,node)) continue;
			number_signals++;
			break;
		}
	}
	else{
		number_signals++;
	}


}

void Port::decreaseUse(DFGNode* extnode, HeuristicMapper* hm) {
	if(number_signals>0){
		if(PathFinderMapper* pfm = dynamic_cast<PathFinderMapper*>(hm)){
			for(DFGNode* con_node : (*pfm->getcongestedPortsPtr())[this]){
				if(extnode == con_node) continue;
				if(pfm->dfg->isMutexNodes(extnode,con_node)) continue;
				number_signals--;
				break;
			}
		}
		else{
			number_signals--;
		}
	}
}

void Port::setNode(DFGNode* node, HeuristicMapper* hm){
	this->node=node;

	if(hm == NULL) return;

	if(PathFinderMapper* pfm = dynamic_cast<PathFinderMapper*>(hm)){
		for(Port* p : getMod()->getConflictPorts(this)){
			(*pfm->getcongestedPortsPtr())[p].insert(node);
		}
	}
}

void Port::increaseConflictedUse(HeuristicMapper* hm) {
	increaseUse();

	if(!getMod()->isConflictPortsEmpty(this)){
		for(Port* p : getMod()->getConflictPorts(this)){
			assert(p!=NULL);
			p->increaseUse();
		}
	}

//	if(this->getType()==OUT){
//		for(Port* p : getMod()->getParent()->getConflictPorts(this)){
//			p->increaseUse();
//		}
//	}
}

} /* namespace CGRAXMLCompile */

CGRAXMLCompile::PE* CGRAXMLCompile::Port::findParentPE() {

	Module* m = mod;

	while(m){
		if(PE* ret = dynamic_cast<PE*>(m)){
			return ret;
		}
		m = m->getParent();
	}
	return NULL;
}

std::string CGRAXMLCompile::Port::getFullName() {
	Module* mod = this->mod;
	std::stack<std::string> fullNameSt;

	while(mod){
		fullNameSt.push(mod->getName());
		mod = mod->getParent();
	}

	std::string fullName;
	while(!fullNameSt.empty()){
		fullName = fullName + fullNameSt.top() + ".";
		fullNameSt.pop();
	}
	fullName = fullName + name;
	return fullName;
}

void CGRAXMLCompile::Port::increastCongCost() {
	if(history_cost == 0){
		history_cost = INIT_CONG_COST/10;
	}
	else{
		history_cost = history_cost + history_cost/10;
	}
}

int CGRAXMLCompile::Port::getCongCost() {
	int cost = base_cost*number_signals + history_cost*(number_signals+1);
	return cost;
}


void CGRAXMLCompile::Port::clear() {
	if(node!=NULL){
		(*this->mod->getCGRA()->getCongestedPortPtr())[this].erase(node);
	}


	for(Port* p : getMod()->getConflictPorts(this)){
		p->decreaseUse(node);
		(*p->mod->getCGRA()->getCongestedPortPtr())[p].erase(node);
	}

	if(this->getType()==OUT){
		for(Port* p : getMod()->getParent()->getConflictPorts(this)){
			p->decreaseUse(node);
			(*p->mod->getCGRA()->getCongestedPortPtr())[p].erase(node);
		}
	}

	node=NULL;
	number_signals=0;
}
