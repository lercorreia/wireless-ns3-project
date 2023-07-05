#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/packet-metadata.h"
#include "ns3/ocb-wifi-mac.h"
#include "ns3/wifi-80211p-helper.h"
#include "ns3/wave-mac-helper.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/socket.h"
#include <iostream>
#include <fstream> 
#include <thread>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("WirelessEnvironment");



Ptr<Socket> alertaTanqueA;
Ptr<Socket> alertaTanqueB;

Ptr<Socket> sendIntermediateToAlertaA;
Ptr<Socket> sendIntermediateToAlertaB;
Ptr<Socket> sendIntermediateToServer;


std::set<uint32_t> sent;


std::string packetSender(std::string content) {
  std::string sensor;
  if (content.find("Ph Tanque A") != -1) sensor = "Ph Tanque A";
  else if (content.find("Temperatura Tanque A") != -1) sensor = "Temperatura Tanque A";
  else if (content.find("Oxigenio Tanque A") != -1) sensor = "Oxigenio Tanque A";
  else if (content.find("Ligacao Tanque A") != -1) sensor = "Ligacao Tanque A";
  else if (content.find("Ph Tanque B") != -1) sensor = "Ph Tanque B";
  else if (content.find("Temperatura Tanque B") != -1) sensor = "Temperatura Tanque B";
  else if (content.find("Oxigenio Tanque B") != -1) sensor = "Oxigenio Tanque B";
  else if (content.find("Ligacao Tanque B") != -1) sensor = "Ligacao Tanque B";

  return sensor;
}




void RxPacoteIntermediario (Ptr<Socket> socket) {

  Ptr<Packet> pkt = socket->Recv ();
  Ptr<Packet> packet;
  std::ostringstream msg;
  std::string content;
  uint8_t *buffer = new uint8_t[pkt->GetSize ()];
  pkt->CopyData(buffer, pkt->GetSize ());
  std::string s = std::string((char*)buffer);
  std::string sensor = packetSender(s);

  uint32_t pckID = pkt->GetUid();
  if (sent.find(pckID) != sent.end()) return;
  sent.insert(pckID);

  if (s.find("ALERTA") != std::string::npos){

    if (s.find("Tanque A") != std::string::npos){
        std::cout << "Pacote recebido no nó intermédiario, enviando para atuador...\n" << std::endl;
        content =" totanqueA";
        msg << sensor + content << '\0';
        packet = Create<Packet>((uint8_t*) msg.str().c_str(), msg.str().length());
        sendIntermediateToAlertaA->Send(packet);
    }else if (s.find("Tanque B") != std::string::npos){
        std::cout << "Pacote recebido no nó intermédiario, enviando para atuador...\n" << std::endl;
        content = " totanqueB";
        msg << sensor + content << '\0';
        packet = Create<Packet>((uint8_t*) msg.str().c_str(), msg.str().length());
        sendIntermediateToAlertaB->Send(packet);
    }
  }else if(s.find("toserver")!= std::string::npos){
    std::cout << "Pacote recebido no nó intermédiario, enviando para servidor...\n" << std::endl;
    sendIntermediateToServer->Send (pkt);
  }

  
  
}


void AlertaTanqueA(Ptr<Socket> socket){
     Ptr<Packet> pkt = socket->Recv ();
    uint8_t *buffer = new uint8_t[pkt->GetSize ()];
    pkt->CopyData(buffer, pkt->GetSize ());
    std::string s = std::string((char*)buffer);
    std::string sensor = packetSender(s);
    if (!(s.find("totanqueA") != -1)) return;
    std::cout << "Sinal de " << sensor << " recebido no nó de Alerta tanque A...\n" << std::endl;
     uint32_t pckID = pkt->GetUid();
    if (sent.find(pckID) != sent.end()) return;
    sent.insert(pckID);


}


void AlertaTanqueB(Ptr<Socket> socket){
    Ptr<Packet> pkt = socket->Recv ();
    uint8_t *buffer = new uint8_t[pkt->GetSize ()];
    pkt->CopyData(buffer, pkt->GetSize ());
    std::string s = std::string((char*)buffer);
    std::string sensor = packetSender(s);
    if (!(s.find("totanqueB") != -1)) return;
    std::cout << "Sinal de " << sensor << " recebido no nó de Alerta tanque B...\n" << std::endl;
    uint32_t pckID = pkt->GetUid();
    if (sent.find(pckID) != sent.end()) return;
    sent.insert(pckID);

    
}


