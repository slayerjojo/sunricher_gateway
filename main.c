#include "env.h"
#include "sr_layer_link.h"
#include "sr_sublayer_link_mqtt.h"
#include "sr_layer_device.h"
#include "interface_os.h"
#include "sigma_console.h"
#include "sigma_event.h"
#include "sigma_mission.h"

static void handle_console(const char *command, const char *parameters[], int count)
{
    if (!os_strcmp(command, "key_press"))
    {
        sigma_event_dispatch(EVENT_TYPE_GATEWAY_BIND, 0, 0);
    }
    else if (!os_strcmp(command, "mqtt_pub"))
    {
        sslm_send(parameters[0], 0, parameters[1], os_strlen(parameters[1]));
    }
}

int main(int argc, char *argv[])
{
    sigma_console_init(8000, handle_console);

    sigma_mission_init();
    sll_init();
    sld_init();

    while (1)
    {
        sigma_console_update();
        sigma_mission_update();

        sll_update();
        sld_update();
    }
    return 0;
}
