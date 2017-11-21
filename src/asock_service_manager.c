#include "asock_service_manager.h"
#include <assert.h>
#include <string.h>

void asock_service_mgr_init(asock_service_manager_t *mgr)
{
    assert(mgr);
    mgr->m_serv_counter = 0;
    for (int i = 0; i < MAX_ASOCK_SERVICES; ++i) {
        mgr->m_services[i] = NULL;
        mgr->m_service_nms[i] = NULL;
    }
}

asock_service_t * asock_service_mgr_get_service_for_nmsp(asock_service_manager_t *mgr, const char *nmsp)
{
    assert(mgr);
    assert(nmsp);

    /* check for available service in the specified namespace */
    for (int i = 0; i < MAX_ASOCK_SERVICES; ++i) {
        if (mgr->m_service_nms[i] && strcmp(mgr->m_service_nms[i], nmsp) == 0) {
            return mgr->m_services[i];
        }
    }

    /* create new service in the desired namespace */
    for (int i = 0; i < MAX_ASOCK_SERVICES; ++i) {
        if (mgr->m_services[i] == NULL) {
            mgr->m_service_nms[i] = strdup(nmsp);
            if (mgr->m_service_nms[i] == NULL) {
                return NULL;
            }

//            asock_service_t *service = asock_service_create();
//            if (service == NULL) {
//                free(mgr->m_service_nms[i]);
//                mgr->m_service_nms[i] = NULL;
//                return NULL;
//            }
//            if (asock_service_setns(service, mgr->m_service_nms[i]) != ASOCK_SUCCESS) {
//                asock_service_destroy(service);
//                free(mgr->m_service_nms[i]);
//                mgr->m_service_nms[i] = NULL;
//                return NULL;
//            }

            asock_service_t *service = asock_service_create_ns(mgr->m_service_nms[i]);
            if (service == NULL) {
                free(mgr->m_service_nms[i]);
                mgr->m_service_nms[i] = NULL;
                return NULL;
            }
            if (asock_service_start(service) != ASOCK_SUCCESS) {
                asock_service_destroy(service);
                free(mgr->m_service_nms[i]);
                mgr->m_service_nms[i] = NULL;
                return NULL;
            }

            mgr->m_services[i] = service;
            mgr->m_serv_counter++;
            return service;
        }
    }
    return NULL;
}

void asock_service_mgr_free_resources(asock_service_manager_t *mgr)
{
    assert(mgr);
    for (int i = 0; i < MAX_ASOCK_SERVICES; ++i) {
        if (mgr->m_services[i] != NULL) {
            free(mgr->m_service_nms[i]);
            asock_service_destroy(mgr->m_services[i]);
        }
    }
}
