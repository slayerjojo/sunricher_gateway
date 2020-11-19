#ifndef __SR_OPCODE_H__
#define __SR_OPCODE_H__

#define PROTOCOL_VERSION "1.0"

#define OPCODE_DISCOVER_GATEWAY "DiscoverGateway"
#define OPCODE_DISCOVER_GATEWAY_RESP "DiscoverGatewayResponse"
#define OPCODE_GATEWAY_ADD_DEVICE_START "AddNewDevice"
#define OPCODE_GATEWAY_ADD_DEVICE_STOP "StopAddNewDevice"
#define OPCODE_BIND_GATEWAY "BindGateway"
#define OPCODE_BOUND_USERS_QUERY "BoundUsers"
#define OPCODE_BOUND_USERS_REPORT "BoundUsersReport"
#define OPCODE_BIND_GATEWAY_RESP "BindGatewayResponse"
#define OPCODE_BIND_GATEWAY_REPORT "BindGatewayReport"
#define OPCODE_ADD_OR_UPDATE "AddOrUpdate"
#define OPCODE_ADD_OR_UPDATE_REPORT "AddOrUpdateReport"
#define OPCODE_DELETE "Delete"
#define OPCODE_DELETE_REPORT "DeleteReport"
#define OPCODE_DEVICE_STATE_REPORT "ReportState"
#define OPCODE_DEVICE_CHANGE_REPORT "ChangeReport"
#define OPCODE_DEVICE_ADD_OR_UPDATE_REPORT "StopAddNewDeviceReport"
#define OPCODE_DISCOVER_ENDPOINTS "DiscoverEndpoints"
#define OPCODE_POWERCONTROLLER_TURNON "TurnOn"
#define OPCODE_POWERCONTROLLER_TURNOFF "TurnOff"
#define OPCODE_DISCOVER "Discover"
#define OPCODE_DISCOVER_REPORT "DiscoverReport"

#endif
