#include "LinuxCOSEMServer.h"
#include "COSEMAddress.h"

namespace EPRI
{
    //
    // Data
    //
    LinuxData::LinuxData()
        : IDataObject({ 0, 0, 96, 1, {0, 9}, 255 })
    {
        for (int Index = 0; Index < 10; ++Index)
        {
            m_Values[Index] = "LINUXDATA" + std::to_string(Index);
        }
    }

    bool LinuxData::InternalGet(ICOSEMAttribute * pAttribute, 
        const Cosem_Attribute_Descriptor& Descriptor, 
        SelectiveAccess * pSelectiveAccess)
    {
        pAttribute->SelectChoice(COSEMDataType::VISIBLE_STRING);
        pAttribute->Append(m_Values[Descriptor.instance_id.GetValueGroup(EPRI::COSEMObjectInstanceID::VALUE_GROUP_E)]);
        return true;
    }
    
    bool LinuxData::InternalSet(ICOSEMAttribute * pAttribute, 
        const Cosem_Attribute_Descriptor& Descriptor, 
        const DLMSVector& Data,
        SelectiveAccess * pSelectiveAccess)
    {
        try
        {
            DLMSValue Value;
            if (ICOSEMObject::InternalSet(pAttribute, Descriptor, Data, pSelectiveAccess) &&
                pAttribute->GetNextValue(&Value) == COSEMType::GetNextResult::VALUE_RETRIEVED)
            {
                m_Values[Descriptor.instance_id.GetValueGroup(EPRI::COSEMObjectInstanceID::VALUE_GROUP_E)] =
                    DLMSValueGet<std::string>(Value);
                return true;
            }
        }
        catch (...) 
        {
        }
        return false;
    }
    //
    // Clock
    //
    LinuxClock::LinuxClock() :
        IClockObject({ 0, 0, 1, 0, 0, 255 })
    {
    }

    bool LinuxClock::InternalGet(ICOSEMAttribute * pAttribute, 
        const Cosem_Attribute_Descriptor& Descriptor, 
        SelectiveAccess * pSelectiveAccess)
    {
        switch (pAttribute->AttributeID)
        {
        case ATTR_TIME:
        case ATTR_TIME_ZONE:
        case ATTR_STATUS:
        case ATTR_DST_BEGIN:
        case ATTR_DST_END:
        case ATTR_DST_DEVIATION:
        case ATTR_DST_ENABLED:
        case ATTR_CLOCK_BASE:
        default:
            break;
        }
        return false;
    }
    
    //
    // Logical Device
    //
    LinuxManagementDevice::LinuxManagementDevice() :
        COSEMServer(ReservedAddresses::MANAGEMENT)
    {
        LOGICAL_DEVICE_BEGIN_OBJECTS
            LOGICAL_DEVICE_OBJECT(m_Clock)
            LOGICAL_DEVICE_OBJECT(m_Data)
        LOGICAL_DEVICE_END_OBJECTS
    }
    
    LinuxManagementDevice::~LinuxManagementDevice()
    {
    }
    //
    // COSEM Device
    //
    LinuxCOSEMDevice::LinuxCOSEMDevice()
    {
        SERVER_BEGIN_LOGICAL_DEVICES
            SERVER_LOGICAL_DEVICE(m_Management)
        SERVER_END_LOGICAL_DEVICES
    }

    LinuxCOSEMDevice::~LinuxCOSEMDevice()
    {
    }
    //
    // COSEM Engine
    //
    LinuxCOSEMServerEngine::LinuxCOSEMServerEngine(const Options& Opt, Transport * pXPort) :
        COSEMServerEngine(Opt, pXPort)
    {
        ENGINE_BEGIN_DEVICES
            ENGINE_DEVICE(m_Device)
        ENGINE_END_DEVICES    
    }
    
    LinuxCOSEMServerEngine::~LinuxCOSEMServerEngine()
    {
    }
    
}