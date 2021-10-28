#include "PathFinderMapper.h"

#include "SimulatedAnnealingMapper.h"
#include <queue>
#include <assert.h>
#include <math.h>
#include <algorithm> // std::reverse
#include "DataPath.h"
#include "FU.h"

#include <stack>
#include <functional>
#include <set>
#include <iostream>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <bitset>

namespace CGRAXMLCompile
{

} /* namespace CGRAXMLCompile */

/*
@TODO Verify
0. Initial mapping
1. Resotre mapping.
2. Parent and Child Node
3. Congestion

*/
/* Things migh need to be improved
1. When selected node to unmap, I select unmapped nodes first and unmap one of their parents. 
	If no umapped nodes, I will randomly select one node.
	Will it be better if we randomly select from any node, or we just give a bit higher priority to unmapped nodes?
*/
bool  CGRAXMLCompile::SAMapper::SAMap(CGRA *cgra, DFG *dfg, std::ofstream& sumFile){
  std::stack<DFGNode *> mappedNodes;
	std::stack<DFGNode *> unmappedNodes;
	std::map<DFGNode *, std::priority_queue<dest_with_cost>> estimatedRouteInfo;


	//Disable mutex paths to test pathfinder
	this->enableMutexPaths = true;

	this->cgra = cgra;
	this->dfg = dfg;

	Check_DFG_CGRA_Compatibility();

	if(cgra->is_spm_modelled){
		UpdateVariableBaseAddr();
	}
	sortBackEdgePriorityASAP();

	std::string mappingLogFileName = fNameLog1 + cgra->getCGRAName() + "_MTP=" + std::to_string(enableMutexPaths);  // + ".mapping.csv";
	std::string mappingLog2FileName = fNameLog1 + cgra->getCGRAName() + "_MTP=" + std::to_string(enableMutexPaths); // + ".routeInfo.log";
	
	

	bool mapSuccess = false;

	std::string congestionInfoFileName = mappingLogFileName + ".congestion.info";
	cout << "Opening congestion file : " << congestionInfoFileName << "!\n";
	congestionInfoFile.open(congestionInfoFileName.c_str());
	assert(congestionInfoFile.is_open());

	

  std::string mappingLogFileName_withIter = mappingLogFileName + "_SA" + ".mapping.csv";
  std::string mappingLog2FileName_withIter = mappingLog2FileName + "_SA" + ".routeInfo.log";
  std::string mappingLog4FileName_withIter = mappingLogFileName + "_II=" + std::to_string(cgra->get_t_max())+ "_SA" + ".mappingwithlatency.txt";

  mappingLog.open(mappingLogFileName_withIter.c_str());
  mappingLog2.open(mappingLog2FileName_withIter.c_str());
  mappingLog4.open(mappingLog4FileName_withIter.c_str());

  cout << "Opening mapping csv file : " << mappingLogFileName_withIter << "\n";
  cout << "Opening routeInfo log file : " << mappingLog2FileName_withIter << "\n";

  assert(mappingLog.is_open());
  assert(mappingLog2.is_open());
  assert(mappingLog4.is_open());

  while (!mappedNodes.empty())
  {
    mappedNodes.pop();
  }
  while (!unmappedNodes.empty())
  {
    unmappedNodes.pop();
  }

  for (DFGNode *node : sortedNodeList)
  {
    unmappedNodes.push(node);
  }

  std::cout << "*******************************************************SA MAP begin***************************\n";
  

  data_routing_path.clear();
	dfg_node_placement.clear();
	if(!initMap(sumFile)){
		//assert(false && "how come?");
		std::cout<<"cannot find an initial mapping, exit....\n";
		return false;
	}

  
	int unmapped_node_numer = getNumberOfUnmappedNodes();
	int overuse_number = getCongestionNumber();
	std::cout<<"Initial mapping done. unmapped node:"<<unmapped_node_numer<<" overuse:" <<overuse_number<<" \n";
	sumFile << "Initial mapping done. unmapped node number:"<<unmapped_node_numer<<" overuse:" <<overuse_number<<" \n";
	if(unmapped_node_numer == 0 && overuse_number == 0){
		std::cout<<"find a valid initial mapping, exit....\n";
		return true;
	}
	if(overuse_number > maxCongestion){
			sumFile << "Map failed due to number of congestions is higher than maximum congestion value(" << maxCongestion <<")\n";
			return false;
	}
	std::cout<<"maximum temperature:"<<maximum_temp<<" minimum temperature:"<<minimim_temp<<"\n";
	curr_cost = getCost();
	curr_temp = maximum_temp;
	time_t begin,end;
	while(curr_temp > minimim_temp){
		time (&begin);
		std::cout<<"*******************************current temperature:"<<curr_temp<<"\n";


		overuse_number = getCongestionNumber();
		unmapped_node_numer = getNumberOfUnmappedNodes();


		sumFile << "current temperature:"<<curr_temp<<",";

		sumFile << "unmapped node number:"<<unmapped_node_numer<<" overuse:" <<overuse_number<<" \n";

		sumFile.flush();
		float accept_rate = inner_map();
		std::cout<<"accept_rate:"<<accept_rate;
		curr_temp = updateTemperature(curr_temp, accept_rate);
		
		if(isCurrMappingValid()){
			std::cout<<"found a valid mapping, exit....\n";
			break;
		}


		time (&end); // note time after execution

		double difference = difftime (end,begin);
		sumFile << "Exec Time: "<< difference/3600 << "(h)\n";
	}
  
	return  isCurrMappingValid();
}

