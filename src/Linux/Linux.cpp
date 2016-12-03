#include <iostream>
#include <cstdio>
#include <time.h>
#include <ctype.h>
#include <unistd.h>
#include <iomanip>
#include <asio.hpp>

#include "LinuxBaseLibrary.h"
#include "LinuxCOSEMServer.h"

#include "HDLCLLC.h"
#include "COSEM.h"
#include "serialwrapper/SerialWrapper.h"
#include "tcpwrapper/TCPWrapper.h"

using namespace std;
using namespace EPRI;
using namespace asio;

class AppBase
{
public:
    typedef std::function<void(const std::string&)> ReadLineFunction;

    AppBase(LinuxBaseLibrary& BL) : 
        m_Base(BL), m_Input(BL.get_io_service(), ::dup(STDIN_FILENO)), 
        m_Output(BL.get_io_service(), ::dup(STDOUT_FILENO))
    {
    }
    
    virtual void Run()
    {
        m_Base.Process();
    }
    
    virtual void PrintLine(const std::string& Line)
    {
        asio::write(m_Output, asio::buffer(Line));
    }
    
    virtual void ReadLine(ReadLineFunction Handler)
    {
        asio::async_read_until(m_Input,
            m_InputBuffer,
            '\n',
            std::bind(&AppBase::ReadLine_Handler,
                      this,
                      std::placeholders::_1,
                      std::placeholders::_2,
                      Handler));
        
    }
    
    virtual std::string GetLine()
    {
        asio::read_until(m_Input, m_InputBuffer, '\n');
        return ConsumeStream();
    }
    
protected:
    void ReadLine_Handler(const asio::error_code& Error, size_t BytesTransferred, ReadLineFunction Handler)
    {
        if (!Error)
        {
            Handler(ConsumeStream());
        }
    }
    
    std::string ConsumeStream()
    {
        std::istream Stream(&m_InputBuffer);
        std::string  RetVal;
        std::getline(Stream, RetVal);
        return RetVal;
    }

    int GetNumericInput(const std::string& PromptText, int Default)
    {
        std::string RetVal;
        do
        {
            PrintLine(PromptText + ": ");
            RetVal = GetLine();
            try
            {
                if (RetVal.length())
                    return std::stoi(RetVal, nullptr, 0);	
                else 
                    return Default;
            }
            catch (const std::invalid_argument&)
            {
                PrintLine("Input must be numeric!\n\n");
            }
            catch (const std::out_of_range&)
            {
                PrintLine("Input is too large!\n\n");
            }
        
        } while (true);
    }

    std::string GetStringInput(const std::string& PromptText, const std::string& Default)
    {
        std::string RetVal;
        PrintLine(PromptText + ": ");
        RetVal = GetLine();
        if (RetVal.empty())
            RetVal = Default;
        return RetVal;
    }
    
    LinuxBaseLibrary&           m_Base;
    posix::stream_descriptor    m_Input;
    asio::streambuf             m_InputBuffer;
    posix::stream_descriptor    m_Output;
    
};

class LinuxClientEngine : public COSEMClientEngine
{
public:
    LinuxClientEngine() = delete;
    LinuxClientEngine(const Options& Opt, Transport * pXPort)
        : COSEMClientEngine(Opt, pXPort)
    {
    }
    virtual ~LinuxClientEngine()
    {
    }
    
    virtual void OnOpenConfirmation(COSEMAddressType ServerAddress)
    {
        Base()->GetDebug()->TRACE("\n\nAssociated with Server %d...\n\n",
            ServerAddress);
    }

    virtual void OnGetConfirmation(GetToken Token,
                                   const DLMSVector& Data)
    {
        Base()->GetDebug()->TRACE("\n\nGet Confirmation for Token %d...\n\n", Token);
        
        IData     SerialNumbers;
        DLMSValue Value;
        
        SerialNumbers.value = Data;
            
        if (COSEMType::VALUE_RETRIEVED == SerialNumbers.value.GetNextValue(&Value))
        {
            Base()->GetDebug()->TRACE("%s\n", DLMSValueGet<VISIBLE_STRING_CType>(Value).c_str());
        }
        
        Base()->GetDebug()->TRACE_VECTOR("GET", Data);
    }

};

