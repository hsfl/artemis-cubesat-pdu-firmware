#ifndef _PDU_PACKET_H /* Guard against multiple inclusion */
#define _PDU_PACKET_H

#include <stdint.h>

void pdu_protocol_process_byte(uint8_t byte);
void disableAllGPIOs(void);

#endif /* _PDU_PACKET_H */

/* *****************************************************************************
 End of File
 */