bool CGRAXMLCompile::SAMapper::initMap(std::ofstream& sumFile){

	std::stack<DFGNode *> mappedNodes;
	std::stack<DFGNode *> unmappedNodes;
	std::map<DFGNode *, std::priority_queue<dest_with_cost>> estimatedRouteInfo;
	enableBackTracking = false;
	int backTrackLimit = 0;
	int backTrackCredits = 0;
	time_t begin,end,now;

	//for (int i = 0; i < this->maxIter; ++i)
	//{
		//sumFile << "\nIteration: "<<i<<"  -----------------------------------------------\n";sumFile.flush();

		time (&begin);
		while (!mappedNodes.empty())
		{
			mappedNodes.pop();
		}
		while (!unmappedNodes.empty())
		{
			unmappedNodes.pop();
		}

		for (DFGNode *node : sortedNodeList)
		{
			unmappedNodes.push(node);
		}
		int total_nodes = unmappedNodes.size();
		sumFile << "Total nodes:" <<total_nodes << "\n";
		double prev_node_percentage = 0;

		while (!unmappedNodes.empty())
		{
			time (&now); // note time before execution

			//if(i>0){
//						double elapsed_time = difftime (now,start_time);
//						double maxIterationTimeinSec = 3600*maxIterationTime;
//						if (elapsed_time > maxIterationTimeinSec){
//							sumFile << "Map stopped due to iteration time limit exceed:\n";sumFile.flush();
//							return false;
//						}
			//}

			DFGNode *node = unmappedNodes.top();
			unmappedNodes.pop();

			std::stringstream MapHeader;
			MapHeader << "current node = " << node->idx;
			MapHeader << ",op = " << node->op;
			MapHeader << ",unmapped nodes = " << unmappedNodes.size();
			MapHeader << ",mapped nodes = " << mappedNodes.size();
			MapHeader << ",freeMemNodes = " << cgra->freeMemNodes;
			MapHeader << ",unmappedMemNodes = " << dfg->unmappedMemOps;
			MapHeader << ",II = " << cgra->get_t_max();
			MapHeader << ",btCredits = " << backTrackCredits;
			double mapped_node_percentage = ((mappedNodes.size()*100/total_nodes));

			if(prev_node_percentage!=mapped_node_percentage && (mapped_node_percentage == 25 || mapped_node_percentage == 50 || mapped_node_percentage > 75)){
				sumFile << mapped_node_percentage<< "% ";

				//if (mapped_node_percentage == 25 || mapped_node_percentage == 50 || mapped_node_percentage > 75)
				sumFile.flush();
			}

			prev_node_percentage = mapped_node_percentage;


			// MapHeader << ",PEType = " << this->cgra->peType;
			// MapHeader << ",XDim = " << this->cgra->get_x_max();
			// MapHeader << ",YDim = " << this->cgra->get_y_max();
			// MapHeader << ",DPs = " << this->cgra->numberofDPs;

			MapHeader << ",CGRA=" << this->cgra->getCGRAName();
			MapHeader << ",MaxHops=" << this->cgra->max_hops;

			MapHeader << ",BB = " << node->BB;
			MapHeader << ",mutexPathEn = " << this->enableMutexPaths;
			MapHeader << "\n";

			std::cout << MapHeader.str();


			bool isEstRouteSucc = false;



			//fill the routing information
			if (estimatedRouteInfo.find(node) == estimatedRouteInfo.end())
			{
				//the routes are not estimated.

				DFGNode *failedNode;
				std::priority_queue<dest_with_cost> estimatedRoutes;
				isEstRouteSucc = estimateRouting(node, estimatedRoutes, &failedNode);
				if(isEstRouteSucc){
					estimatedRouteInfo[node] = estimatedRoutes;
				}else{

#ifdef HIERARCHICAL
#ifdef HOTFIX3

					if(node->map_on_any_cluster == false){//no point of continuing if the node can be already placed in any cluster

						node->map_on_any_cluster = true;
						unmappedNodes.push(node);
						std::cout << "This node will be mapped in any cluster in future iterations! Return mapping.\n";
						sumFile << "ANYCLUS:["<< node->idx<<"] ";
						continue;
					}
#endif
#endif
				return false;//initial mapping should have valid estimated routes
				}


			}

			bool isRouteSucc = false;
			DFGNode *failedNode = NULL;

			std::cout << "estimatedRouteInfo[node].size = " << estimatedRouteInfo[node].size() << "\n";
			mappingLog << "estimatedRouteInfo[node].size = " << estimatedRouteInfo[node].size() << "\n";
			if (!estimatedRouteInfo[node].empty())
			{
				isRouteSucc = Route(node, estimatedRouteInfo[node], &failedNode);
				if (!isRouteSucc){
					LOG(ROUTE) << "route not successful!\n";
				}
				// else if(!estimatedRouteInfo[node].empty()){
				// 	//route not successful
				// }
			} else{
				isRouteSucc = false;
			}

			if (!isRouteSucc)
			{


				if (enableBackTracking) // always false
				{
					if (backTrackCredits == 0)
					{
						std::cout << "Map Failed!.\n";
						return false;
					}
					//					assert(failedNode!=NULL);
					backTrackCredits--;

					DFGNode *prevNode = mappedNodes.top();
					mappedNodes.pop();
					unmappedNodes.push(node);
					unmappedNodes.push(prevNode);

					prevNode->clear(this->dfg);
					estimatedRouteInfo.erase(node);
					continue;
				}
				else
				{

					node->clear(this->dfg);
					std::cout<<"----------node" <<node->idx<<" not mapped in initial mapping\n";
				}
				return false;//initial mapping should have valid routes
			}
			backTrackCredits = std::min(this->backTrackLimit, backTrackCredits + 1);
			mappedNodes.push(node);
		}
		time (&end); // note time after execution

		double difference = difftime (end,begin);
		sumFile << "Init map: Iteration Exec Time: "<< difference/3600 << "(h)\n";
		sumFile.flush();
		return true;
	//}
	//return false;

}


