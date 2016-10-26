#include "APDU/APDUConstants.h"

namespace EPRI
{
    const ASNType APDUConstants::protocol_version_default = ASNBitString(1, 0x00);
    
    ASN_BEGIN_SCHEMA(APDUConstants::protocol_version_Schema)
        ASN_BIT_STRING_TYPE(ASN::IMPLICIT, 1)
    ASN_END_SCHEMA
    ASN_BEGIN_SCHEMA(APDUConstants::acse_requirements_Schema)
        ASN_BIT_STRING_TYPE(ASN::NO_OPTIONS, 1)
    ASN_END_SCHEMA
    ASN_BEGIN_SCHEMA(APDUConstants::authentication_value_Schema)
        ASN_BEGIN_CHOICE
            ASN_GraphicString_TYPE(ASN::IMPLICIT)
        ASN_END_CHOICE
    ASN_END_SCHEMA

}