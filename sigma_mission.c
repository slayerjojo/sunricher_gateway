#include "sigma_mission.h"
#include "sigma_log.h"
#include "interface_os.h"

static SigmaMission *_missions = 0;

void sigma_mission_init(void)
{
    _missions = 0;
}

void sigma_mission_update(void)
{
    SigmaMission *m = _missions, *prev = 0;
    while (m)
    {
        if (m->release)
        {
            if (m->deps)
            {
                if (prev)
                    prev->next = m->deps;
                else
                    _missions = m->deps;
                m->deps->next = m->next;
            }
            else
            {
                if (prev)
                    prev->next = m->next;
                else
                    _missions = m->next;
            }
            os_free(m);
            if (prev)
                m = prev->next;
            else
                m = _missions;
            continue;
        }
        if (m->handler)
        {
            int ret = m->handler(m, 0);
            if (ret)
            {
                m->handler(m, 1);
                m->release = 1;
            }
        }
        prev = m;
        m = m->next;
    }
}

SigmaMission *sigma_mission_create(SigmaMission *mission, uint8_t type, SigmaMissionHandler handler, size_t extends)
{
    SigmaMission *m = (SigmaMission *)os_malloc(sizeof(SigmaMission) + extends);
    if (!m)
    {
        SigmaLogError(0, 0, "out of memory");
        return 0;
    }
    os_memset(m, 0, sizeof(SigmaMission) + extends);
    m->type = type;
    m->handler = handler;

    if (mission)
    {
        while (mission && mission->deps)
            mission = mission->deps;
        mission->deps = m;
    }
    else
    {
        m->next = _missions;
        _missions = m;
    }

    return m;
}

void sigma_mission_release(SigmaMission *mission)
{
    mission->release = 1;
    if (mission->handler)
        mission->handler(mission, 1);
}

void *sigma_mission_extends(SigmaMission *mission)
{
    return mission->extends;
}

SigmaMission *sigma_mission_iterator(SigmaMissionIterator *it)
{
    if (!it)
        return 0;

    if (!it->root)
    {
        it->root = _missions;
        if (it->root)
            it->depends = it->root->deps;
    }
    else if (it->depends)
    {
        it->depends = it->depends->deps;
    }
    else
    {
        it->root = it->root->next;
        if (it->root)
            it->depends = it->root->deps;
    }

    if (!it->root)
        return 0;

    while ((it->root && it->root->release) || (it->depends && it->depends->release))
    {
        if (it->depends)
        {
            it->depends = it->depends->deps;
        }
        else
        {
            it->root = it->root->next;
            if (it->root)
                it->depends = it->root->deps;
        }

        if (!it->root)
            return 0;
    }
    if (it->depends)
        return it->depends;
    return it->root;
}
