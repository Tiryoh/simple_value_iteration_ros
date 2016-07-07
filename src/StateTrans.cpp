#include "StateTrans.h"
#include <climits>
#include <cmath>
#include <fstream>
#include <sstream>
#include "State.h"
#include "picojson/picojson.h"
using namespace std;

StateTrans::StateTrans()
{
	init();
}

StateTrans::~StateTrans(){}

void StateTrans::init(void)
{
	m_delta = 0.0;
	m_states.clear();
	m_actions.clear();
}

bool StateTrans::setStateNum(const string &str)
{
	m_state_num = atoi(str.c_str());
	if(m_state_num <= 0 or m_states.size() > 0)
		return false;

	m_states.reserve(m_state_num);
	for(unsigned long i=0;i<m_state_num;i++){
		State s;

		m_states.push_back(s);
	}
	return true;
}

unsigned long StateTrans::getStateNum(void)
{
	return m_state_num;
}

bool StateTrans::setAction(const string &action)
{
	m_actions.push_back(action);
	return true;
}

/*
void StateTrans::status(void)
{
	cerr << "statenum: " << m_state_num << endl;
	cerr << "actions: ";
	for(auto i=m_actions.begin();i<m_actions.end();i++){
		cerr << *i << " ";
	}
	cerr << endl;
}
*/

State* StateTrans::getState(unsigned long index)
{
	return &m_states[index];
}

unsigned int StateTrans::getActionIndex(string &line)
{
	for(unsigned int i=0;i<m_actions.size();i++){
		if(m_actions[i] == line)
			return i;
	}
	return -1;
}

bool StateTrans::setStateTrans(unsigned long s,int a,unsigned long s_to,double prob,unsigned long cost)
{
	return m_states[s].setStateTrans(a,s_to, (unsigned int)(prob*65536),
				cost,m_actions.size());
}

bool StateTrans::valueIteration(unsigned long start_num)
{
	double delta = 0.0;
	for(unsigned long i=start_num;i<m_state_num+start_num;i++){
		unsigned long index = (i + m_state_num)%m_state_num;
		unsigned long v = m_states[index].valueIteration(m_states);
		unsigned long prv = m_states[index].getValue();

		//cerr << index << ' ' << v << ' ' << prv << endl;
		double d = fabs((double)v - (double)prv);
		if(delta < d)
			delta = d;

		m_states[index].setValue(v);
		//cerr << "***" <<  m_states[index].getValue() << endl;

	}
	m_delta = delta;
	return true;
}

bool StateTrans::setValue(unsigned long s,unsigned long v)
{
	m_states[s].setValue(v);
	return true;
}

bool StateTrans::printValues(string filename)
{
	ofstream ofs(filename);
	if(!ofs)
		return false;

	for(unsigned long i=0;i<m_state_num;i++){
		ofs << i << " " << m_states[i].getValue() << endl;
	}
	ofs.close();
	return true;
}

bool StateTrans::printActions(string filename)
{
	ofstream ofs(filename);
	if(!ofs)
		return false;

	for(unsigned long i=0;i<m_state_num;i++){
		int a = m_states[i].getActionIndex();
		if(a >= 0)
			ofs << i << " " << m_actions[a] << endl;
		else
			ofs << i << " " << "null" << endl;
	}
	ofs.close();
	return true;
}

bool StateTrans::readStateTransFile(const char *filename)
{
	ifstream ifs_state_trans(filename);
	string buf;

	if(!ifs_state_trans)
		return false;

	//parse of header in state transition file
	while(ifs_state_trans && getline(ifs_state_trans,buf)){
		if(parseHeader(buf) == false)
			break;
	}

	//checking global setting
	//g_state_trans.status();

	//parse of state transtions
	while(ifs_state_trans && getline(ifs_state_trans,buf)){
		if(parseStateTrans(buf) == false)
			break;
	}

	//parse of final states
	while(ifs_state_trans && getline(ifs_state_trans,buf)){
		if(parseFinalStates(buf) == false)
			break;
	}

	ifs_state_trans.close();

	return true;
}