float CGRAXMLCompile::SAMapper::inner_map(){
	int accepted_number = 0;
  
  for(int i = 0 ; i < movement_in_each_temp; i ++ ){
		LOG(SA)<<"************ NO."<<i<<" movement, unmapped nodes:"<<getNumberOfUnmappedNodes()<< ", congestion:"<<getCongestionNumber();


		//select DFG node to unmap
	
    auto selected_dfg_node = selectDFGNodeToUnmap();
		DFGNode * child_node = NULL;
		if(dfg_node_placement.find(selected_dfg_node)== dfg_node_placement.end()){
			// this node is not placed yet because of routing failure.
			// The reason for routing failure is that it cannot route to its parent nodes or recurrent dependency constraint;
			child_node = selected_dfg_node;
			selected_dfg_node = selectAParentForDFGNode(child_node);
			LOG(SA)<<"----------be careful. Have a child node "<<child_node->idx<<"\n";
		}
		// assert(dfg_node_placement.find(selected_dfg_node)!= dfg_node_placement.end());
		std::map<DFGNode*, std::pair<DataPath*, int>> old_dfg_node_placement ;
		old_dfg_node_placement.insert(dfg_node_placement.begin(), dfg_node_placement.end());
		std::map<dfg_data, std::vector<LatPort>> old_data_routing_path;
		old_data_routing_path.insert(data_routing_path.begin(), data_routing_path.end());
		clearNodeMapping(selected_dfg_node);
    auto dp_candidate = getRandomDPCandidate(selected_dfg_node).front();


		//start map
		LOG(SA)<<"selected DFG node:"<<selected_dfg_node->idx<<" op:"<<selected_dfg_node->op<<" previous_pe:"
		<<old_dfg_node_placement[selected_dfg_node].first->getPE()->getName()<<" current_pe:"<<dp_candidate->getPE()->getName()<<"\n";
		bool route_succ = SARoute(selected_dfg_node, dp_candidate);

		//process the child node
		if(route_succ && child_node!= NULL  ){
			LOG(SA)<<"----------be careful. Processing the child node "<<child_node->idx<<"\n";
			std::priority_queue<dest_with_cost> estimatedRoutes;
			DFGNode *failedNode;
			bool isEstRouteSucc = estimateRouting(child_node, estimatedRoutes, &failedNode);
			bool isRouteSucc = false;
			if (isEstRouteSucc)
			{
				isRouteSucc = Route(child_node, estimatedRoutes, &failedNode);
			}
			if(!isRouteSucc){
				clearNodeMapping(child_node);
			}
		}

		//decide accept or node
    bool accept = false;
		int attempted_cost = getCost();
		if(route_succ && child_node!= NULL && dfg_node_placement.find(child_node)!= dfg_node_placement.end()){
			// if route success, has a child_node and successfully route
			accept = true;
		}else if(route_succ){
			// if not route success, then nothing changes and should restore mapping
			accept = whetherAcceptNewMapping(attempted_cost,curr_cost,  curr_temp); 
		}
    
		LOG(SA)<<"accept "<<accept<<" route_succ:"<<route_succ<<" curr_cost:"<<curr_cost<<" attepmted cost:"<<attempted_cost<<"\n";

    if(accept){
      //update overuse
      // save the mapping state
			curr_cost =  attempted_cost;
			accepted_number ++;

			old_dfg_node_placement.clear();
			old_data_routing_path.clear();
			if(isCurrMappingValid()){
				std::cout<<"find a valid mapping, exit inner_map....\n";
				break;
			}
			
    }else{
			clearNodeMapping(selected_dfg_node);
			restoreMapping(selected_dfg_node, old_dfg_node_placement, old_data_routing_path);
			dfg_node_placement.clear();
			dfg_node_placement.insert(old_dfg_node_placement.begin(), old_dfg_node_placement.end());
			data_routing_path.clear();
			data_routing_path.insert(old_data_routing_path.begin(), old_data_routing_path.end());
			old_dfg_node_placement.clear();
			old_data_routing_path.clear();
    }
		
  }
	return float(accepted_number)/movement_in_each_temp;
 
}


CGRAXMLCompile::DFGNode * CGRAXMLCompile::SAMapper::selectDFGNodeToUnmap(){
	std::vector<DFGNode*> unmapped_node;
	for(auto node: sortedNodeList)
	{
		if(dfg_node_placement.find(node) == dfg_node_placement.end()){
			unmapped_node.push_back(node);
		}
	}
	std::random_device r;
	std::default_random_engine e1(r());

	
	if(unmapped_node.size() == 0){
 	  std::uniform_int_distribution<int> uniform_dist(0, sortedNodeList.size()-1);
		return sortedNodeList[std::abs(std::round(uniform_dist(e1)))];
	}else{
		std::uniform_int_distribution<int> uniform_dist(0, unmapped_node.size()-1);
		return unmapped_node[std::abs(std::round(uniform_dist(e1)))];
	}
}	