class ClientApp : public AppBase
{
public:
    ClientApp(LinuxBaseLibrary& BL)
        : AppBase(BL)
    {
        m_Base.get_io_service().post(std::bind(&ClientApp::ClientMenu, this));
    }

protected:
    void ClientMenu()
    {
        PrintLine("\nClient Menu:\n\n");
        PrintLine("\t0 - Exit Application\n");
        PrintLine("\t1 - TCP Connect\n");
        PrintLine("\t2 - COSEM Open\n");
        PrintLine("\t3 - COSEM Get\n");
        PrintLine("\t4 - COSEM Close\n\n");
        PrintLine("Select: ");
        ReadLine(std::bind(&ClientApp::ClientMenu_Handler, this, std::placeholders::_1));
    }
    
    void ClientMenu_Handler(const std::string& RetVal)
    {
        if (RetVal == "0")
        {
            exit(0);
        }
        else if (RetVal == "1")
        {
            int         SourceAddress = GetNumericInput("Client Address (Default: 1)", 1);
            std::string TCPAddress = GetStringInput("Destination TCP Address (Default: localhost)", "localhost");
    
            m_pClientEngine = new LinuxClientEngine(COSEMClientEngine::Options(SourceAddress), 
                new TCPWrapper((m_pSocket = Base()->GetCore()->GetIP()->CreateSocket(LinuxIP::Options(LinuxIP::Options::MODE_CLIENT)))));
            if (SUCCESSFUL != m_pSocket->Open(TCPAddress.c_str()))
            {
                PrintLine("Failed to initiate connect\n");
            }
        }
        else if (RetVal == "2")
        {
            if (m_pSocket->IsConnected())
            {
                int                  DestinationAddress = GetNumericInput("Server Address (Default: 1)", 1);
                COSEM::SecurityLevel Security = (COSEM::SecurityLevel)
                    GetNumericInput("Security Level [0 - None, 1 - Low, 2 - High] (Default: 0)", COSEM::SECURITY_NONE);
                std::string          Password;
                if (COSEM::SECURITY_LOW_LEVEL == Security)
                {
                    Password = GetStringInput("Password", "");
                }
                m_pClientEngine->Open(DestinationAddress, { Security, Password });
            }
            else
            {
                PrintLine("TCP Connection Not Established Yet!\n");
            }
        }
        else if (RetVal == "3")
        {
            if (m_pSocket->IsConnected() && m_pClientEngine->IsOpen())
            {
                Cosem_Attribute_Descriptor Descriptor;
                
                Descriptor.class_id = (ClassIDType) GetNumericInput("Class ID (Default: 1)", CLSID_IData);
                Descriptor.attribute_id = (ObjectAttributeIdType) GetNumericInput("Attribute (Default: 2)", 2);
                if (Descriptor.instance_id.Parse(GetStringInput("OBIS Code (Default: 0-0:96.1.0*255)", "0-0:96.1.0*255")))
                {
                    if (m_pClientEngine->Get(Descriptor,
                        &m_GetToken))
                    {
                    }
                }
                else
                {
                    PrintLine("Malformed OBIS Code!\n");
                }
            }
            else
            {
                PrintLine("Not Connected!\n");
            }
            
        }
        else if (RetVal == "4")
        {
            if (m_pClientEngine->Close())
            {
                PrintLine("COSEM Connection Closed\n");
            }
        }
        m_Base.get_io_service().post(std::bind(&ClientApp::ClientMenu, this));
    }
    
