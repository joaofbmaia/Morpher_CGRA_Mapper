/*
 * Module.h
 *
 *  Created on: 26 Feb 2018
 *      Author: manupa
 */

#ifndef MODULE_H_
#define MODULE_H_

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <unordered_set>

#include "Port.h"

//#include <nlohmann/json.hpp>
#include "json.hpp"


using namespace std;
using json = nlohmann::json;

namespace CGRAXMLCompile
{

class CGRA;
class PE;
class FU;
class HeuristicMapper;
typedef std::pair<int, Port *> LatPort;

class Module
{
public:
	Module(const Module *Parent, std::string name, string type, int t);
	virtual ~Module();
	std::vector<Port *> inputPorts;
	std::vector<Port *> outputPorts;
	std::vector<Port *> internalPorts;
	std::vector<Port *> socketPorts;
	std::vector<Module *> subModules;

	Port *getInPort(std::string Pname);
	Port *getOutPort(std::string Pname);
	Port *getInternalPort(std::string Pname);
	Port* getSocketPort(std::string Pname);
	std::pair<Port *, Port *> getRegPort(std::string Pname);
	Port* getSingleRegPort(std::string Pname);
	Port* getSingleRegPort2(std::string Pname);
	Module *getSubMod(std::string Mname);

	std::string getName() { return name; }
	std::string getFullName();
	Module *getParent() { return (Module *)Parent; }

	CGRA *getCGRA();
	PE *getPE();
	FU *getFU();

	std::vector<LatPort> getNextPorts(LatPort currPort, HeuristicMapper *hm);
	std::vector<Port *> getNextPorts(Port *currPort);
	std::vector<Port *> getFromPorts(Port *currPort, HeuristicMapper *hm);
	std::vector<Port *> getFromPorts(Port *currPort);

	std::set<Port *> getConflictPorts(Port *currPort);
	bool isConflictPortsEmpty(Port *p);

	void insertConnection(Port *src, Port *dest);
	void insertConnection(Port *src, std::pair<Port*,Port*> regDest);
	void insertConnection(std::pair<Port*,Port*> regSrc, Port* src);

	std::map<std::pair<Port *, Port *>, bool> regCons;
	void insertRegPort(std::string pName);

	unordered_map<string,Port*> Name2Port;
	unordered_map<string,pair<Port*,Port*>> Name2RegPort;
	unordered_map<string,Module*> Name2SubMod;
	int get_t(){return t;}

	void attachNextTimeIns(Module* nt){nextTimeIns = nt;}
	Module* getNextTimeIns(){return nextTimeIns;}
	string get_type(){return type;}
	void UpdateMappedConnectionsJSON(json& output_json);
	void UpdateMappedConnectionsPillars(json& output_json, ofstream& outFile_i);

	Port* getJSONPort(string pname, bool isSrc);

	bool isSPM = false;
	std::unordered_set<std::string> data_layout;
	const std::map<Port *, std::vector<Port *>> getconnectedFrom(){return connectedFrom;}
	const std::map<Port *, std::vector<Port *>> getconnectedTo(){return connectedTo;}
protected:
	//private:

	std::map<Port *, std::vector<Port *>> connectedTo;
	std::map<Port *, std::vector<Port *>> connectedFrom;
	const Module *Parent;
	std::string name;
	Module* nextTimeIns;
	string type;

	int t;

private:
	std::vector<std::pair<Port *, Port *>> regPorts;
};

} /* namespace CGRAXMLCompile */

#endif /* MODULE_H_ */