CGRAXMLCompile::DFGNode* CGRAXMLCompile::SAMapper::selectAParentForDFGNode(DFGNode* target_node){
	// the parent node include: nodes has data dependency, or ones which has recurrent dependency (load/Store constraint)
	std::vector<DFGNode*> candidates;
	candidates.insert( candidates.end(), target_node->parents.begin(), target_node->parents.end() );
	for(auto node: sortedNodeList){
		auto rec_parent = node->recParents;
		// as the rec_parent is reversed in DFG, the rec_parent means rec_child actually.
		if ( std::find(rec_parent.begin(), rec_parent.end(), target_node) != rec_parent.end() ){
			candidates.push_back(node);
		}
	}
	assert(candidates.size() > 0);
	std::random_device r;
	std::default_random_engine e1(r());
	std::uniform_int_distribution<int> uniform_dist(0, candidates.size()-1);
	return candidates[std::abs(std::round(uniform_dist(e1)))];
}


bool CGRAXMLCompile::SAMapper::clearNodeMapping(DFGNode * node){
	bool mapped = (dfg_node_placement.find(node) != dfg_node_placement.end());
	if(mapped){
		dfg_node_placement.erase(dfg_node_placement.find(node));
	}
	bool routed = false;
	for(auto it = data_routing_path.begin(); it != data_routing_path.end();){
		LOG(SA)<<"routing path:" << it->first.first->idx<<" -> "<<it->first.second->idx;
		if(it->first.first == node || it->first.second == node ){
			routed = true;
			it = data_routing_path.erase(it);
		}
		else{
			it ++ ;
		}
	}
	// assert(mapped ==  routed);

	node->clear(this->dfg);
	return true;

}


bool CGRAXMLCompile::SAMapper::restoreMapping(DFGNode *node, 	std::map<DFGNode*, std::pair<DataPath*, int>> & dfg_node_placement, 
std::map<dfg_data, std::vector<LatPort>>& data_routing_path){
	//assign placement
	assert(dfg_node_placement.find(node)!= dfg_node_placement.end());
	auto placement_info = dfg_node_placement[node];
	placement_info.first->assignNode(node, placement_info.second, this->dfg);
	node->rootDP = placement_info.first;

	//assign routing
	for(auto & data_path: data_routing_path){
		if(data_path.first.first == node || data_path.first.second == node){
		 assignPath(data_path.first.first, data_path.first.second,  data_path.second);
		}
	}

	return true;
}

int CGRAXMLCompile::SAMapper::getCongestionNumber(){
	int congestion_number = 0;
	
	for (std::pair<Port *, std::set<DFGNode *>> pair : congestedPorts)
	{
		Port *p = pair.first;
		if (pair.second.size() > 1)
		{
			for (DFGNode *node1 : pair.second)
			{
				for (DFGNode *node2 : pair.second)
				{
					if (node1 == node2)
					{
						continue;
					}
					if (this->dfg->isMutexNodes(node1, node2, p))
						continue;
					LOG(ROUTE) << "CONGESTION:" << p->getFullName();
					for (DFGNode *node : pair.second)
					{
						LOG(ROUTE) << "," << node->idx << "|BB=" << node->BB;
					}
					LOG(ROUTE) << "\n";
					congestion_number ++;
					//					break;
				}
			
			}
		}
	}
	return congestion_number;
}

int CGRAXMLCompile::SAMapper::getPortUsage(){
	int usage_number = 0;
	
	for (std::pair<Port *, std::set<DFGNode *>> pair : congestedPorts)
	{
		usage_number += pair.second.size();
		
	}
	return usage_number;
}

int CGRAXMLCompile::SAMapper::getCost(){
	int unmapped_node_number = getNumberOfUnmappedNodes();
	int port_usage = getPortUsage();
	int congestion_number = getCongestionNumber();
	int total_cost = unmapped_node_number *  100 + congestion_number * 10 + port_usage  ;
	LOG(ROUTE)<<"unmapped node:"<<unmapped_node_number<<" port usage:"<<port_usage<<" congestion:"<<congestion_number<<" total cost:"<<total_cost<<"\n";
	return total_cost;
}