void RxPacoteServidor (Ptr<Socket> socket) {
  Ptr<Packet> pkt = socket->Recv ();
  uint8_t *buffer = new uint8_t[pkt->GetSize ()];
  pkt->CopyData(buffer, pkt->GetSize ());
  std::string s = std::string((char*)buffer);
  std::string sensor = packetSender(s);
  std::cout << "Pacote de " << sensor << " recebido no nó servidor" << std::endl;

  TypeId tipoId = TypeId::LookupByName ("ns3::UdpSocketFactory");
  Ptr<Socket> serverToIntermediario = Socket::CreateSocket (socket->GetNode (), tipoId);
  serverToIntermediario->SetAllowBroadcast(true);
  serverToIntermediario->Connect (InetSocketAddress (Ipv4Address ("255.255.255.255"), 80));
  
  

  if (s.find('0') != -1){
    std::string content = sensor + ' ' + "ALERTA";
    Ptr<Packet> packet = Create<Packet> ((uint8_t*) content.c_str(), content.length());
    std::cout << sensor << " Fora dos parâmetros\nEnviando sinal para um nó atuador\n\n" << std::endl;
    serverToIntermediario->Send(packet);
    
  }else{

    std::cout << sensor << " está OK\n\n" << std::endl;
   }
}



static void enviarPacote (Ptr<Socket> socket, uint32_t pktSize, uint32_t pktCount, Time pktInterval, Ptr<Packet> pkt) {
  if (pktCount > 0) {
    uint8_t *buffer = new uint8_t[pkt->GetSize ()];
    pkt->CopyData(buffer, pkt->GetSize ());
    std::string s = std::string((char*)buffer);
    std::string sensor = packetSender(s);
    std::cout << "Enviando informações  de " << sensor << std::endl;
    socket->Send (pkt);
    Simulator::Schedule (pktInterval, &enviarPacote, socket, pktSize,pktCount - 1, pktInterval, pkt);
  } else {
    socket->Close();
  }
}




