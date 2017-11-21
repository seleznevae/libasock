#include "asock_impl.h"
#include "asock_logger.h"
#include "asock_service_manager.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define ASOCK_MES_BUFFER_SIZE 4096


static int s_servicies_created = 0;
pthread_mutex_t s_services_created_mutex = PTHREAD_MUTEX_INITIALIZER;

asock_service_manager_t s_service_manager;


AsockReturnCode asock_init()
{
    asock_service_mgr_init(&s_service_manager);
    return ASOCK_SUCCESS;
}

void asock_deinit()
{
    asock_service_mgr_free_resources(&s_service_manager);
}

asock_t * asock_create(int domain, int type, int protocol)
{

    return asock_create_ns(domain, type, protocol, "");
}

asock_t * asock_create_ns(int domain, int type, int protocol, const char* net_ns)
{
    char *tmp = strdup(net_ns);
    if (tmp == NULL)
        return NULL;

    asock_service_t *service = asock_service_mgr_get_service_for_nmsp(&s_service_manager, net_ns);
    if (service == NULL)
        return NULL;

    asock_t *con = asock_service_create_asock(service, domain, type, protocol);
    if (con == NULL)
        return NULL;

    con->m_net_ns = tmp;
    return con;
}


void asock_destroy(asock_t *con)
{
    asock_destroy_asock(con);
}

AsockReturnCode asock_connect(asock_t *con, const struct sockaddr *addr, socklen_t addrlen)
{
    asock_service_t *service = asock_service_mgr_get_service_for_nmsp(&s_service_manager, con->m_net_ns);
    if (service == NULL)
        return ASOCK_FAILURE;

    return asock_service_connect(service, con, addr, addrlen);
}



/////////////////////////////////////////////////////


static uint64_t asock_cons_number = 0;
pthread_mutex_t asock_cons_number_mutex;

asock_t * asock_create_asock(int domain, int type, int protocol)
{
    int fd = socket(domain, type, protocol);
    if (fd == -1) {
        SYS_LOG_ERROR("Failed to implement socket");
        return NULL;
    }

    asock_t *result = asock_fcreate_asock(fd);
    if (result == NULL)
        close(fd);
    return result;
}

asock_t * asock_fcreate_asock(int fd)
{
    asock_t *new_con = (asock_t *)malloc(sizeof(asock_t));
    if (new_con == NULL) {
        SYS_LOG_ERROR("Failed to allocate memory for new connection.\n");
        return NULL;
    }

    new_con->m_in_queue = asock_tsqueue_create(sizeof(uint8_t), ASOCK_MES_BUFFER_SIZE);
    if (new_con->m_in_queue == NULL) {
        SYS_LOG_ERROR("Failed to allocate memory for in queue for new connection.\n");
        free(new_con);
        return NULL;
    }

    new_con->m_out_queue = asock_tsqueue_create(sizeof(uint8_t), ASOCK_MES_BUFFER_SIZE);
    if (new_con->m_out_queue == NULL) {
        SYS_LOG_ERROR("Failed to allocate memory for out queue for new connection.\n");
        asock_tsqueue_destroy(new_con->m_in_queue);
        free(new_con);
        return NULL;
    }

    new_con->m_sock_desc = fd;

    pthread_mutex_lock(&asock_cons_number_mutex);
    new_con->m_connection_id = asock_cons_number++;
    pthread_mutex_unlock(&asock_cons_number_mutex);

    new_con->m_master_id = -1;

    new_con->m_state = Idle;
    new_con->m_disconnect = 0;

    new_con->m_peer_addr = NULL;
    new_con->m_peer_addrlen = 0;

    new_con->m_host_addr = NULL;
    new_con->m_host_addrlen = 0;

    new_con->m_net_ns = NULL;

    new_con->m_ref_counter = 1;
    new_con->m_ref_counter_mutex = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;

    return new_con;
}

void asock_destroy_asock(asock_t *con)
{
    // It is possible to destroy NULL ptr (like  "free" function)
    if (con == NULL)
        return;

    con->m_disconnect = 1;

    int delete_con = 0;

    pthread_mutex_lock(&con->m_ref_counter_mutex);
    --con->m_ref_counter;

    if (con->m_ref_counter == 0) {
        if (con->m_sock_desc != -1)
            close(con->m_sock_desc);
        asock_tsqueue_destroy(con->m_in_queue);
        asock_tsqueue_destroy(con->m_out_queue);
        if (con->m_peer_addr)
            free(con->m_peer_addr);
        if (con->m_host_addr)
            free(con->m_host_addr);
        if (con->m_net_ns)
            free(con->m_net_ns);
        delete_con = 1;
//        free(con);
    }
    pthread_mutex_unlock(&con->m_ref_counter_mutex);

    if (delete_con)
        free(con);
}


void asock_con_counter_incr(asock_t *con)
{
    assert(con);

    pthread_mutex_lock(&con->m_ref_counter_mutex);
    ++con->m_ref_counter;
    pthread_mutex_unlock(&con->m_ref_counter_mutex);
}



ssize_t asock_write(asock_t *con, const void *src, size_t src_sz)
{
    assert(con);
    if (asock_tsqueue_enqueue(con->m_out_queue, src, src_sz) == EXIT_SUCCESS)
        return src_sz;
    else
        return 0;
}

ssize_t asock_read(asock_t *con, void *dst, size_t dst_sz)
{
    assert(con);
    return asock_tsqueue_dequeue(con->m_in_queue, dst, dst_sz);
}

enum AsockConState asock_state(asock_t *con)
{
    assert(con);
    return con->m_state;
}


AsockReturnCode asock_bind(asock_t *con, const struct sockaddr *addr, socklen_t addrlen)
{
    struct sockaddr *new_addr = malloc(addrlen);
    if (new_addr == NULL) {
        SYS_LOG_ERROR("Failed to allocate memory for asock address");
        return ASOCK_ERR_NOMEM;
    }

    if (bind(con->m_sock_desc, addr, addrlen) == -1) {
        free(new_addr);
        SYS_LOG_ERROR("Failed to bind asock");
        return ASOCK_FAILURE;
    }

    memcpy(new_addr, addr, addrlen);
    con->m_host_addr = new_addr;
    con->m_host_addrlen = addrlen;
    return ASOCK_SUCCESS;
}



int32_t asock_get_unique_service_id()
{
    int32_t result = 0;
    pthread_mutex_lock(&s_services_created_mutex);
    result = ++s_servicies_created;
    pthread_mutex_unlock(&s_services_created_mutex);
    return result;
}
