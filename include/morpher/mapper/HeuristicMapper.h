/*
 * HeuristicMapper.h
 *
 *  Created on: 28 Feb 2018
 *      Author: manupa
 */

#ifndef HEURISTICMAPPER_H_
#define HEURISTICMAPPER_H_

#include <morpher/dfg/DFG.h>
#include <morpher/arch/CGRA.h>
#include <morpher/arch/DataPath.h>
#include <queue>
#include <assert.h>
#include <iostream>
#include <fstream>
#include <math.h>
#include <sstream>
#define MRC 100000
#define UOP 1000

namespace CGRAXMLCompile
{

//struct definitions
class HeuristicMapper;

struct dest_child_with_cost
{
	DFGNode *child;
	DataPath* childDP;
	LatPort childDest;
	LatPort startPort;
	int cost;

	dest_child_with_cost(DFGNode *child, DataPath* dp, LatPort childDest, LatPort startPort, int cost) : child(child), childDP(dp), childDest(childDest), startPort(startPort), cost(cost) {}
	bool operator<(const dest_child_with_cost &rhs) const
	{
		if(cost == rhs.cost){
			return (childDest.second->getFullName()+startPort.second->getFullName()).compare(
				rhs.childDest.second->getFullName()+rhs.startPort.second->getFullName()) > 0;
		}
		return cost > rhs.cost;
	}
};

struct cand_src_with_cost
{
	LatPort src;
	LatPort dest;
	int cost;
	std::string path_in_str;
	cand_src_with_cost(LatPort src, LatPort dest, int cost, std::string path_in_str) : src(src), dest(dest), cost(cost), path_in_str(path_in_str) {}

	bool operator<(const cand_src_with_cost &rhs) const
	{
		if(cost != rhs.cost){
			return cost > rhs.cost;
		}else{
			return path_in_str.compare(rhs.path_in_str) < 0;
		}
	}
};

struct parent_cand_src_with_cost
{
	DFGNode *parent;
	std::priority_queue<cand_src_with_cost> cswc;
	int cost;
	parent_cand_src_with_cost(DFGNode *parent, std::priority_queue<cand_src_with_cost> cswc) : parent(parent), cswc(cswc)
	{
		cost = cswc.top().cost;
	}

	bool operator<(const parent_cand_src_with_cost &rhs) const
	{
		if(this->cost == rhs.cost){
			return (cswc.top().path_in_str + std::to_string(cswc.size())).compare(
				rhs.cswc.top().path_in_str + std::to_string(rhs.cswc.size()) ) > 0;
		}
		return this->cost > rhs.cost;
	}
};

struct dest_with_cost
{
	//	std::map<DFGNode*,std::priority_queue<cand_src_with_cost>> parentStartLocs;
	std::priority_queue<parent_cand_src_with_cost> parentStartLocs;
	std::priority_queue<dest_child_with_cost> alreadyMappedChilds;
	int bestCost;
	DataPath *dest;
	int destLat;
	DFGNode *node;
	dest_with_cost() {}
	dest_with_cost(std::priority_queue<parent_cand_src_with_cost> parentStartLocs,
				   std::priority_queue<dest_child_with_cost> alreadyMappedChilds,
				   DataPath *dest, int destLat, DFGNode *node, int cost,
				   int unmappedMemNodeCount, HeuristicMapper *hm) : parentStartLocs(parentStartLocs), alreadyMappedChilds(alreadyMappedChilds), dest(dest), destLat(destLat), node(node)
	{
		bestCost = sumBestCosts(unmappedMemNodeCount, hm);
	}