int main(int argc, char* argv[]) {
    
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
    LogComponentEnable("WirelessEnvironment", LOG_LEVEL_INFO);

    // Configuração de parâmetros
    uint32_t numNodes = 11;  // Número total de nós na rede
    uint32_t numSensors = 8;  // Número de nós medidores (temperatura, pH e oxigênio)
    uint32_t numBroadcastNodes = 1; 
    uint32_t numServers = 1;  // Número de nós servidores
    uint32_t numIntermediateNodes = 1;  // Número de nós intermediários

    std::ifstream file("scratch/teste1.txt");
    if (!file.is_open()) {
    std::cout << "Erro ao abrir o arquivo!" << std::endl;
    return 1; // Saia do programa se o arquivo não puder ser aberto
}
    char myText;

    uint32_t packetSize = 1000; // bytes
    uint32_t numPackets = 1;
    double interval = 1000;

    ns3::PacketMetadata::Enable();
    Time interPacketInterval = Seconds (interval);



    // Criação de nós

    Ptr<Node> serverNode = CreateObject<Node>();
    Ptr<Node> sensorPhNodeTA= CreateObject<Node>();
    Ptr<Node> sensorOxigenioNodeTA= CreateObject<Node>();
    Ptr<Node> sensorTemperaturaNodeTA= CreateObject<Node>();
    Ptr<Node> alertaNodeTA= CreateObject<Node>();
    Ptr<Node> sensorPhNodeTB= CreateObject<Node>();
    Ptr<Node> sensorOxigenioNodeTB= CreateObject<Node>();
    Ptr<Node> sensorTemperaturaNodeTB= CreateObject<Node>();
    Ptr<Node> alertaNodeTB= CreateObject<Node>();

    Ptr<Node> intermediarioNodeToServer= CreateObject<Node>();
    Ptr<Node> intermediarioNodeToAlertaA= CreateObject<Node>();
    Ptr<Node> intermediarioNodeToAlertaB= CreateObject<Node>();

    NodeContainer allNodes(serverNode, sensorPhNodeTA, sensorOxigenioNodeTA, sensorTemperaturaNodeTA, alertaNodeTA,sensorPhNodeTB, sensorOxigenioNodeTB, sensorTemperaturaNodeTB, alertaNodeTB, intermediarioNodeToServer, intermediarioNodeToAlertaA, intermediarioNodeToAlertaB);



    // Configuração do canal sem fio
    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211n);

    WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac");

    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(100));

    YansWifiPhyHelper wifiPhy;
    wifiPhy.SetChannel(wifiChannel.Create());
    

    
    NetDeviceContainer devices = wifi.Install(wifiPhy, wifiMac, allNodes);


    // Configuração da mobilidade dos nós
    MobilityHelper mobility;
    mobility.SetPositionAllocator("ns3::RandomDiscPositionAllocator",
                                  "X", StringValue("100.0"),
                                  "Y", StringValue("100.0"),
                                  "Rho", StringValue("ns3::UniformRandomVariable[Min=0|Max=30]"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(allNodes);

    // Configuração da pilha de protocolos da internet nos nós
    InternetStackHelper stack;
    stack.Install(allNodes);

    // Atribuição de endereços IP
    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");

    Ipv4InterfaceContainer interface;
    interface = address.Assign(devices);

    
    /////////////////////////////////////////////////////////////////////////

    // configurando o recebimento do pacote no nó servidor
    TypeId tipoId = TypeId::LookupByName ("ns3::UdpSocketFactory");

    Ptr<Socket> sServidor = Socket::CreateSocket (serverNode, tipoId);
    InetSocketAddress localServidor = InetSocketAddress (Ipv4Address ("10.1.1.0"), 80);
    sServidor->Bind(localServidor);
    sServidor->SetRecvCallback (MakeCallback (&RxPacoteServidor));

    



    InetSocketAddress remoteServer = InetSocketAddress (Ipv4Address ("10.1.1.1"), 80);

    InetSocketAddress remoteIntermediario = InetSocketAddress (Ipv4Address ("255.255.255.255"), 80);
    // configurando o recebimento do pacote nos nós intermediários///
    
    TypeId tipoalerta = TypeId::LookupByName ("ns3::UdpSocketFactory");
    //intermediário servidor
    sendIntermediateToServer = Socket::CreateSocket (intermediarioNodeToServer, tipoId);
    InetSocketAddress local = InetSocketAddress (Ipv4Address::GetAny (), 80);
    sendIntermediateToServer->Bind(local);
    sendIntermediateToServer->SetAllowBroadcast (true);
    sendIntermediateToServer->Connect(remoteServer);
    sendIntermediateToServer->SetRecvCallback (MakeCallback (&RxPacoteIntermediario));
    
    
    //intermediario alerta tanque A
    sendIntermediateToAlertaA = Socket::CreateSocket (intermediarioNodeToAlertaA, tipoId);
    InetSocketAddress localA = InetSocketAddress (Ipv4Address::GetAny (), 80);
    sendIntermediateToAlertaA->Bind(localA);
    sendIntermediateToAlertaA->SetAllowBroadcast (true);
    sendIntermediateToAlertaA->Connect(remoteIntermediario);
    sendIntermediateToAlertaA->SetRecvCallback (MakeCallback (&RxPacoteIntermediario));

    //intermediario alerta tanque B
    sendIntermediateToAlertaB = Socket::CreateSocket (intermediarioNodeToAlertaB, tipoId);
    InetSocketAddress localB = InetSocketAddress (Ipv4Address::GetAny (), 80);
    sendIntermediateToAlertaB->Bind(localB);
    sendIntermediateToAlertaB->SetAllowBroadcast (true);
    sendIntermediateToAlertaB->Connect(remoteIntermediario);
    sendIntermediateToAlertaB->SetRecvCallback (MakeCallback (&RxPacoteIntermediario));


    


// configurando atuador de Ligação do tanque 1
    alertaTanqueA = Socket::CreateSocket (alertaNodeTA, tipoId);
    InetSocketAddress localAtuadorTA = InetSocketAddress (Ipv4Address::GetAny (), 80);
    alertaTanqueA->Bind(localAtuadorTA);
    alertaTanqueA->SetRecvCallback (MakeCallback (&AlertaTanqueA));


    // configurando atuador de Ligação do tanque 2
    alertaTanqueB = Socket::CreateSocket (alertaNodeTB, tipoId);
    InetSocketAddress localAtuadorTB = InetSocketAddress (Ipv4Address::GetAny (), 80);
    alertaTanqueB->Bind(localAtuadorTB);
    alertaTanqueB->SetRecvCallback (MakeCallback (&AlertaTanqueB));







    // confugurando sensor de Ph do tanque 1
    Ptr<Socket> sensorPhTA = Socket::CreateSocket (sensorPhNodeTA, tipoId);
    sensorPhTA->SetAllowBroadcast (true);
    sensorPhTA->Connect (remoteIntermediario);

    // configurando sensor de temperatura do tanque 1
    Ptr<Socket> sensorTemperaturaTA = Socket::CreateSocket (sensorTemperaturaNodeTA, tipoId);
    sensorTemperaturaTA->SetAllowBroadcast (true);
    sensorTemperaturaTA->Connect (remoteIntermediario);

    // configurando sensor de Oxigenio do tanque 1
    Ptr<Socket> sensorOxigenioTA = Socket::CreateSocket (sensorOxigenioNodeTA, tipoId);
    sensorOxigenioTA->SetAllowBroadcast (true);
    sensorOxigenioTA->Connect (remoteIntermediario);

    
    
/////////////////////////////////////////////////////////////////////////////////////////////

    // confugurando sensor de Ph do tanque 2
    Ptr<Socket> sensorPhTB = Socket::CreateSocket (sensorPhNodeTB, tipoId);
    sensorPhTB->SetAllowBroadcast (true);
    sensorPhTB->Connect (remoteIntermediario);

    // configurando sensor de temperatura do tanque 2
    Ptr<Socket> sensorTemperaturaTB = Socket::CreateSocket (sensorTemperaturaNodeTB, tipoId);
    sensorTemperaturaTB->SetAllowBroadcast (true);
    sensorTemperaturaTB->Connect (remoteIntermediario);

    // configurando sensor de Oxigenio do tanque 2
    Ptr<Socket> sensorOxigenioTB = Socket::CreateSocket (sensorOxigenioNodeTB, tipoId);
    sensorOxigenioTB->SetAllowBroadcast (true);
    sensorOxigenioTB->Connect (remoteIntermediario);

    
    
    int sensor = 0;
    int timer = 0;
    while (file.get(myText)) {
        
        
        std::ostringstream msg;
        std::string content;
        Ptr<Packet> packet;
        
        if(sensor==0)
            {
                content = "Ph Tanque A ";
                msg << content + myText + " toserver"<< '\0';
                packet = Create<Packet>((uint8_t*) msg.str().c_str(), msg.str().length());
                Simulator::Schedule(Seconds(timer*1.5), &enviarPacote, sensorPhTA, packetSize, numPackets, interPacketInterval, packet);
                
            }
            
            else if(sensor==1)
                {content = "Temperatura Tanque A ";
                msg << content + myText + " toserver"<< '\0';
                packet = Create<Packet>((uint8_t*) msg.str().c_str(), msg.str().length());
                Simulator::Schedule(Seconds(timer*1.5), &enviarPacote, sensorTemperaturaTA, packetSize, numPackets, interPacketInterval, packet);
                }

            else if(sensor==2)
                {content = "Oxigenio Tanque A  ";
                msg << content + myText + " toserver"<< '\0';
                packet = Create<Packet>((uint8_t*) msg.str().c_str(), msg.str().length());
                Simulator::Schedule(Seconds(timer*1.5), &enviarPacote, sensorOxigenioTA, packetSize, numPackets, interPacketInterval, packet);
                }

            else if(sensor==3)
          {      content = "Ph Tanque B ";
                msg << content + myText + " toserver"<< '\0';
                packet = Create<Packet>((uint8_t*) msg.str().c_str(), msg.str().length());
                Simulator::Schedule(Seconds(timer*1.5), &enviarPacote, sensorPhTB, packetSize, numPackets, interPacketInterval, packet);
                }
                
            
            else if(sensor==4)
             {   content = "Temperatura Tanque B ";
                msg << content + myText + " toserver"<< '\0';
                packet = Create<Packet>((uint8_t*) msg.str().c_str(), msg.str().length());
                Simulator::Schedule(Seconds(timer*1.5), &enviarPacote, sensorTemperaturaTB, packetSize, numPackets, interPacketInterval, packet);
                }

            else if(sensor==5)
                {content = "Oxigenio Tanque B ";
                msg << content + myText + " toserver"<< '\0';
                packet = Create<Packet>((uint8_t*) msg.str().c_str(), msg.str().length());
                Simulator::Schedule(Seconds(timer*1.5), &enviarPacote, sensorOxigenioTB, packetSize, numPackets, interPacketInterval, packet);
                break;
          }

         
        

        if (sensor < 5) sensor++;
        else sensor = 0;

        timer++;
    }


    file.close();

    Simulator::Run();
    Simulator::Destroy();



    

    return 0;
}