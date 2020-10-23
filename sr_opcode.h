#ifndef __SR_OPCODE_H__
#define __SR_OPCODE_H__

#define PROTOCOL_VERSION "1.0"

#define OPCODE_DISCOVER_GATEWAY "DiscoverGateway"
#define OPCODE_GATEWAY_ADD_DEVICE_START "AddNewDevice"
#define OPCODE_GATEWAY_ADD_DEVICE_STOP "StopAddNewDevice"
#define OPCODE_BIND_GATEWAY "BindGateway"
#define OPCODE_BIND_GATEWAY_RESP "BindGatewayResponse"
#define OPCODE_BIND_GATEWAY_REPORT "BindGatewayReport"
#define OPCODE_DEVICE_ADD_OR_UPDATE "AddOrUpdate"
#define OPCODE_DEVICE_DELETE "Delete"
#define OPCODE_DEVICE_DELETE_REPORT "DeleteReport"
#define OPCODE_DEVICE_STATE_REPORT "ReportState"
#define OPCODE_DEVICE_CHANGE_REPORT "ChangeReport"

#endif