bool CGRAXMLCompile::SAMapper::SARoute(DFGNode *node, DataPath * candidateDP){
	std::map<DFGNode *, std::vector<Port *>> parent_output_ports;
	std::map<DFGNode *, Port *> alreadyMappedChildPorts;

	int minLat = 0;
	int II = 0;

	// this part if from PathFinder
	{
		for (DFGNode *parent : node->parents)
		{
			//		std::cout << "parent = " << parent->idx << "\n";
			if (parent->rootDP != NULL)
			{ //already mapped
				assert(parent->rootDP->getOutputDP()->getOutPort("T"));
				parent_output_ports[parent].push_back(parent->rootDP->getOutputDP()->getOutPort("T"));
			}
		}

		for (DFGNode *child : node->children)
		{
			if (child->rootDP != NULL)
			{ // already mapped
				LOG(ROUTE) << "child=" << child->idx << ",childOpType=" << node->childrenOPType[child] << "\n";
				assert(child->rootDP->getLat() != -1);
				if (node->childrenOPType[child] == "PS")
				{
					LOG(ROUTE) << "Skipping.....\n";
					continue;
				}
				assert(child->rootDP->getInPort(node->childrenOPType[child]));
				alreadyMappedChildPorts[child] = child->rootDP->getInPort(node->childrenOPType[child]);

				int ii = child->rootDP->getCGRA()->get_t_max();
				assert(child->rootDP->getLat() != -1);
				alreadyMappedChildPorts[child]->setLat(child->rootDP->getLat() + ii); //next iteration?
			}
			else if(child->idx == node->idx){
				//adding a placeholder as this will be modified according to the destination in consideration.
				alreadyMappedChildPorts[child] == NULL;
			}
		}

		minLat = getlatMinStartsPHI(node, parent_output_ports);
		PE *pe = candidateDP->getPE();
		CGRA *cgra = candidateDP->getCGRA();
		II= cgra->get_t_max();
		int t = pe->T;
		int minLatmodii = minLat % II;

		if (minLatmodii > t)
		{
			minLat += II + (t - minLatmodii);
		}
		else
		{
			minLat += t - minLatmodii;
		}
	}
	LOG(ROUTE)<<"***routing destination:"<<candidateDP->getFullName()<<"\n";
  //check latency constraint before mapping
	bool route_success = false; 
	for(int iteration  = 0;iteration < maximum_routing_iteration; iteration++){
		 //
		LOG(ROUTE)<<"routing iteration:"<<iteration<<"\n";
		int minLatDestVal_prime = minLat + II * iteration;

		LatPort candiLatport ; // get a latency according to the iteration
	
		std::map<DFGNode *, std::vector<LatPort>> mappedChildPaths;
		
		FU *parentFU = candidateDP->getFU();
		assert(parentFU->supportedOPs.find(node->op) != parentFU->supportedOPs.end());
		int latency = parentFU->supportedOPs[node->op];
		int minLatDestVal = minLatDestVal_prime;
		//route parent nodes
		for(auto& parent_info: parent_output_ports){
			auto parent = parent_info.first;
			minLatDestVal = minLatDestVal_prime + parent->childNextIter[node] * II;
			if (parent->getOPtype(node) == "PS"){
				LOG(ROUTE)<< "Skipping.....\n";
				continue;
			}
					
			Port *destPort = candidateDP->getInPort(parent->getOPtype(node));
			LatPort destPortLat = std::make_pair(minLatDestVal + latency, destPort);
			if (!canExitCurrPE(destPortLat))
			{
					LOG(ROUTE)<< "cannot exit current PE. Continue.....\n";
					route_success = false;
					break;
			}
			auto startCand = parent_info.second.front();
			LatPort startCandLat = std::make_pair(startCand->getLat(), startCand);
			int cost; 
			std::vector<LatPort> path;
			std::map<Port *, std::set<DFGNode *>> mutexPaths;
			route_success  = LeastCostPathAstar(startCandLat, destPortLat, candidateDP, path, cost, parent, mutexPaths, node);
			
			if(route_success){
				assignPath(parent, node, path);
				data_routing_path.emplace(std::make_pair(parent, node),path );
				LOG(ROUTE)<<"routing success from " <<parent->idx<<","<< startCand->getFullName()<<","<<startCandLat.first<<" to "
				<<node->idx<<","<< destPort->getFullName()<<","<<destPortLat.first<<"\n";
			}else{
				LOG(ROUTE)<<"routing failure from " <<parent->idx<<","<< startCand->getFullName()<<","<<startCandLat.first<<" to "
				<<node->idx<<","<< destPort->getFullName()<<","<<destPortLat.first<<"\n";
				break;
			}
		}

		if(!route_success){
			node->clear(this->dfg);
			continue;
		}


		//route child nodes
		for (std::pair<DFGNode *, Port *> pair : alreadyMappedChildPorts)
		{
			DFGNode *child = pair.first;
			Port *childDestPort = pair.second;
			DataPath* childDP = child->rootDP;

			if (node->childrenOPType[child] == "PS")
			{
				LOG(ROUTE)<< "Skipping.....\n";
				continue;
			}

			if (child->idx == node->idx)
			{
				childDestPort = candidateDP->getInPort(node->childrenOPType[child]);
				// if (detailedDebug) cout << "setting latency = " << minLatDestVal + ii << "\n";
				childDestPort->setLat(minLatDestVal + II);
				childDP = candidateDP;
			}

			std::vector<LatPort> path;
			int cost;

			FU *parentFU = candidateDP->getFU();
			assert(parentFU->supportedOPs.find(node->op) != parentFU->supportedOPs.end());
			int latency = parentFU->supportedOPs[node->op];
			Port *destPort = candidateDP->getOutputPort(latency);

			std::map<Port *, std::set<DFGNode *>> mutexPaths;
			// if (detailedDebug)
			// 	std::cout << "already child Estimating Path" << destPort->getFullName() << "," << minLatDestVal + latency << ","
			// 				<< "--->" << childDestPort->getFullName() << "," << childDestPort->getLat() << "," << "exist_child = " << child->idx  
			// 				<< "\n";
			// if (detailedDebug)
			// 	std::cout << "lat = " << childDestPort->getLat() << ",PE=" << childDestPort->getMod()->getPE()->getName() << ",t=" << childDestPort->getMod()->getPE()->T << "\n";

			LatPort childDestPortLat = std::make_pair(childDestPort->getLat(), childDestPort);
			assert(childDestPort->getLat() != -1);
			LatPort destPortLat = std::make_pair(minLatDestVal + latency, destPort);

			route_success  = LeastCostPathAstar(destPortLat, childDestPortLat, childDP, path, cost, node, mutexPaths, child);

			if(route_success){
				assignPath(node, child, path);
				data_routing_path.emplace(std::make_pair(node, child),path );
			}else{
				break;
			}
		}
		if(!route_success){
			node->clear(this->dfg);
			continue;
		}


		candidateDP->assignNode(node, minLatDestVal, this->dfg);
		dfg_node_placement.emplace(node, std::make_pair(candidateDP, minLatDestVal));
		node->rootDP = candidateDP;
		break;
	}
	// if(route_success) 
	return route_success;
  

}



