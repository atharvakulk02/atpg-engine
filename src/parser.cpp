#include <string>
#include <fstream>
#include "../include/gate.h"
#include "../include/circuit.h"
#include "../include/parser.h"
#include <sstream>
using namespace std;

GateType stringToGateType(string s) {
    if (s == "AND")  return AND;
    if (s == "NAND") return NAND;
    if (s == "OR")   return OR;
    if (s == "NOR")  return NOR;
    if (s == "XOR")  return XOR;
    if (s == "XNOR") return XNOR;
    if (s == "NOT")  return NOT;
    if (s == "BUFF") return NOT; // buffer, treat as wire
    return INPUT; // fallback
}

Circuit parse(string fileName){
    Circuit circuit;
    ifstream file(fileName);
    string line;
    while (getline(file,line)){
        if (line.find("#")!=string::npos){

        }else if(line.find("INPUT")!=string::npos){
            int start=line.find("(")+1;
            int end=line.find(")");
            string pos=line.substr(start,end-start);
            Gate g;
            g.name=pos;
            g.type=INPUT;
            g.output=pos;
            g.value=X;
            circuit.gates[pos]=g;
            circuit.inputs.push_back(pos);
        }else if(line.find("OUTPUT")!=string::npos){
            int start=line.find("(")+1;
            int end=line.find(")");
            string pos=line.substr(start,end-start);
            Gate o;
            o.name=pos;
            o.type=OUTPUT;
            o.value=X;
            circuit.gates[pos]=o;
            circuit.outputs.push_back(pos);
        }else{
            int eq=line.find("=");
            int obr=line.find("(");
            string gatename=line.substr(eq+2,(obr-(eq+2)));
            string name=line.substr(0,eq);
            name=name.substr(0,name.find_last_not_of(" ")+1);
            int inplen=line.find(")");
            string inpname=line.substr(obr+1,inplen-(obr+1));
            stringstream ss(inpname);
            string token;
            Gate g;
            g.value=X;
            g.name=name;
            g.output=name;
            g.type = stringToGateType(gatename);
            while(getline(ss,token,',')){
                token = token.substr(token.find_first_not_of(" "));
                g.inputs.push_back(token);
            }
            circuit.gates[name]=g;
        }
    }
    return circuit;
}