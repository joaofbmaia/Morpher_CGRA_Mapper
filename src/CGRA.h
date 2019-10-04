/*
 * cgra.h
 *
 *  Created on: 20 Feb 2018
 *      Author: manupa
 */

#ifndef CGRA_H_
#define CGRA_H_

#include "Module.h"
#include "PE.h"
#include "FU.h"
#include "DataPath.h"
#include <memory>
#include <nlohmann/json.hpp>

using namespace std;
using json = nlohmann::json;


namespace CGRAXMLCompile
{

class DFG;

class CGRA : public Module
{
public:
	CGRA(const Module *Parent, std::string name, int t, int y, int x, DFG *dfg, std::string peType = "GENERIC_8REGF", int numberofDPs = 1, std::map<Port *, std::set<DFGNode *>> *_congestedPortPtr = NULL) : Module(Parent, name, t)
	{
		createGenericCGRA(x, y, t, peType, numberofDPs);
		this->peType = peType;
		this->numberofDPs = numberofDPs;
		this->congestedPortPtr = _congestedPortPtr;

		for (int t = 0; t < this->get_t_max(); ++t)
		{
			for (int y = 0; y < this->y_max; ++y)
			{
				for (int x = 0; x < this->x_max; ++x)
				{
					PE *pe = this->PEArr[t][y][x];
					for (Module *submod : pe->subModules)
					{
						if (FU *fu = dynamic_cast<FU *>(submod))
						{
							if (fu->supportedOPs.find("LOAD") != fu->supportedOPs.end())
							{
								for (Module *submod2 : fu->subModules)
								{
									if (DataPath *dp = dynamic_cast<DataPath *>(submod2))
									{
										if (dp->getMappedNode() == NULL)
										{
											freeMemNodes++;
											freeMemNodeSet.insert(dp);
										}
									}
								}
							}
						}
					}
				}
			}
		}
		currDFG = dfg;
	}

	CGRA(const Module *Parent, std::string name, int t) : Module(Parent,name,t){}
	CGRA(std::string json_filename, int II,  std::map<Port *, std::set<DFGNode *>> *_congestedPortPtr = NULL) : Module(NULL,"CGRA_Ins",0){
		json_file = json_filename;
		this->congestedPortPtr = _congestedPortPtr;
		ParseJSONArch(json_filename,II);
	}

	void createGenericCGRA(int x, int y, int t, std::string peType = "GENERIC_8REGF", int numberofDPs = 1);

	void adjustII(int newII);
	int get_t_max() { return t_max; }
	// int get_y_max() { return y_max; }
	// int get_x_max() { return x_max; }

	std::map<Port *, std::set<DFGNode *>> *getCongestedPortPtr() { return congestedPortPtr; }
	void setCongestedPortPtr(std::map<Port *, std::set<DFGNode *>> *congestedPortPtr) { this->congestedPortPtr = congestedPortPtr; }

	//	bool isGenericPE=false;
	std::string peType;
	int numberofDPs = 1;
	int freeMemNodes = 0;
	std::set<DataPath *> freeMemNodeSet;

	std::set<Port *> getConflictPorts(Port *p);
	void insertConflictPort(Port *a, Port *b);
	bool isConflictPortsEmpty(Port *p) { return conflictPorts[p].empty(); }
	DFG *currDFG = NULL;

	int minLatBetweenPEs = 1;

	Module* ParseModule(json& module_desc, Module* parent, string module_name, string type, int t, int x = -1, int y = -1);
	void ParseCGRA(json& cgra_desc, int II);
	unordered_map<int,std::vector<Module*>> subModArr;


	bool ParseJSONArch(string fileName, int II);
	bool PreprocessPattern(json& top);
	json top_desc;

	unordered_set<PE*> getAllPEList();
	vector<PE*> getSpatialPEList(int t);
	PE* getPE(int t, int y, int x);
	PE* getLatencyPE(PE* currPE, int lat);

	void insertGlobalOP(string OP, int lat);
	unordered_map<string,int> getGlobalOPMinLatencyMap(){return GlobalOPMinLatencyMap;}
	string getCGRAName();


private:
	int x_max;
	int y_max;
	int t_max;
	std::map<Port *, std::set<Port *>> conflictPorts;
	std::map<Port *, std::set<DFGNode *>> *congestedPortPtr;

	std::map<int, std::map<int, std::map<int, PE *>>> PEArr;
	unordered_map<PE*,PE*> NextCyclePEMap;
	unordered_map<string,int> GlobalOPMinLatencyMap;
	string json_file;


};

} /* namespace CGRAXMLCompile */

#endif /* CGRA_H_ */