std::vector<CGRAXMLCompile::DataPath*> CGRAXMLCompile::SAMapper::getRandomDPCandidate(DFGNode *node){
  std::vector<DataPath *> candidateDests;
	int penalty = 0;
	std::map<DataPath *, int> dpPenaltyMap;
  unordered_set<PE *> allPEs = cgra->getAllPEList();
	for (PE *currPE : allPEs)
	{
		for (Module *submod : currPE->subModules)
		{
			if (FU *fu = dynamic_cast<FU *>(submod))
			{

				if (fu->supportedOPs.find(node->op) == fu->supportedOPs.end())
				{
					continue;
				}

				if (fu->currOP.compare(node->op) == 0)
				{
					for (Module *submodFU : fu->subModules)
					{
						if (DataPath *dp = dynamic_cast<DataPath *>(submodFU))
						{

							if(cgra->is_spm_modelled){
								if(!node->base_pointer_name.empty()){
									//base pointer name is not empty
									if(dp->accesible_memvars.find(node->base_pointer_name) == dp->accesible_memvars.end()){
										//this dp does not support the variable
										continue;
									}
								}
							}
							
							if (checkDPFree(dp, node, penalty))
							{
								//									if(dp->getMappedNode()==NULL){
								//									if(dataPathCheck(dp,&node)){

								if (node->blacklistDest.find(dp) == node->blacklistDest.end())
								{
									candidateDests.push_back(dp);
									dpPenaltyMap[dp] = penalty;
								}
							}
						}
					}
				}
				else if (fu->currOP.compare("NOP") == 0)
				{
					for (Module *submodFU : fu->subModules)
					{
						if (DataPath *dp = dynamic_cast<DataPath *>(submodFU))
						{

							bool is_mem_op = node->op.find("LOAD") != string::npos || node->op.find("STORE") != string::npos;
							if(cgra->is_spm_modelled){
								if(!node->base_pointer_name.empty() && is_mem_op){
									//base pointer name is not empty
									if(dp->accesible_memvars.find(node->base_pointer_name) == dp->accesible_memvars.end()){
										//this dp does not support the variable
										LOG(ROUTE) << "memvar=" << node->base_pointer_name <<  " is not supported in " << dp->getFullName() << "\n";
										continue;
									}
								}
							}

							if (checkDPFree(dp, node, penalty))
							{
								//									if(dp->getMappedNode()==NULL){
								//									if(dataPathCheck(dp,&node)){

								if (node->blacklistDest.find(dp) == node->blacklistDest.end())
								{
									candidateDests.push_back(dp);
									dpPenaltyMap[dp] = penalty;
								}
							}
						}
					}
				}
			}
		}
	}

  unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();

  shuffle (candidateDests.begin(), candidateDests.end(), std::default_random_engine(seed));

  return candidateDests;


}