bool StateTrans::readStateTransJsonFile(const char *filename){
	ifstream ifs_state_trans(filename);
	picojson::value v;

	ifs_state_trans >> v;
	string err = picojson::get_last_error();
	if(!err.empty()){
		cerr << err << endl;
		return false;
	}

	picojson::object& obj = v.get<picojson::object>();
	//metadata
	picojson::object& metadata_obj = obj["metadata"].get<picojson::object>();
	setStateNum( metadata_obj["statenum"].get<string>().c_str() );
	picojson::array& actions = metadata_obj["actions"].get<picojson::array>();
	for (picojson::array::iterator it = actions.begin(); it != actions.end(); it++) {
		setAction( (*it).get<string>() );
	}
	//state
	static int state_index = -1;
	static int action_index = -1;
	picojson::array& state_array = obj["state_transitions"].get<picojson::array>();
	for (picojson::array::iterator it2 = state_array.begin(); it2 != state_array.end(); it2++) {
		picojson::object& o = it2->get<picojson::object>();
		picojson::object& e = o["prediction"].get<picojson::object>();
		state_index = atoi( o["state"].get<string>().c_str() );
		action_index = getActionIndex( o["action"].get<string>() );
		if(state_index < 0){
			cerr << "Invalid State No." << endl;
			return false;
		}
		int s_after = atoi( e["state"].get<string>().c_str() );
		double p = atof( e["prob."].get<string>().c_str() );
		double c = atof( e["cost"].get<string>().c_str() );
		if(s_after < 0){
			cerr << "Invalid Posterior State" << endl;
			return false;
		}
		if(p <= 0.0 || p > 1.0){
			cerr << "Invalid Probability" << endl;
			return false;
		}
		setStateTrans(state_index,action_index,s_after,p,c);
	}
	//final
	picojson::object& final_states = obj["final states"].get<picojson::object>();
	static int final_state_index = -1;
	static int value = 0;
	final_state_index = atoi( final_states["state"].get<string>().c_str() );
	value = atoi( final_states["value"].get<string>().c_str() );
	setValue(final_state_index,value);
	if(final_state_index < 0){
		cerr << "Invalid State No." << endl;
		return false;
	}

	ifs_state_trans.close();
	return true;
}

bool StateTrans::parseHeader(string &line){
	if(line == "%%state transitions%%")
		return false;
	
	vector<string> words;
	tokenizer(line,words);
	if(words.size() < 1)
		return true;

	if(words[0] == "statenum"){
		if(! setStateNum(words[1])){
			cerr << "Invalid State Number" << endl;
			return false;
		}
	}
	else if(words[0] == "actions"){
		for(auto i=++words.begin();i<words.end();i++){
			setAction(*i);
		}	
	}

	return true;
}

bool StateTrans::parseStateTrans(string &line)
{
	vector<string> words;
	tokenizer(line,words);
	if(words.size() < 1)
		return true;

	if(words[0].at(0) == '%')
		return false;

	static int state_index = -1;
	static int action_index = -1;
	if(words[0] == "state"){
		state_index = atoi(words[1].c_str());
		action_index = getActionIndex(words[3]);

		if(state_index < 0){
			cerr << "Invalid State No." << endl;
			return false;
		}
	}
	else if(words[0][0] == '\t'){
		int s_after = atoi(words[1].c_str());
		double p = atof(words[3].c_str());
		double c = atof(words[5].c_str());

		if(s_after < 0){
			cerr << "Invalid Posterior State" << endl;
			return false;
		}
		if(p <= 0.0 || p > 1.0){
			cerr << "Invalid Probability" << endl;
			return false;
		}

		setStateTrans(state_index,action_index,s_after,p,c);
	}

	return true;
}

bool StateTrans::parseFinalStates(string &line)
{
	vector<string> words;
	tokenizer(line,words);
	if(words.size() < 1)
		return true;

	if(words[0].at(0) == '%')
		return false;

	static int state_index = -1;
	static int value = 0;
	if(words[0] == "state"){
		state_index = atoi(words[1].c_str());
		value = atoi(words[3].c_str());
		setValue(state_index,value);

		if(state_index < 0){
			cerr << "Invalid State No." << endl;
			return false;
		}
	}else{
		cerr << "Invalid statement" << endl;
		return false;
	}
	
	return true;
}

bool StateTrans::tokenizer(string &line,vector<string> &words){
	string token;
	stringstream ss(line);
	while(getline(ss,token,' ')){
		words.push_back(token);
	}
	return true;
}