    LinuxClientEngine *         m_pClientEngine = nullptr;
    ISocket *                   m_pSocket = nullptr;
    COSEMClientEngine::GetToken m_GetToken;
   
};

class ServerApp : public AppBase
{
public:
    ServerApp(LinuxBaseLibrary& BL) : 
        AppBase(BL)
    {
        m_Base.get_io_service().post(std::bind(&ServerApp::ServerMenu, this));
    }

protected:
    void ServerMenu()
    {
        PrintLine("\nServer Menu:\n\n");
        PrintLine("\t0 - Exit Application\n");
        PrintLine("\t1 - TCP Server\n\n");
        PrintLine("Select: ");
        ReadLine(std::bind(&ServerApp::ServerMenu_Handler, this, std::placeholders::_1));
    }
    
    void ServerMenu_Handler(const std::string& RetVal)
    {
        if (RetVal == "0")
        {
            exit(0);
        }
        else if (RetVal == "1")
        {
            ISocket *   pSocket;
   
            PrintLine("\nTCP Server Mode - Listening on Port 4059\n");
            m_pServerEngine = new LinuxCOSEMServerEngine(COSEMServerEngine::Options(),
                new TCPWrapper((pSocket = Base()->GetCore()->GetIP()->CreateSocket(LinuxIP::Options()))));
            if (SUCCESSFUL != pSocket->Open())
            {
                PrintLine("Failed to initiate listen\n");
            }
        }
        m_Base.get_io_service().post(std::bind(&ServerApp::ServerMenu, this));
    }
    
    LinuxCOSEMServerEngine * m_pServerEngine = nullptr;
   
};