	int sumBestCosts(int unmappedMemNodeCount, HeuristicMapper *hm)
	{
		int cost = 0;
		//		for(std::pair<DFGNode*,std::priority_queue<cand_src_with_cost>> pair : parentStartLocs){
		//			assert(!pair.second.empty());
		//			cost+=pair.second.top().cost;
		//		}
		std::priority_queue<parent_cand_src_with_cost> parentStartLocsCopy = parentStartLocs;
		while (!parentStartLocsCopy.empty())
		{
			parent_cand_src_with_cost pcswc = parentStartLocsCopy.top();
			parentStartLocsCopy.pop();
			cost += pcswc.cswc.top().cost;
		}

		//		std::cout << "sumBestCosts :: alreadyMappedChilds size=" << alreadyMappedChilds.size() << "\n";
		std::priority_queue<dest_child_with_cost> alreadyMappedChildCopy = alreadyMappedChilds;
		while (!alreadyMappedChildCopy.empty())
		{
			dest_child_with_cost dcwc = alreadyMappedChildCopy.top();
			alreadyMappedChildCopy.pop();
			cost += dcwc.cost;
		}
		//		std::cout << "sumBestCosts :: alreadyMappedChilds size=" << alreadyMappedChilds.size() << "\n";

		if (cost == 0)
		{
			int freePorts = 0;
			for (Port *p : dest->getPE()->outputPorts)
			{
				Module *parent = dest->getPE()->getParent();
				if (parent->getNextPorts(std::make_pair(dest->getPE()->T, p), hm).empty())
					continue;
				if (p->getNode() == NULL)
				{
					freePorts++;
				}
			}
			//			cost = cost + (dest->getPE()->outputPorts.size() - freePorts)*UOP;
			cost = cost + (15 - freePorts) * UOP;

			int primaryCost = 0;
			for (DFGNode *child : node->children)
			{
				for (DFGNode *parentil : child->parents)
				{
					if (parentil->rootDP != NULL && parentil != node)
					{
						PE *parentilPE = parentil->rootDP->getPE();
						PE *destPE = dest->getPE();
						CGRA *cgra = destPE->getCGRA();

						// int dx = std::abs(parentilPE->X - destPE->X);
						// int dy = std::abs(parentilPE->Y - destPE->Y);
						int dt = (destPE->T - parentilPE->T + cgra->get_t_max()) % cgra->get_t_max();
						primaryCost = primaryCost + dt;
					}
				}
			}
			//parse json branch temporally not using dx dy costs due to templates -- removed again
			cost = cost + primaryCost * 100;

			int secondaryCost = 0;
			for (DFGNode *child : node->children)
			{
				for (DFGNode *childchild : node->children)
				{
					for (DFGNode *childparent : childchild->parents)
					{
						if (childparent != childchild)
						{
							for (DFGNode *parent : childparent->parents)
							{
								if (parent != node && parent->rootDP != NULL)
								{
									PE *parentilPE = parent->rootDP->getPE();
									PE *destPE = dest->getPE();
									CGRA *cgra = destPE->getCGRA();

									// int dx = std::abs(parentilPE->X - destPE->X);
									// int dy = std::abs(parentilPE->Y - destPE->Y);
									// int dt = (destPE->T - parentilPE->T + cgra->get_t_max()) % cgra->get_t_max();
									// secondaryCost = secondaryCost + dx + dy + dt;
								}
							}
						}
					}
				}
			}
			//			cost = cost + secondaryCost*10;

			FU *fu = dest->getFU();
			CGRA *cgra = dest->getCGRA();
			int memcost = 0;
			if (fu->supportedOPs.find("LOAD") != fu->supportedOPs.end())
			{
				double memrescost_dbl = (double)unmappedMemNodeCount / (double)cgra->freeMemNodes;
				memrescost_dbl = memrescost_dbl * (double)MRC;
				memcost = (int)memrescost_dbl;
			}

			cost = cost + memcost;

			LOG(ROUTE) << dest->getPE()->getName() << ",cost=" << cost ;
		}
		cost += getDestTportCost();
		return cost;
	}

	int getDestTportCost()
	{
		PE *destPE = dest->getPE();
		FU *destFU = dest->getFU();

		int latency = destFU->supportedOPs[node->op];
		Port *outputPort = dest->getOutputPort(latency);
		return outputPort->getCongCost();
	}

