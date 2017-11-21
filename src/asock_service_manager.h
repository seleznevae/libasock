#ifndef ASOCK_SERVICE_MANAGER_H
#define ASOCK_SERVICE_MANAGER_H

#include "asock_service.h"

#define MAX_ASOCK_SERVICES 10


struct asock_service_manager
{
    int m_serv_counter;
    asock_service_t *m_services[MAX_ASOCK_SERVICES];
    char *m_service_nms[MAX_ASOCK_SERVICES];
};

typedef struct asock_service_manager asock_service_manager_t;


void asock_service_mgr_init(asock_service_manager_t *mgr);
asock_service_t *asock_service_mgr_get_service_for_nmsp(asock_service_manager_t *mgr, const char* nmsp);
void asock_service_mgr_free_resources(asock_service_manager_t *mgr);

#endif /* ASOCK_SERVICE_MANAGER_H */
