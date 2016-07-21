#include"EventSenderPS.hh"

#include<sstream>

using namespace eudaq;

namespace{
  static RegisterDerived<Processor, typename std::string, EventSenderPS, uint32_t> reg_EXAMPLEPS("EventSenderPS");
}

namespace{
  static RegisterDerived<Processor, typename std::string, EventSenderPS, typename std::string> reg_EXAMPLEPS_str("EventSenderPS");
}

EventSenderPS::EventSenderPS(uint32_t psid)
  :Processor("EventSenderPS", psid){
}

void EventSenderPS::ProcessUserEvent(EVUP ev){
  if (!m_sender) EUDAQ_THROW("DataSender is not created!");
  m_sender->SendEvent(*ev);
}

void EventSenderPS::Connect(std::string type, std::string name, std::string server){
  m_sender.reset(new DataSender(type, name));
  m_sender->Connect(server); //tcp://ipaddress:portnum
}

void EventSenderPS::ProcessUsrCmd(const std::string cmd_name, const std::string cmd_par){
  switch(cstr2hash(cmd_name.c_str())){
  case cstr2hash("CONNECT"):{
    std::stringstream ss(cmd_par);
    std::string type_str;
    std::string name_str;
    std::string server_str;
    getline(ss, type_str, ',');
    getline(ss, name_str, ',');
    getline(ss, server_str, ',');
    Connect(type_str, name_str, server_str);
    break;
  }
  default:
    Processor::ProcessUsrCmd(cmd_name, cmd_par);
  }
}