	bool operator<(const dest_with_cost &rhs) const
	{
		return bestCost > rhs.bestCost;
	}
};

class HeuristicMapper
{
public:
	//	HeuristicMapper(CGRA* cgra, DFG* dfg) : cgra(cgra), dfg(dfg){};
	HeuristicMapper(std::string fName)
	{
		fNameLog1 = fName;
		//		mappingLog.open(fName.c_str());
		//		mappingLog2.open(fName2.c_str());
	}
	virtual ~HeuristicMapper(){};
	CGRA *cgra;
	DFG *dfg;

	int getMinimumII(CGRA *cgra, DFG *dfg);
	int getRecMinimumII(DFG *dfg);
	void SortTopoGraphicalDFG();
	void SortSCCDFG();
	bool Map(CGRA *cgra, DFG *dfg);
	//	bool LeastCostPathAstar(Port* start, Port* end, std::vector<Port*>& path, int& cost, DFGNode* node, std::map<Port*,std::set<DFGNode*>>& mutexPaths, DFGNode* currNode);
	bool LeastCostPathAstar(LatPort start, LatPort end, std::vector<LatPort> &path, int &cost, DFGNode *node, std::map<Port *, std::set<DFGNode *>> &mutexPaths, DFGNode *currNode);
	bool LeastCostPathDjk(Port *start, Port *end, std::vector<Port *> &path, int &cost, DFGNode *node, std::map<Port *, std::set<DFGNode *>> &mutexPaths);
	//	int calculateCost(Port* src, Port* next_to_src, Port* dest);
	int calculateCost(LatPort src, LatPort next_to_src, LatPort dest);

	bool estimateRouting(DFGNode *node, std::priority_queue<dest_with_cost> &estimatedRoutes, DFGNode **failedNode);
	bool Route(DFGNode *node, std::priority_queue<dest_with_cost> &estimatedRoutes, DFGNode **failedNode);

	void assignPath(DFGNode *src, DFGNode *dest, std::vector<LatPort> path);
	bool dataPathCheck(DataPath *dp, DFGNode *node);

	bool sanityCheck();

	bool enableBackTracking = true;
	bool enableMutexPaths = false;
	int backTrackLimit = 4;

	void printMappingLog();
	void printMappingLog2();
	void printFinalMapping(string fileName);
	void printFinalMappingLog2(string fileName);

	std::string dumpMappingToStr();
	std::string dumpCGRAMappingStat();

	bool checkRecParentViolation(DFGNode *node, LatPort nextPort);


	std::string getMappingMethodName(){return mapping_method_name;}
	int MOD(int t){
		int cgra_t =  this->cgra->get_t_max();
		return (t + cgra_t * 100 ) %  cgra_t;
	}

	int upperboundII = 1000000;
	int upperboundIter = -1;
	int upperboundFoundBy = -1;
	std::string mapping_method_name  = "heuristic";
	std::string path_toStr(std::vector<LatPort> path){
		std::stringstream output_stream;
		for(auto p: path){
			output_stream<<p.first<<","<<p.second->getFullName()<<" -> ";
		}
		return output_stream.str();
	};

protected:
	int regDiscourageFactor = 10;
	int PETransitionCostFactor = 100;
	int PortTransitionCost = 1;
	int UOPCostFactor = UOP;
	int MEMResourceCost = MRC;

	std::string fNameLog1;
	std::string fNameLog2;

	std::ofstream mappingLog;
	std::ofstream mappingLog2;
	std::ofstream mappingLog4;
	std::vector<DFGNode *> sortedNodeList;

	void removeFailedNode(std::stack<DFGNode *> &mappedNodes, std::stack<DFGNode *> &unmappedNodes, DFGNode *failedNode);

	int getlatMinStarts(const std::map<DFGNode *, std::vector<Port *>> &possibleStarts);
	std::map<DataPath *, int> getLatCandDests(const std::vector<DataPath *> &candidateDests, int minlat);

	// bool check_parent_violation = true;
};

} /* namespace CGRAXMLCompile */

#endif /* HEURISTICMAPPER_H_ */