//compated to the PathFinderMapper one, add some additional information for SA 
bool CGRAXMLCompile::SAMapper::Route(DFGNode *node,
											 std::priority_queue<dest_with_cost> &estimatedRoutes,
											 DFGNode **failedNode)
{

	LOG(ROUTE) << "Route begin...\n";

	int parentRoutingPortCount = 0;
	int routedParents = 0;

	for (DFGNode *parent : node->parents)
	{
		int thisParentNodeCount = 0;
		if (parent->rootDP != NULL)
		{
			thisParentNodeCount = parent->routingPorts.size();
		}

		//		if(thisParentNodeCount>0){
		//			routedParents++;
		//			thisParentNodeCount--; //remove the T port in the cout
		//		}
		parentRoutingPortCount += thisParentNodeCount;
	}
	//	if(parentRoutingPortCount>0){
	//		parentRoutingPortCount-=1; //remove the T port in the cout
	//	}

	int addedRoutingParentPorts = 0;

	bool routeSucc = false;
	dest_with_cost currDest;
	while (!estimatedRoutes.empty())
	{
		currDest = estimatedRoutes.top();
		estimatedRoutes.pop();

		if (currDest.dest->getMappedNode() != NULL)
		{
			LOG(ROUTE)<< "currDest is not NULL \n";
			LOG(ROUTE)<< "currDP:" << currDest.dest->getName() << ",currPE:" << currDest.dest->getPE()->getName() << "\n";
			LOG(ROUTE)<< "currNode:" << currDest.dest->getMappedNode()->idx << "\n";
		}
		assert(currDest.dest->getMappedNode() == NULL);
		LOG(ROUTE)<< "alreadyMappedChilds = " << currDest.alreadyMappedChilds.size() << "\n";

		bool alreadMappedChildRouteSucc = true; //this will change to false if failure in alreadyMappedChilds
		std::map<DFGNode *, std::vector<LatPort>> mappedChildPaths;
		std::map<DFGNode *, std::map<Port *, std::set<DFGNode *>>> mappedChildMutexPaths;
		while (!currDest.alreadyMappedChilds.empty())
		{
			dest_child_with_cost dest_child_with_cost_ins = currDest.alreadyMappedChilds.top();
			currDest.alreadyMappedChilds.pop();

			std::vector<LatPort> possibleStarts;
			possibleStarts.clear();
			possibleStarts.push_back(dest_child_with_cost_ins.startPort);
			for (std::pair<Port *, int> pair : node->routingPorts)
			{
				possibleStarts.push_back(std::make_pair(pair.first->getLat(), pair.first));
				assert(pair.first->getLat() != -1);
			}

			std::priority_queue<cand_src_with_cost> q;
			std::map<Port *, std::set<DFGNode *>> mutexPathsTmp;
			std::vector<LatPort> pathTmp;
			for (LatPort p : possibleStarts)
			{
				int cost;
				if (LeastCostPathAstar(p, dest_child_with_cost_ins.childDest, dest_child_with_cost_ins.childDP, pathTmp, cost, node, mutexPathsTmp, dest_child_with_cost_ins.child))
				{
					pathTmp.clear();
					q.push(cand_src_with_cost(p, dest_child_with_cost_ins.childDest, cost));
				}
			}

			int cost;
			std::vector<LatPort> path;
			LatPort src = dest_child_with_cost_ins.startPort;
			LatPort dest = dest_child_with_cost_ins.childDest;

			while (!q.empty())
			{
				cand_src_with_cost head = q.top();
				q.pop();
				std::map<Port *, std::set<DFGNode *>> mutexPaths;
				alreadMappedChildRouteSucc = LeastCostPathAstar(head.src, dest, dest_child_with_cost_ins.childDP, path, cost, node, mutexPaths, dest_child_with_cost_ins.child);
				if (alreadMappedChildRouteSucc)
				{
					assignPath(node, dest_child_with_cost_ins.child, path);
					data_routing_path.emplace(std::make_pair(node, dest_child_with_cost_ins.child),path );
					mappedChildPaths[dest_child_with_cost_ins.child] = path;
					mappedChildMutexPaths[dest_child_with_cost_ins.child] = mutexPaths;
					LOG(ROUTE)<< "Route success :: from=" << src.second->getFullName() << "--> to=" << dest.second->getFullName() << "|node=" << node->idx << "\n";
					break;
				}
				else
				{
					LOG(ROUTE)<< "Route Failed :: from=" << src.second->getFullName() << "--> to=" << dest.second->getFullName() << "\n";
					for (LatPort p : path)
					{
						if (p.second->getMod()->getPE())
						{
							LOG(ROUTE)<< p.second->getMod()->getPE()->getName() << "-->";
						}
					}
					LOG(ROUTE)<< "\n";

					for (LatPort p : path)
					{
						LOG(ROUTE)<< p.second->getFullName() << "\n";
					}
				}
				path.clear();
			}
			if (!alreadMappedChildRouteSucc)
			{
				*failedNode = dest_child_with_cost_ins.child;
				break;
			}
		}

		if (alreadMappedChildRouteSucc)
		{
			for (std::pair<Port *, int> pair : node->routingPorts)
			{
				Port *p = pair.first;
				int destIdx = pair.second;
				LOG(ROUTE)<< "to:" << destIdx << "," << p->getFullName() << "\n";
			}
		}

		if (!alreadMappedChildRouteSucc)
		{
			node->clear(this->dfg);
			continue; //try the next dest
		}
		else
		{
			std::stringstream ss;
			ss<< "Already Mapped child Routes....\n";
			for (std::pair<DFGNode *, std::vector<LatPort>> pair : mappedChildPaths)
			{
				DFGNode *child = pair.first;
				for (LatPort lp : pair.second)
				{
					Port *p = lp.second;
					ss<< "to:" << child->idx << " :: ";
					ss<< p->getFullName();
					if (mappedChildMutexPaths[child].find(p) != mappedChildMutexPaths[child].end())
					{
						ss<< "|mutex(";
						for (DFGNode *mutexnode : mappedChildMutexPaths[child][p])
						{
							ss<< mutexnode->idx << ",";
						}
						ss<< ")";
					}
					ss<< "\n";
				}
				ss<< "\n";
			}
			ss<< "\n";
			LOG(ROUTE)<<ss.str();
		}

		bool parentRoutSucc = true;
		addedRoutingParentPorts = 0;
		std::map<DFGNode *, std::map<Port *, std::set<DFGNode *>>> mappedParentMutexPaths;
		while (!currDest.parentStartLocs.empty())
		{
			parent_cand_src_with_cost pcswc = currDest.parentStartLocs.top();
			currDest.parentStartLocs.pop();
			DFGNode *parent = pcswc.parent;
			std::priority_queue<cand_src_with_cost> &q = pcswc.cswc;

			bool succ = false;
			while (!q.empty())
			{
				cand_src_with_cost cand_src_with_cost_ins = q.top();
				q.pop();
				LatPort src = cand_src_with_cost_ins.src;
				LatPort dest = cand_src_with_cost_ins.dest;
				std::vector<LatPort> path;
				std::map<Port *, std::set<DFGNode *>> mutexPath;
				int cost;
				succ = LeastCostPathAstar(src, dest, currDest.dest, path, cost, parent, mutexPath, node);
				if (succ)
				{
					//					bool routedParent=true;
					//					if(parent->routingPorts.size()==0){ //unrouted parent
					//						routedParent=false;
					//					}
					assignPath(parent, node, path);
					data_routing_path.emplace(std::make_pair(parent, node),path );

					mappedParentMutexPaths[parent] = mutexPath;
					addedRoutingParentPorts += path.size();
					//					if(routedParent){
					addedRoutingParentPorts -= 1;
					//					}
					//					for(Port* p : path){
					//						std::cout << p->getFullName() << ",\n";
					//					}
					//					std::cout << "\n";
					break;
				}
				else
				{
					addedRoutingParentPorts = 0;
					node->clear(this->dfg);
					LOG(ROUTE)<< "Route Failed :: from=" << src.second->getFullName() << "--> to=" << dest.second->getFullName() << "\n";
				}
				path.clear();
			}
			if (!succ)
			{
				*failedNode = parent;
				node->clear(this->dfg);
				addedRoutingParentPorts = 0;
				parentRoutSucc = false; // at least one parent failed to route, try a new dest
				break;
			}
		}

		if (parentRoutSucc)
		{ //all parents routed succesfull + all mapped childs are connected
			routeSucc = true;
			std::stringstream ss;
			ss<< "node=" << node->idx << ",op=" << node->op << " is mapped to " << currDest.dest->getPE()->getName() << ",lat=" << currDest.destLat << "\n";
			ss<< "routing info ::\n";
			for (DFGNode *parent : node->parents)
			{
				ss<< "parent routing port size = " << parent->routingPorts.size() << "\n";
				int prev_lat = -1;
				for (std::pair<Port *, int> pair : parent->routingPorts)
				{
					Port *p = pair.first;
					//					if(node.routingPortDestMap[p]==&node){
					ss<< "fr:" << parent->idx << " :: ";
					ss<< ",dest=" << pair.second << " :: ";
					ss<< p->getFullName();
					ss<< ",lat=" << p->getLat();

					if (mappedParentMutexPaths[parent].find(p) != mappedParentMutexPaths[parent].end())
					{
						ss<< "|mutex(";
						for (DFGNode *mutexnode : mappedParentMutexPaths[parent][p])
						{
							ss<< mutexnode->idx << ",";
						}
						ss<< ")";
					}
					ss<< std::endl;
					//					}
					if (prev_lat != -1)
					{
						//							assert(p->getLat() - prev_lat <= 1);
					}
					prev_lat = p->getLat();
				}
			}
			ss<< "routing info done.\n";
			currDest.dest->assignNode(node, currDest.destLat, this->dfg);
			node->rootDP = currDest.dest;
			dfg_node_placement.emplace(node, std::make_pair(currDest.dest, currDest.destLat));

			mappingLog4 << node->idx << "," << currDest.dest->getPE()->X << ","<< currDest.dest->getPE()->Y << "," << currDest.destLat << "\n";
			ss << "mappingLog4=" << node->idx << "," << currDest.dest->getPE()->X << ","<< currDest.dest->getPE()->Y << "," << currDest.destLat << "\n";
			LOG(ROUTE)<<ss.str();
			
			break;
		}
		node->clear(this->dfg);
	}

	if (routeSucc)
	{
		LOG(ROUTE)<< "Route success...\n";

		int parentRoutingPortCountEnd = 0;
		//		int mappedParentCount=0;
		for (DFGNode *parent : node->parents)
		{
			if (parent->rootDP != NULL)
			{
				//				mappedParentCount++;
				parentRoutingPortCountEnd += parent->routingPorts.size();
			}
		}
		parentRoutingPortCountEnd = std::max(0, parentRoutingPortCountEnd - routedParents);
		if (parentRoutingPortCountEnd != parentRoutingPortCount + addedRoutingParentPorts)
		{
			LOG(ROUTE)<< "parentRoutingPortCountEnd=" << parentRoutingPortCountEnd << "\n";
			LOG(ROUTE)<< "addedRoutingParentPorts=" << addedRoutingParentPorts << "\n";
			LOG(ROUTE)<< "parentRoutingPortCount=" << parentRoutingPortCount << "\n";
		}

		//		assert(parentRoutingPortCountEnd==parentRoutingPortCount+addedRoutingParentPorts);
		return true;
	}
	else
	{
		currDest.dest->assignNode(node, currDest.destLat, this->dfg);
		dfg_node_placement.emplace(node, std::make_pair(currDest.dest, currDest.destLat));
		node->rootDP = currDest.dest;
		node->clear(this->dfg);
		LOG(ROUTE)<< "Route failed...\n";

		int parentRoutingPortCountEnd = 0;
		//		int mappedParentCount=0;
		for (DFGNode *parent : node->parents)
		{
			if (parent->rootDP != NULL)
			{
				//				mappedParentCount++;
				parentRoutingPortCountEnd += parent->routingPorts.size();
			}
		}
		parentRoutingPortCountEnd = std::max(0, parentRoutingPortCountEnd - routedParents);
		if (parentRoutingPortCountEnd != parentRoutingPortCount + addedRoutingParentPorts)
		{
			LOG(ROUTE)<< "parentRoutingPortCountEnd=" << parentRoutingPortCountEnd << "\n";
			LOG(ROUTE)<< "addedRoutingParentPorts=" << addedRoutingParentPorts << "\n";
			LOG(ROUTE)<< "parentRoutingPortCount=" << parentRoutingPortCount << "\n";
		}
		//		assert(parentRoutingPortCountEnd==parentRoutingPortCount);
		assert(*failedNode != NULL);
		return false;
	}
}


std::string CGRAXMLCompile::SAMapper::mapStatus(){
	std::stringstream ss;
	for(auto node: sortedNodeList)
	{
		if(dfg_node_placement.find(node) == dfg_node_placement.end()){
			ss<<"node "<<node->idx<<" not placed.\n";
		}else{
			auto p = dfg_node_placement[node];
			ss<<"node "<<node->idx<<" placed on "<< p.first->getFullName()<<" lat:"<<p.second<<"\n";
		}
	}
	return ss.str();
}

int CGRAXMLCompile::SAMapper::getNumberOfUnmappedNodes(){
	int n = 0;
	for(auto node: sortedNodeList)
	{
		if(dfg_node_placement.find(node) == dfg_node_placement.end()){
			n++;
		}
	}
	return n;
}
