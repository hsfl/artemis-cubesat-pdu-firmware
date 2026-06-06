#ifndef _PDU_PACKET_H /* Guard against multiple inclusion */
#define _PDU_PACKET_H

#include <stdbool.h>
#include <stdint.h>

void pdu_protocol_process_byte(uint8_t byte);
void pdu_protocol_service_timers(void);
void pdu_protocol_reset_parser(void);
void pdu_request_software_reset(void);
bool pdu_software_reset_requested(void);
void disableAllGPIOs(void);

#endif /* _PDU_PACKET_H */

/* *****************************************************************************
 End of File
 */
