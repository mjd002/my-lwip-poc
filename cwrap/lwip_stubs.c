/* Minimal lwIP stub implementations for building a focused DLL POC.
 * These stubs allow linking lwip_init and a few other core symbols
 * without pulling in the entire lwIP core.
 */

#include <stdint.h>

/* Provide simple stats objects used by some core files */
struct stats_branch { int dummy; };
struct stats_branch lwip_stats_branch_dummy;
struct stats_branch *lwip_stats = &lwip_stats_branch_dummy;

/* Note: netif_init is provided by core/netif.c when included; do not
 * define it here to avoid duplicate symbol. */

/* Minimal protocol init stubs */
void udp_init(void) { }
void tcp_init(void) { }

/* Minimal timers / sys functions */
unsigned long sys_now(void) { return 0; }

/* Some tcp helper symbols used by pbuf/timers when building small sets */
void tcp_tmr(void) { }

/* Minimal TCP symbols referenced by pbuf and other core files */
void tcp_segs_free(void *seg) { (void)seg; }
void *tcp_active_pcbs = NULL;

/* Note: etharp and many netif helpers will be provided by real lwIP sources
 * (we prefer to include `etharp.c` from lwIP rather than stub it here).
 */

/* Minimal tcp symbols referenced by some core files */
void tcp_abort(void *pcb) { (void)pcb; }
void *tcp_listen_pcbs = NULL;

/* SNMP and IGMP minimal stubs */
void snmp_inc_iflist(void) { }
void snmp_dec_iflist(void) { }
void snmp_delete_ipaddridx_tree(void *ni) { (void)ni; }

int igmp_start(void *netif) { (void)netif; return 0; }
int igmp_stop(void *netif) { (void)netif; return 0; }

/* Minimal lwip_init implementation to call the per-protocol inits.
 * The real lwip_init resides in lwIP core; we expose a small compatible
 * implementation for the POC.
 */
void lwip_init(void) {
    /* initialize simple subsystems */
    tcp_init();
    udp_init();
    /* netif_init will be executed from the real netif.c if linked in */
}

/* Minimal input path stubs for higher-level protocols. Real implementations
 * live in raw.c, tcp_in.c and udp.c; we provide no-op stubs here for the
 * POC so ip_input/ip_output can link without including the full TCP/UDP
 * stacks. These return 0 (ERR_OK) to indicate they 'handled' nothing.
 */
int raw_input(void *p, void *inp) { (void)p; (void)inp; return 0; }
int tcp_input(void *p, void *inp) { (void)p; (void)inp; return 0; }
int udp_input(void *p, void *inp) { (void)p; (void)inp; return 0; }

/* ip_input and ip_route are provided by the real lwIP IPv4 sources included in the build. */