int main(int argc, char *argv[])
{
    int                  opt;
    bool                 Server = false;
    LinuxBaseLibrary     bl;
    
    while ((opt =:: getopt(argc, argv, "S")) != -1)
    {
        switch (opt)
        {
        case 'S':
            Server = true;
            break;
        default:
            break;
        }
    }
    
    std::cout << "EPRI DLMS/COSEM " << (Server ? "Server" : "Client") << "Test Harness\n";

    if (Server)
    {
        ServerApp App(bl);
        App.Run();
       
    }
    else
    {
        ClientApp App(bl);
        App.Run();
    }
        
#if 0
// -s 1 -d 1 -C /dev/ttyUSB0 -W -p 33333333
    LinuxBaseLibrary     bl;
    ISerial *		     pSerial = bl.GetCore()->GetSerial();
    ISocket *		     pSocket;
    int                  opt;
    bool                 StartWithIEC = false;
    bool                 Server = false;
    bool                 IsSerialWrapper = false;
    bool                 IsTCP = false;
    char *               pCOMPortName = nullptr;
    char *               pSourceAddress = nullptr;
    char *               pDestinationAddress = nullptr;
    char *               pPassword = nullptr;
    COSEM::SecurityLevel Security = COSEM::SECURITY_NONE;
    
    while ((opt =::getopt(argc, argv, "TSC:s:d:IWp:P:")) != -1)
    {
        switch (opt)
        {
        case 'T':
            IsTCP = true;
            break;
        case 'I':
            StartWithIEC = true;
            break;
        case 'W':
            IsSerialWrapper = true;
            break;
        case 'S':
            Server = true;
            break;
        case 's':
            pSourceAddress = optarg;
            break;
        case 'd':
            pDestinationAddress = optarg;
            break;
        case 'C':
            pCOMPortName = optarg;
            break;
        case 'p':
            pPassword = optarg;
            Security = COSEM::SECURITY_LOW_LEVEL;
            break;
        case 'P':
            pPassword = optarg;
            Security = COSEM::SECURITY_HIGH_LEVEL;
            break;
        default:
            std::cerr << "Internal error!\n";
            return -1;
        }
    }
    
    if (Server &&
        nullptr != pSourceAddress)
    {
        Transport *              pTransport = nullptr;
        LinuxCOSEMServerEngine * pServerEngine = nullptr;
        
        if (IsTCP)
        {
            std::cout << "TCP Server Mode\n";
            pServerEngine = new LinuxCOSEMServerEngine(COSEMServerEngine::Options(),
                                new TCPWrapper((pSocket = Base()->GetCore()->GetIP()->CreateSocket(LinuxIP::Options()))));
            if (SUCCESSFUL != pSocket->Open())
            {
                std::cout << "Failed to initiate listen\n";
                exit(-1);
            }
            
        }
        else
        {
            ISerial::Options::BaudRate  BR = LinuxSerial::Options::BAUD_9600;

            if (pSerial->SetOptions(LinuxSerial::Options(BR)) != SUCCESS ||
                pSerial->Open(pCOMPortName) != SUCCESS)
            {
                std::cerr << "Error opening port " << pCOMPortName << "\n";
                exit(-1);
            }

            if (IsSerialWrapper)
            {
                pTransport = new SerialWrapper(pSerial);
            }
            else
            {
                //
                // TODO - Update HDLCServerLLC to support multiple SAP
                //
                uint8_t SourceAddress = uint8_t(strtol(pSourceAddress, nullptr, 16));
                pTransport = new HDLCServerLLC(HDLCAddress(SourceAddress), pSerial,
                    HDLCOptions({ StartWithIEC, 3, 500 }));
                
            }            
        }

        if (pServerEngine)
        {
            while (Base()->Process()) 
            {
                if (!pServerEngine->Process())
                    break;
            }
        }

    }
    else if (nullptr != pCOMPortName &&
             nullptr != pSourceAddress &&
             nullptr != pDestinationAddress)
    {
        uint8_t             SourceAddress = uint8_t(strtol(pSourceAddress, nullptr, 16));
        uint8_t             DestinationAddress = uint8_t(strtol(pDestinationAddress, nullptr, 16));
        Transport *         pTransport = nullptr;
        COSEMClientEngine * pClientEngine = nullptr;
            
        if (IsTCP)
        {
            pClientEngine = new COSEMClientEngine(COSEMClientEngine::Options(SourceAddress), 
                new TCPWrapper((pSocket = Base()->GetCore()->GetIP()->CreateSocket(LinuxIP::Options(LinuxIP::Options::MODE_CLIENT)))));
            if (SUCCESSFUL != pSocket->Open(pCOMPortName))
            {
                std::cout << "Failed to initiate connect\n";
                exit(-1);
            }
        }
        else
        {
            ISerial::Options::BaudRate  BR = LinuxSerial::Options::BAUD_9600;

            if (pSerial->SetOptions(LinuxSerial::Options(BR)) != SUCCESS ||
                pSerial->Open(pCOMPortName) != SUCCESS)
            {
                std::cerr << "Error opening port " << pCOMPortName << "\n";
                exit(-1);
            }

            if (IsSerialWrapper)
            {
                pTransport = new SerialWrapper(pSerial);
            }
            else
            {
                pTransport = new HDLCClientLLC(HDLCAddress(SourceAddress), pSerial,
                                               HDLCOptions({ StartWithIEC, 3, 500 }));
            }
                
        }

        if (pClientEngine)
        {
            while (Base()->Process()) 
            {
                if (pSocket->IsConnected())
                {
                    if (pClientEngine->Open(DestinationAddress, { Security, pPassword }))
                    {
                        IData     SerialNumbers;
                        DLMSValue Value;
                        if (pClientEngine->Get({ 0, 0, 96, 1, 0, 255 },
                                               &SerialNumbers.value) &&
                            COSEMType::VALUE_RETRIEVED == SerialNumbers.value.GetNextValue(&Value))
                        {
                            std::cout << DLMSValueGet<VISIBLE_STRING_CType>(Value) << std::endl;
                        }
            
                        pClientEngine->Close();
                    }
                }
                
                if (!pClientEngine->Process())
                    break; 
            }

        }
    }
#endif
}