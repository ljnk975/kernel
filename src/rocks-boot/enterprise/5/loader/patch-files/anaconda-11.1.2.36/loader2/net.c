/*
 * Copyright 1999-2006 Red Hat, Inc.
 *
 * David Cantrell <dcantrell@redhat.com>
 * 
 * All Rights Reserved.
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
 * OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 * Except as contained in this notice, the name of Red Hat shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from Red Hat.
 *
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <popt.h>
#include <resolv.h>
#include <net/if.h>
#include <newt.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <kudzu/kudzu.h>

#include "../isys/dns.h"
#include "../isys/isys.h"
#include "../isys/net.h"
#include "../isys/wireless.h"
#include "../isys/nl.h"
#include "../isys/str.h"

#include "lang.h"
#include "loader.h"
#include "loadermisc.h"
#include "log.h"
#include "method.h"
#include "net.h"
#include "windows.h"

/* boot flags */
extern int flags;

char *netServerPrompt = \
    N_("Please enter the following information:\n"
       "\n"
       "    o the name or IP number of your %s server\n" 
       "    o the directory on that server containing\n" 
       "      %s for your architecture\n");

/**
 * Callback function for the CIDR entry boxes on the manual TCP/IP
 * configuration window.
 *
 * @param co The entry field that triggered the callback.
 * @param dptr Pointer to intfconfig_s data structure for this field.
 * @see intfconfig_s
 */
static void cidrCallback(newtComponent co, void * dptr) {
    struct intfconfig_s * data = dptr;
    int cidr, upper = 0;

    if (co == data->cidr4Entry) {
        if (data->cidr4 == NULL && data->ipv4 == NULL)
            return;

        cidr = atoi(data->cidr4);
        upper = 32;
    } else if (co == data->cidr6Entry) {
        if (data->cidr6 == NULL && data->ipv6 == NULL)
            return;

        cidr = atoi(data->cidr6);
        upper = 128;
    }

    if (upper != 0) {
        if (cidr < 1 || cidr > upper) {
            newtWinMessage(_("Invalid Prefix"), _("Retry"),
                           _("Prefix must be between 1 and 32 "
                             "for IPv4 networks or between 1 and 128 "
                             "for IPv6 networks"));
        }
    }
}

static void ipCallback(newtComponent co, void * dptr) {
    int i;
    char *buf, *octet;
    struct intfconfig_s * data = dptr;

    if (co == data->ipv4Entry) {
        /* do we need to guess a netmask for the user? */
        if (data->cidr4 == NULL && data->ipv4 != NULL) {
            buf = strdup(data->ipv4);
            octet = strtok(buf, ".");
            i = atoi(octet);

            free(buf);
            free(octet);

            if (i >= 0 && i <= 127)
                newtEntrySet(data->cidr4Entry, "8", 1);
            else if (i >= 128 && i <= 191)
                newtEntrySet(data->cidr4Entry, "16", 1);
            else if (i >= 192 && i <= 222)
                newtEntrySet(data->cidr4Entry, "24", 1);
        }

        return;
    } else if (co == data->ipv6Entry) {
        /* users must provide a mask, we can't guess for ipv6 */
        return;
    }
}

static void setMethodSensitivity(void *dptr, int radio_button_count) {
    int i = 0;

    for (i = 0; i < radio_button_count; i++) {
        newtCheckboxSetFlags(*((newtComponent *) dptr), NEWT_FLAG_DISABLED,
                             NEWT_FLAGS_TOGGLE);
        dptr += sizeof (newtComponent);
    }

    return;
}

static void v4MethodCallback(newtComponent co, void *dptr) {
    setMethodSensitivity(dptr, 2);
    return;
}

static void v6MethodCallback(newtComponent co, void *dptr) {
    setMethodSensitivity(dptr, 3);
    return;
}

static int waitForLink(char * dev) {
    extern int num_link_checks;
    int tries = 0;

    /* try to wait for a valid link -- if the status is unknown or
     * up continue, else sleep for 1 second and try again for up
     * to five times */
    logMessage(DEBUGLVL, "waiting for link %s...", dev);
    while (tries < num_link_checks) {
      if (get_link_status(dev) != 0)
            break;
        sleep(1);
        tries++;
    }
    logMessage(DEBUGLVL, "   %d seconds.", tries);
    if (tries < num_link_checks)
        return 0;
    logMessage(WARNING, "    no network link detected on %s", dev);
    return 1;
}

static void parseEthtoolSettings(struct loaderData_s * loaderData) {
    char * option, * buf;
    ethtool_duplex duplex = ETHTOOL_DUPLEX_UNSPEC;
    ethtool_speed speed = ETHTOOL_SPEED_UNSPEC;
    
    buf = strdup(loaderData->ethtool);
    option = strtok(buf, " ");
    while (option) {
        if (option[strlen(option) - 1] == '\"')
            option[strlen(option) - 1] = '\0';
        if (option[0] == '\"')
            option++;
        if (!strncmp(option, "duplex", 6)) {
            if (!strncmp(option + 7, "full", 4)) 
                duplex = ETHTOOL_DUPLEX_FULL;
            else if (!strncmp(option + 7, "half", 4))
                duplex = ETHTOOL_DUPLEX_HALF;
            else
                logMessage(WARNING, "Unknown duplex setting: %s", option + 7);
            option = strtok(NULL, " ");
        } else if (!strncmp("speed", option, 5)) {
            if (!strncmp(option + 6, "1000", 4))
                speed = ETHTOOL_SPEED_1000;
            else if (!strncmp(option + 6, "100", 3))
                speed = ETHTOOL_SPEED_100;
            else if (!strncmp(option + 6, "10", 2))
                speed = ETHTOOL_SPEED_10;
            else
                logMessage(WARNING, "Unknown speed setting: %s", option + 6);
            option = strtok(NULL, " ");
        } else {
            logMessage(WARNING, "Unknown ethtool setting: %s", option);
        }
        option = strtok(NULL, " ");
    }
    setEthtoolSettings(loaderData->netDev, speed, duplex);
    free(buf);
}

void initLoopback(void) {
    NLH_t nh;
    NIC_t nic;
    uint32_t nflags;

    /* open nic handle and set device name */
    nh = nic_open(nic_sys_logger);
    nic = nic_by_name(nh, "lo");

    /* bring the interface up */
    nflags = nic_get_flags(nic);
    if ((nflags & (IFF_UP | IFF_RUNNING)) == 0) {
        nic_set_flags(nic, nflags | IFF_UP | IFF_RUNNING);
        nic_update(nic);
    }

    /* clean up */
    nic_close(&nh);

    return;
}

static int getWirelessConfig(struct networkDeviceConfig *cfg, char * ifname) {
    char * wepkey = "";
    char * essid = "";
    int rc = 0;
    char * buf;

    if (cfg->wepkey != NULL) {
        wepkey = strdup(cfg->wepkey);
    }
    if (cfg->essid != NULL) {
        essid = strdup(cfg->essid);
    } else {
        essid = get_essid(ifname);
    }

    buf = sdupprintf(_("%s is a wireless network adapter.  Please "
                       "provide the ESSID and encryption key needed "
                       "to access your wireless network.  If no key "
                       "is needed, leave this field blank and the "
                       "install will continue."), ifname);
    do {
        struct newtWinEntry entry[] = { { N_("ESSID"), (const char **)&essid, 0 },
                                        { N_("Encryption Key"), (const char **) &wepkey, 0 },
                                        { NULL, NULL, 0 } };

        rc = newtWinEntries(_("Wireless Settings"), buf,
                            40, 5, 10, 30, entry, _("OK"), _("Back"), NULL);
        if (rc == 2) return LOADER_BACK;

        /* set stuff up */
    } while (rc == 2);

    if (cfg->wepkey != NULL) 
        free(cfg->wepkey);

    if (wepkey && (strlen(wepkey) > 0))
        cfg->wepkey = strdup(wepkey);
    else
        cfg->wepkey = NULL;

    if (cfg->essid != NULL)
        free(cfg->essid);

    if (essid && (strlen(essid) > 0))
        cfg->essid = strdup(essid);
    else
        cfg->essid = NULL;

    return LOADER_OK;
}

static int getDnsServers(struct networkDeviceConfig * cfg) {
    int rc;
    struct in_addr addr;
    struct in6_addr addr6;
    const char * ns = "";
    struct newtWinEntry entry[] = { { N_("Nameserver IP"), &ns, 0 },
                                      { NULL, NULL, 0 } };

    do {
        rc = newtWinEntries(_("Missing Nameserver"), 
                _("Your IP address request returned configuration "
                  "information, but it did not include a nameserver address. "
                  "If you do not have this information, you can leave "
                  "the field blank and the install will continue."),
                61, 0, 0, 45, entry, _("OK"), _("Back"), NULL);

        if (rc == 2) return LOADER_BACK;

        rc = 0;
        if (!ns || !*ns) {
            cfg->dev.numDns = 0;
            break;
        } else {
            if (inet_pton(AF_INET, ns, &addr) >= 1)
                cfg->dev.dnsServers[0] = ip_addr_in(&addr);
            else if (inet_pton(AF_INET6, ns, &addr6) >= 1)
                cfg->dev.dnsServers[0] = ip_addr_in6(&addr6);
            else
                rc = 2;
        }

        if (rc) {
            newtWinMessage(_("Invalid IP Information"), _("Retry"),
                           _("You entered an invalid IP address."));
        } else {
            cfg->dev.set |= PUMP_NETINFO_HAS_DNS;
            cfg->dev.numDns = 1;
        }
    } while (rc == 2);

    return LOADER_OK;
}

void printLoaderDataIPINFO(struct loaderData_s *loaderData) {
    logMessage(DEBUGLVL, "loaderData->ipinfo_set   = |%d|", loaderData->ipinfo_set);
    logMessage(DEBUGLVL, "loaderData->ip           = |%s|", loaderData->ip);
    logMessage(DEBUGLVL, "loaderData->ipv6info_set = |%d|", loaderData->ipv6info_set);
    logMessage(DEBUGLVL, "loaderData->ipv6         = |%s|", loaderData->ipv6);
    logMessage(DEBUGLVL, "loaderData->netmask      = |%s|", loaderData->netmask);
    logMessage(DEBUGLVL, "loaderData->gateway      = |%s|", loaderData->gateway);
    logMessage(DEBUGLVL, "loaderData->dns          = |%s|", loaderData->dns);
    logMessage(DEBUGLVL, "loaderData->hostname     = |%s|", loaderData->hostname);
    logMessage(DEBUGLVL, "loaderData->noDns        = |%d|", loaderData->noDns);
    logMessage(DEBUGLVL, "loaderData->netDev_set   = |%d|", loaderData->netDev_set);
    logMessage(DEBUGLVL, "loaderData->netDev       = |%s|", loaderData->netDev);
    logMessage(DEBUGLVL, "loaderData->netCls_set   = |%d|", loaderData->netCls_set);
    logMessage(DEBUGLVL, "loaderData->netCls       = |%s|", loaderData->netCls);
}

/* given loader data from kickstart, populate network configuration struct */
void setupNetworkDeviceConfig(struct networkDeviceConfig * cfg, 
                              struct loaderData_s * loaderData) {
    struct in_addr addr;
    struct in6_addr addr6;
    char * c;

    /* set to 1 to get ks network struct logged */
#if 0
    printLoaderDataIPINFO(loaderData);
#endif

    cfg->noipv4 = 0;
    if (FL_NOIPV6(flags)) {
        cfg->noipv6 = 1;
    } else {
        cfg->noipv6 = 0;
    }

    if (loaderData->ethtool) {
        parseEthtoolSettings(loaderData);
    }

    if (loaderData->netCls_set) {
        cfg->vendor_class = loaderData->netCls;
    } else {
        cfg->vendor_class = NULL;
    }

    if (loaderData->ipinfo_set) {
        if (is_wireless_interface(loaderData->netDev)) {
            if (loaderData->essid) {
                logMessage(INFO, "setting specified essid of %s",
                           loaderData->essid);
                cfg->essid = strdup(loaderData->essid);
            }
            if (loaderData->wepkey) {
                logMessage(INFO, "setting specified wepkey");
                cfg->wepkey = strdup(loaderData->wepkey);
            }
            /* go ahead and set up the wireless interface in case 
             * we're using dhcp */
            setupWireless(cfg);
        }

        /* this is how we specify dhcp */
        if (!strncmp(loaderData->ip, "dhcp", 4)) {
            char *ret = NULL;

            /* JKFIXME: this soooo doesn't belong here.  and it needs to
             * be broken out into a function too */
            logMessage(INFO, "sending dhcp request through device %s",
                       loaderData->netDev);

            if (!FL_CMDLINE(flags)) {
                startNewt();
                winStatus(55, 3, NULL, 
                          _("Sending request for IP information for %s..."), 
                          loaderData->netDev, 0);
            } else {
                printf("Sending request for IP information for %s...\n", 
                       loaderData->netDev);
            }

            if (!FL_TESTING(flags)) {
                waitForLink(loaderData->netDev);
                cfg->noipv4 = loaderData->noipv4;
                cfg->noipv6 = loaderData->noipv6;
                cfg->noDns = loaderData->noDns;
                ret = doDhcp(cfg);
            }

            if (!FL_CMDLINE(flags))
                newtPopWindow();

            if (ret != NULL) {
                logMessage(DEBUGLVL, "dhcp: %s", ret);
                return;
            }

            cfg->isDynamic = 1;
            cfg->preset = 1;
        } else if (inet_pton(AF_INET, loaderData->ip, &addr) >= 1) {
            cfg->dev.ip = ip_addr_in(&addr);
            cfg->dev.ipv4 = ip_addr_in(&addr);
            cfg->dev.set |= PUMP_INTFINFO_HAS_IP|PUMP_INTFINFO_HAS_IPV4_IP;
            cfg->isDynamic = 0;
            cfg->preset = 1;
        } else if (inet_pton(AF_INET6, loaderData->ip, &addr6) >= 1) {
            cfg->dev.ip = ip_addr_in6(&addr6);
            cfg->dev.ipv6 = ip_addr_in6(&addr6);
            cfg->dev.set |= PUMP_INTFINFO_HAS_IP|PUMP_INTFINFO_HAS_IPV6_IP;
            cfg->isDynamic = 0;
            cfg->preset = 1;
        } else { /* invalid ip information, disable the setting of ip info */
            loaderData->ipinfo_set = 0;
            cfg->isDynamic = 0;
            loaderData->ip = NULL;
        }
    }

    if (loaderData->netmask && (inet_pton(AF_INET, loaderData->netmask, &addr) >= 1)) {
        cfg->dev.netmask = ip_addr_in(&addr);
        cfg->dev.set |= PUMP_INTFINFO_HAS_NETMASK;
    }

    if (loaderData->netmask && (inet_pton(AF_INET6, loaderData->netmask, &addr6) >= 1)) {
        cfg->dev.netmask = ip_addr_in6(&addr6);
        cfg->dev.set |= PUMP_INTFINFO_HAS_NETMASK;
    }

    if (loaderData->gateway && (inet_pton(AF_INET, loaderData->gateway, &addr) >= 1)) {
        cfg->dev.gateway = ip_addr_in(&addr);
        cfg->dev.set |= PUMP_NETINFO_HAS_GATEWAY;
    }

    if (loaderData->gateway && (inet_pton(AF_INET6, loaderData->gateway, &addr6) >= 1)) {
        cfg->dev.gateway = ip_addr_in6(&addr6);
        cfg->dev.set |= PUMP_NETINFO_HAS_GATEWAY;
    }

    if (loaderData->dns) {
        char * buf;
        char ret[48];
        buf = strdup(loaderData->dns);

        /* Scan the dns parameter for multiple comma-separated IP addresses */
        c = strtok(buf, ",");  
        while ((cfg->dev.numDns < MAX_DNS_SERVERS) && (c != NULL)) {
            if (inet_pton(AF_INET, c, &addr) >= 1) {
                cfg->dev.dnsServers[cfg->dev.numDns] = ip_addr_in(&addr);
                cfg->dev.numDns++;
                inet_ntop(AF_INET, &addr, ret, INET_ADDRSTRLEN);
                logMessage(DEBUGLVL, "adding dns4 %s", ret);
                c = strtok(NULL, ",");
            } else if (inet_pton(AF_INET6, c, &addr6) >= 1) {
                cfg->dev.dnsServers[cfg->dev.numDns] = ip_addr_in6(&addr6);
                cfg->dev.numDns++;
                inet_ntop(AF_INET6, &addr6, ret, INET6_ADDRSTRLEN);
                logMessage(DEBUGLVL, "adding dns6 %s", ret);
                c = strtok(NULL, ",");
            }
        }
        logMessage(INFO, "dnsservers is %s", loaderData->dns);
        if (cfg->dev.numDns)
            cfg->dev.set |= PUMP_NETINFO_HAS_DNS;
    }

    if (loaderData->hostname) {
        logMessage(INFO, "setting specified hostname of %s",
                   loaderData->hostname);
        cfg->dev.hostname = strdup(loaderData->hostname);
        cfg->dev.set |= PUMP_NETINFO_HAS_HOSTNAME;
    }

    if (loaderData->mtu) {
        cfg->mtu = loaderData->mtu;
        cfg->dev.mtu = loaderData->mtu;
        cfg->dev.set |= PUMP_INTFINFO_HAS_MTU;
    }

    if (loaderData->peerid) {
        cfg->peerid = strdup(loaderData->peerid);
    }

    if (loaderData->subchannels) {
        cfg->subchannels = strdup(loaderData->subchannels);
    }

    if (loaderData->ctcprot) {
        cfg->ctcprot = strdup(loaderData->ctcprot);
    }

    if (loaderData->portname) {
        cfg->portname = strdup(loaderData->portname);
    }

    if (loaderData->nettype) {
        cfg->nettype = strdup(loaderData->nettype);
    }

    if (loaderData->ethtool) {
        parseEthtoolSettings(loaderData);
    }

    cfg->noipv4 = loaderData->noipv4;
    cfg->noipv6 = loaderData->noipv6;
    cfg->noDns = loaderData->noDns;
}

int readNetConfig(char * device, struct networkDeviceConfig * cfg,
                  char * dhcpclass, int methodNum) {
    struct networkDeviceConfig newCfg;
    int ret;
    int i = 0;
    struct netconfopts opts;
    struct in_addr addr, nm, nw;
    struct in6_addr addr6;
    struct intfconfig_s ipcomps;

    memset(&ipcomps, '\0', sizeof(ipcomps));
    ipcomps.ipv4 = NULL;
    ipcomps.ipv6 = NULL;
    ipcomps.cidr4 = NULL;
    ipcomps.cidr6 = NULL;
    ipcomps.gw = NULL;
    ipcomps.ns = NULL;

    /* init opts */
    opts.ipv4Choice = 0;
    opts.ipv6Choice = 0;

    /* init newCfg */
    memset(&newCfg, '\0', sizeof(newCfg));
    strcpy(newCfg.dev.device, device);
    newCfg.essid = NULL;
    newCfg.wepkey = NULL;
    newCfg.isDynamic = cfg->isDynamic;
    newCfg.noDns = cfg->noDns;
    newCfg.preset = cfg->preset;
    if (dhcpclass) {
        newCfg.vendor_class = strdup(dhcpclass);
    } else {
        newCfg.vendor_class = NULL;
    }

    /* JKFIXME: we really need a way to override this and be able to change
     * our network config */
    if (!FL_TESTING(flags) && cfg->preset) {
        logMessage(INFO, "doing kickstart... setting it up");
        if (configureNetwork(cfg)) {
            newtWinMessage(_("Network Error"), _("Retry"),
                           _("There was an error configuring your network "
                             "interface."));
            return LOADER_BACK;
        }

        findHostAndDomain(cfg);

        if (!cfg->noDns)
            writeResolvConf(cfg);
        return LOADER_NOOP;
    }        

    /* handle wireless device configuration */
    if (is_wireless_interface(device)) {
        logMessage(INFO, "%s is a wireless adapter", device);
        if (getWirelessConfig(cfg, device) == LOADER_BACK)
            return LOADER_BACK;

        if (cfg->essid != NULL)
            newCfg.essid = strdup(cfg->essid);

        if (cfg->wepkey != NULL)
            newCfg.wepkey = strdup(cfg->wepkey);
    } else {
        logMessage(INFO, "%s is not a wireless adapter", device);
    }

    /* dhcp/manual network configuration loop */
    i = 1;
    while (i == 1) {
        ret = configureTCPIP(device, cfg, &newCfg, &opts, methodNum);

        if (ret == LOADER_NOOP) {
            /* dhcp selected, proceed */
            i = 0;
        } else if (ret == LOADER_OK) {
            /* do manual configuration */
            ret = manualNetConfig(device, cfg, &newCfg, &ipcomps, &opts);

            if (ret == LOADER_BACK) {
                continue;
            } else if (ret == LOADER_OK) {
                i = 0;
            }
        } else if (ret == LOADER_BACK) {
            return LOADER_BACK;
        }
    }

    cfg->noipv4 = newCfg.noipv4;
    cfg->ipv4method = newCfg.ipv4method;
    cfg->noipv6 = newCfg.noipv6;
    cfg->ipv6method = newCfg.ipv6method;

    /* preserve extra dns servers for the sake of being nice */
    if (cfg->dev.numDns > newCfg.dev.numDns) {
        for (i = newCfg.dev.numDns; i < cfg->dev.numDns; i++) {
            memcpy(&newCfg.dev.dnsServers[i], &cfg->dev.dnsServers[i],
                sizeof (newCfg.dev.dnsServers[i]));
        }
        newCfg.dev.numDns = cfg->dev.numDns;
    }

    cfg->isDynamic = newCfg.isDynamic;
    memcpy(&cfg->dev,&newCfg.dev,sizeof(newCfg.dev));

    if (!(cfg->dev.set & PUMP_NETINFO_HAS_GATEWAY)) {
        if (ipcomps.gw && *ipcomps.gw) {
            if (inet_pton(AF_INET, ipcomps.gw, &addr) >= 1) {
                cfg->dev.gateway = ip_addr_in(&addr);
                cfg->dev.set |= PUMP_NETINFO_HAS_GATEWAY;
            } else if (inet_pton(AF_INET6, ipcomps.gw, &addr6) >= 1) {
                cfg->dev.gateway = ip_addr_in6(&addr6);
                cfg->dev.set |= PUMP_NETINFO_HAS_GATEWAY;
            }
        }
    }

    /* calculate any missing IPv4 pieces */
    if (opts.ipv4Choice == '*') {
        addr = ip_in_addr(&cfg->dev.ipv4);
        nm = ip_in_addr(&cfg->dev.netmask);

        if (!(cfg->dev.set & PUMP_INTFINFO_HAS_NETWORK)) {
            cfg->dev.network = ip_addr_v4(ntohl((addr.s_addr) & nm.s_addr));
            cfg->dev.set |= PUMP_INTFINFO_HAS_NETWORK;
        }

        if (!(cfg->dev.set & PUMP_INTFINFO_HAS_BROADCAST)) {
            nw = ip_in_addr(&cfg->dev.network);
            cfg->dev.broadcast = ip_addr_v4(ntohl(nw.s_addr | ~nm.s_addr));
            cfg->dev.set |= PUMP_INTFINFO_HAS_BROADCAST;
        }
    }

    /* make sure we don't have a dhcp_nic handle for static */
    if ((cfg->isDynamic == 0) && (cfg->dev.dhcp_nic != NULL)) {
        dhcp_nic_free(cfg->dev.dhcp_nic);
        cfg->dev.dhcp_nic = NULL;
    }

    /* dump some network debugging info */
    debugNetworkInfo(cfg);

    /* bring up the interface */
    if (!FL_TESTING(flags)) {
        if (configureNetwork(cfg)) {
            newtWinMessage(_("Network Error"), _("Retry"),
                           _("There was an error configuring your network "
                             "interface."));
            return LOADER_BACK;
        }

        findHostAndDomain(cfg);
        writeResolvConf(cfg);
    }

    return LOADER_OK;
}

int configureTCPIP(char * device, struct networkDeviceConfig * cfg,
                   struct networkDeviceConfig * newCfg,
                   struct netconfopts * opts, int methodNum) {
    int i = 0, z = 0;
    char *dret = NULL;
    newtComponent f, okay, back, answer;
    newtComponent ipv4Checkbox, ipv6Checkbox, v4Method[2], v6Method[3];
    newtGrid grid, checkgrid, buttons;

    /* UI WINDOW 1: ask for ipv4 choice, ipv6 choice, and conf methods */

    /* IPv4 checkbox */
    if (!opts->ipv4Choice)
        opts->ipv4Choice = '*';

    ipv4Checkbox = newtCheckbox(-1, -1, _("Enable IPv4 support"),
                                opts->ipv4Choice, NULL, &(opts->ipv4Choice));
    v4Method[0] = newtRadiobutton(-1, -1, DHCP_METHOD_STR, 1, NULL);
    v4Method[1] = newtRadiobutton(-1, -1, MANUAL_METHOD_STR, 0, v4Method[0]);

    /* IPv6 checkbox */
    if (!opts->ipv6Choice) {
        if (FL_NOIPV6(flags))
            opts->ipv6Choice = ' ';
        else
            opts->ipv6Choice = '*';
    }

    ipv6Checkbox = newtCheckbox(-1, -1, _("Enable IPv6 support"),
                                opts->ipv6Choice, NULL, &(opts->ipv6Choice));
    v6Method[0] = newtRadiobutton(-1, -1, AUTO_METHOD_STR, 1, NULL);
    v6Method[1] = newtRadiobutton(-1, -1, DHCP_METHOD_STR, 0, v6Method[0]);
    v6Method[2] = newtRadiobutton(-1, -1, MANUAL_METHOD_STR, 0, v6Method[1]);

    /* button bar at the bottom of the window */
    buttons = newtButtonBar(_("OK"), &okay, _("Back"), &back, NULL);

    /* checkgrid contains the toggle options for net configuration */
    checkgrid = newtCreateGrid(1, 8);

    newtGridSetField(checkgrid, 0, 0, NEWT_GRID_COMPONENT, ipv4Checkbox,
                     0, 0, 0, 0, NEWT_ANCHOR_LEFT, 0);
    for (i = 1; i < 3; i++)
        newtGridSetField(checkgrid, 0, i, NEWT_GRID_COMPONENT, v4Method[i-1],
                         7, 0, 0, 0, NEWT_ANCHOR_LEFT, 0);

    newtGridSetField(checkgrid, 0, 4, NEWT_GRID_COMPONENT, ipv6Checkbox,
                     0, 1, 0, 0, NEWT_ANCHOR_LEFT, 0);
    for (i = 5; i < 8; i++)
        newtGridSetField(checkgrid, 0, i, NEWT_GRID_COMPONENT, v6Method[i-5],
                         7, 0, 0, 0, NEWT_ANCHOR_LEFT, 0);

    /* main window layout */
    grid = newtCreateGrid(1, 2);
    newtGridSetField(grid, 0, 0, NEWT_GRID_SUBGRID, checkgrid,
                     0, 0, 0, 1, 0, 0);
    newtGridSetField(grid, 0, 1, NEWT_GRID_SUBGRID, buttons,
                     0, 0, 0, 0, 0, NEWT_GRID_FLAG_GROWX);

    f = newtForm(NULL, NULL, 0);
    newtGridAddComponentsToForm(grid, f, 1);
    newtGridWrappedWindow(grid, _("Configure TCP/IP"));
    newtGridFree(grid, 1);

    /* callbacks */
    newtComponentAddCallback(ipv4Checkbox, v4MethodCallback, &v4Method);
    newtComponentAddCallback(ipv6Checkbox, v6MethodCallback, &v6Method);

    /* match radio button sensitivity to initial checkbox choices */
    if (opts->ipv4Choice == ' ')
        setMethodSensitivity(&v4Method, 2);

    if (opts->ipv6Choice == ' ')
        setMethodSensitivity(&v6Method, 3);

    /* run the form */
    do {
        answer = newtRunForm(f);

        if (answer == back) {
            newtFormDestroy(f);
            newtPopWindow();
            return LOADER_BACK;
        }

        /* need at least one stack */
        if (opts->ipv4Choice == ' ' && opts->ipv6Choice == ' ') {
            newtWinMessage(_("Missing Protocol"), _("Retry"),
                           _("You must select at least one protocol (IPv4 "
                             "or IPv6)."));
            continue;
        }

        /* NFS only works over IPv4 */
        if (opts->ipv4Choice == ' ' && methodNum == METHOD_NFS) {
            newtWinMessage(_("IPv4 Needed for NFS"), _("Retry"),
                           _("NFS installation method requires IPv4 support."));
            continue;
        }

        /* what TCP/IP stacks do we use? what conf methods? */
        if (opts->ipv4Choice == '*') {
            newCfg->noipv4 = 0;
            for (z = 0; z < 2; z++)
                if (newtRadioGetCurrent(v4Method[0]) == v4Method[z])
                    newCfg->ipv4method = z;
        } else {
            newCfg->noipv4 = 1;
        }

        if (opts->ipv6Choice == '*') {
            newCfg->noipv6 = 0;
            for (z = 0; z < 3; z++)
                if (newtRadioGetCurrent(v6Method[0]) == v6Method[z])
                    newCfg->ipv6method = z;
        } else {
            newCfg->noipv6 = 1;
        }

        /* do interface configuration (call DHCP here, or return for manual) */
        if ((!newCfg->noipv4 && newCfg->ipv4method == IPV4_DHCP_METHOD) ||
            (!newCfg->noipv6 && (newCfg->ipv6method == IPV6_AUTO_METHOD ||
                                 newCfg->ipv6method == IPV6_DHCP_METHOD))) {
            /* do DHCP if selected */
            if (!FL_TESTING(flags)) {
                winStatus(55, 3, NULL,
                          _("Sending request for IP information for %s..."),
                          device, 0);
                waitForLink(device);
                dret = doDhcp(newCfg);
                newtPopWindow();
            }

            if (dret == NULL) {
                newCfg->isDynamic = 1;
                if (!(newCfg->dev.set & PUMP_NETINFO_HAS_DNS)) {
                    logMessage(WARNING,
                        "dhcp worked, but did not return a DNS server");

                    /*
                     * prompt for a nameserver IP address when:
                     * - DHCP for IPv4, DHCP/AUTO for IPv6 and both enabled
                     * - IPv4 disabled and DHCP/AUTO for IPv6
                     * - IPv6 disabled and DHCP for IPv4
                     */
                    if ((newCfg->ipv4method == IPV4_DHCP_METHOD
                         && (newCfg->ipv6method == IPV6_AUTO_METHOD ||
                             newCfg->ipv6method == IPV6_DHCP_METHOD))
                        || (newCfg->ipv4method == IPV4_DHCP_METHOD
                            && newCfg->noipv6)
                        || (newCfg->noipv4
                            && (newCfg->ipv6method == IPV6_AUTO_METHOD ||
                                newCfg->ipv6method == IPV6_DHCP_METHOD))) {
                        i = getDnsServers(newCfg);
                        i = i ? 0 : 1;
                    } else {
                        i = 1;
                    }
                } else {
                    i = 1;
                }
            } else {
                logMessage(DEBUGLVL, "dhcp: %s", dret);
                i = 0;
            }
        } else {
            /* manual IP configuration for IPv4 and IPv6 */
            newtFormDestroy(f);
            newtPopWindow();
            return LOADER_OK;
        }
    } while (i != 1);

    newtFormDestroy(f);
    newtPopWindow();

    if ((!newCfg->noipv4 && newCfg->ipv4method == IPV4_MANUAL_METHOD) ||
        (!newCfg->noipv6 && newCfg->ipv6method == IPV6_MANUAL_METHOD))
        return LOADER_OK;
    else
        return LOADER_NOOP;
}

int manualNetConfig(char * device, struct networkDeviceConfig * cfg,
                    struct networkDeviceConfig * newCfg,
                    struct intfconfig_s * ipcomps, struct netconfopts * opts) {
    int i, rows, pos, primary, prefix, cidr, q, have[2], stack[2];
    char *buf = NULL;
    char ret[48];
    ip_addr_t *tip;
    struct in_addr addr;
    struct in6_addr addr6;
    newtComponent f, okay, back, answer;
    newtGrid egrid = NULL;
    newtGrid qgrid = NULL;
    newtGrid rgrid = NULL;
    newtGrid buttons, grid;
    newtComponent text = NULL;

    /* so we don't perform this test over and over */
    stack[IPV4] = opts->ipv4Choice == '*'
                  && newCfg->ipv4method == IPV4_MANUAL_METHOD;
    stack[IPV6] = opts->ipv6Choice == '*'
                  && newCfg->ipv6method == IPV6_MANUAL_METHOD;

    /* UI WINDOW 2 (optional): manual IP config for non-DHCP installs */
    rows = 2;
    for (i = 0; i < 2; i++)
        if (stack[i]) rows++;
    egrid = newtCreateGrid(4, rows);

    pos = 0;

    /* IPv4 entry items */
    if (stack[IPV4]) {
        newtGridSetField(egrid, 0, pos, NEWT_GRID_COMPONENT,
                         newtLabel(-1, -1, _("IPv4 address:")),
                         0, 0, 0, 0, NEWT_ANCHOR_LEFT, 0);

        ipcomps->ipv4Entry = newtEntry(-1, -1, NULL, 16, &ipcomps->ipv4, 0);
        ipcomps->cidr4Entry = newtEntry(-1, -1, NULL, 16, &ipcomps->cidr4, 0);

        /* use a nested grid for ipv4 addr & netmask */
        qgrid = newtCreateGrid(3, 1);

        newtGridSetField(qgrid, 0, 0, NEWT_GRID_COMPONENT,
                         ipcomps->ipv4Entry, 1, 0, 0, 0, NEWT_ANCHOR_LEFT, 0);
        newtGridSetField(qgrid, 1, 0, NEWT_GRID_COMPONENT,
                         newtLabel(-1, -1, _("/")),
                         1, 0, 0, 0, NEWT_ANCHOR_LEFT, 0);
        newtGridSetField(qgrid, 2, 0, NEWT_GRID_COMPONENT,
                         ipcomps->cidr4Entry, 1, 0, 0, 0, NEWT_ANCHOR_LEFT, 0);

        newtGridSetField(egrid, 1, pos, NEWT_GRID_SUBGRID, qgrid,
                         0, 0, 0, 0, NEWT_ANCHOR_LEFT, 0);

        newtComponentAddCallback(ipcomps->ipv4Entry, ipCallback, &ipcomps);
        newtComponentAddCallback(ipcomps->cidr4Entry, cidrCallback, &ipcomps);

        /* populate fields if we have data already */
        tip = NULL;
        if (cfg->dev.set & PUMP_INTFINFO_HAS_IPV4_IP)
            tip = &(cfg->dev.ipv4);
        else if (newCfg->dev.set & PUMP_INTFINFO_HAS_IPV4_IP)
            tip = &(newCfg->dev.ipv4);

        if (tip) {
            inet_ntop(tip->sa_family, IP_ADDR(tip), ret, IP_STRLEN(tip));
            newtEntrySet(ipcomps->ipv4Entry, ret, 1);
        }

        tip = NULL;
        if (cfg->dev.set & PUMP_INTFINFO_HAS_NETMASK)
            tip = &(cfg->dev.netmask);
        else if (newCfg->dev.set & PUMP_INTFINFO_HAS_NETMASK)
            tip = &(newCfg->dev.netmask);

        if (tip) {
            inet_ntop(tip->sa_family, IP_ADDR(tip), ret, IP_STRLEN(tip));
            newtEntrySet(ipcomps->cidr4Entry, ret, 1);
        }

        pos++;
    }

    /* IPv6 entry items */
    if (stack[IPV6]) {
        newtGridSetField(egrid, 0, pos, NEWT_GRID_COMPONENT,
                         newtLabel(-1, -1, _("IPv6 address:")),
                         0, 0, 0, 0, NEWT_ANCHOR_LEFT, 0);

        ipcomps->ipv6Entry = newtEntry(-1, -1, NULL, 41, &ipcomps->ipv6, 0);
        ipcomps->cidr6Entry = newtEntry(-1, -1, NULL, 4, &ipcomps->cidr6, 0);

        /* use a nested grid for ipv6 addr & netmask */
        rgrid = newtCreateGrid(3, 1);

        newtGridSetField(rgrid, 0, 0, NEWT_GRID_COMPONENT,
                         ipcomps->ipv6Entry, 1, 0, 0, 0, NEWT_ANCHOR_LEFT, 0);
        newtGridSetField(rgrid, 1, 0, NEWT_GRID_COMPONENT,
                         newtLabel(-1, -1, _("/")),
                         1, 0, 0, 0, NEWT_ANCHOR_LEFT, 0);
        newtGridSetField(rgrid, 2, 0, NEWT_GRID_COMPONENT,
                         ipcomps->cidr6Entry, 1, 0, 0, 0, NEWT_ANCHOR_LEFT, 0);

        newtGridSetField(egrid, 1, pos, NEWT_GRID_SUBGRID, rgrid,
                         0, 0, 0, 0, NEWT_ANCHOR_LEFT, 0);

        newtComponentAddCallback(ipcomps->ipv6Entry, ipCallback, &ipcomps);
        newtComponentAddCallback(ipcomps->cidr6Entry, cidrCallback, &ipcomps);

        /* populate fields if we have data already */
        tip = NULL;
        if (cfg->dev.set & PUMP_INTFINFO_HAS_IPV6_IP)
            tip = &(cfg->dev.ipv6);
        else if (newCfg->dev.set & PUMP_INTFINFO_HAS_IPV6_IP)
            tip = &(newCfg->dev.ipv6);

        if (tip) {
            inet_ntop(tip->sa_family, IP_ADDR(tip), ret, IP_STRLEN(tip));
            newtEntrySet(ipcomps->ipv6Entry, ret, 1);
        }

        if (cfg->dev.set & PUMP_INTFINFO_HAS_IPV6_PREFIX)
            q = asprintf(&buf, "%d", cfg->dev.ipv6_prefixlen);
        else if (newCfg->dev.set & PUMP_INTFINFO_HAS_IPV6_PREFIX)
            q = asprintf(&buf, "%d", newCfg->dev.ipv6_prefixlen);

        if (buf) {
            newtEntrySet(ipcomps->cidr6Entry, buf, 1);
            free(buf);
        }

        pos++;
    }

    /* common entry items */
    ipcomps->gwEntry = newtEntry(-1, -1, NULL, 41, &ipcomps->gw, 0);
    ipcomps->nsEntry = newtEntry(-1, -1, NULL, 41, &ipcomps->ns, 0);

    newtGridSetField(egrid, 0, pos, NEWT_GRID_COMPONENT,
                     newtLabel(-1, -1, _("Gateway:")),
                     0, 0, 0, 0, NEWT_ANCHOR_LEFT, 0);
    newtGridSetField(egrid, 1, pos, NEWT_GRID_COMPONENT,
                     ipcomps->gwEntry, 1, 0, 0, 0, NEWT_ANCHOR_LEFT, 0);

    pos++;

    newtGridSetField(egrid, 0, pos, NEWT_GRID_COMPONENT,
                     newtLabel(-1, -1, _("Name Server:")),
                     0, 0, 0, 0, NEWT_ANCHOR_LEFT, 0);
    newtGridSetField(egrid, 1, pos, NEWT_GRID_COMPONENT,
                     ipcomps->nsEntry, 1, 0, 0, 0, NEWT_ANCHOR_LEFT, 0);

    tip = NULL;
    if (cfg->dev.set & PUMP_NETINFO_HAS_GATEWAY)
        tip = &(cfg->dev.gateway);
    else if (newCfg->dev.set & PUMP_NETINFO_HAS_GATEWAY)
        tip = &(newCfg->dev.gateway);

    if (tip) {
        inet_ntop(tip->sa_family, IP_ADDR(tip), ret, IP_STRLEN(tip));
        newtEntrySet(ipcomps->gwEntry, ret, 1);
    }

    tip = NULL;
    if (cfg->dev.numDns)
        tip = &(cfg->dev.dnsServers[0]);
    else if (newCfg->dev.numDns)
        tip = &(newCfg->dev.dnsServers[0]);

    if (tip) {
        inet_ntop(tip->sa_family, IP_ADDR(tip), ret, IP_STRLEN(tip));
        newtEntrySet(ipcomps->nsEntry, ret, 1);
    }

    newtComponentAddCallback(ipcomps->gwEntry, ipCallback, &ipcomps);
    newtComponentAddCallback(ipcomps->nsEntry, ipCallback, &ipcomps);

    /* button bar at the bottom of the window */
    buttons = newtButtonBar(_("OK"), &okay, _("Back"), &back, NULL);

    /* main window layout */
    grid = newtCreateGrid(1, 3);

    buf = sdupprintf(_("Enter the IPv4 and/or the IPv6 address and prefix "
                       "(address / prefix).  For IPv4, the dotted-quad "
                       "netmask or the CIDR-style prefix are acceptable. "
                       "The gateway and name server fields must be valid IPv4 "
                       "or IPv6 addresses."));
    text = newtTextboxReflowed(-1, -1, buf, 52, 0, 10, 0);

    newtGridSetField(grid, 0, 0, NEWT_GRID_COMPONENT, text,
                     0, 0, 0, 1, NEWT_ANCHOR_LEFT, 0);
    newtGridSetField(grid, 0, 1, NEWT_GRID_SUBGRID, egrid,
                     0, 0, 0, 1, NEWT_ANCHOR_LEFT, 0);
    newtGridSetField(grid, 0, 2, NEWT_GRID_SUBGRID, buttons,
                     0, 0, 0, 0, 0, NEWT_GRID_FLAG_GROWX);

    f = newtForm(NULL, NULL, 0);
    newtGridAddComponentsToForm(grid, f, 1);
    newtGridWrappedWindow(grid, _("Manual TCP/IP Configuration"));
    newtGridFree(grid, 1);

    /* run the form */
    have[IPV4] = 0;
    have[IPV6] = 0;
    while ((have[IPV4] != 2) || (have[IPV6] != 2)) {
        for (i = 0; i < 2; i++)
            if (!stack[i]) have[i] = 2;

        answer = newtRunForm(f);
        /* memset(newCfg, 0, sizeof(*newCfg)); */

        /* collect IPv4 data */
        if (stack[IPV4]) {
            if (ipcomps->ipv4) {
                if (inet_pton(AF_INET, ipcomps->ipv4, &addr) >= 1) {
                    newCfg->dev.ipv4 = ip_addr_in(&addr);
                    newCfg->dev.set |= PUMP_INTFINFO_HAS_IPV4_IP;
                    have[IPV4]++;
                }
            }

            if (ipcomps->cidr4) {
                if (inet_pton(AF_INET, ipcomps->cidr4, &addr) >= 1) {
                    newCfg->dev.netmask = ip_addr_in(&addr);
                    newCfg->dev.set |= PUMP_INTFINFO_HAS_NETMASK;
                    have[IPV4]++;
                } else {
                    cidr = atoi(ipcomps->cidr4);
                    if (cidr >= 1 && cidr <= 32) {
                        if (inet_pton(AF_INET, "255.255.255.255", &addr) >= 1) {
                            addr.s_addr = htonl(ntohl(addr.s_addr) << (32 - cidr));
                            newCfg->dev.netmask = ip_addr_in(&addr);
                            newCfg->dev.set |= PUMP_INTFINFO_HAS_NETMASK;
                            have[IPV4]++;
                        }
                    }
                }
            }
        }

        /* collect IPv6 data */
        if (stack[IPV6]) {
            if (ipcomps->ipv6) {
                if (inet_pton(AF_INET6, ipcomps->ipv6, &addr6) >= 1) {
                    newCfg->dev.ipv6 = ip_addr_in6(&addr6);
                    newCfg->dev.set |= PUMP_INTFINFO_HAS_IPV6_IP;
                    have[IPV6]++;
                }
            }

            if (ipcomps->cidr6) {
                prefix = atoi(ipcomps->cidr6);
                if (prefix > 0 || prefix <= 128) {
                    newCfg->dev.ipv6_prefixlen = prefix;
                    newCfg->dev.set |= PUMP_INTFINFO_HAS_IPV6_PREFIX;
                    have[IPV6]++;
                }
            }
        }

        /* collect common network settings */
        if (ipcomps->gw) {
            primary = 0;

            if (inet_pton(AF_INET, ipcomps->gw, &addr) >= 1) {
                newCfg->dev.gateway = ip_addr_in(&addr);
                newCfg->dev.set |= PUMP_NETINFO_HAS_GATEWAY;
                primary = AF_INET;
            } else if (inet_pton(AF_INET6, ipcomps->gw, &addr6) >= 1) {
                newCfg->dev.gateway = ip_addr_in6(&addr6);
                newCfg->dev.set |= PUMP_NETINFO_HAS_GATEWAY;
                primary = AF_INET6;
            }

            /* We set cfg->dev.ip to the IP address of the dominant
             * network.  Determine that by the address family of the
             * gateway.
             */
            if (primary == AF_INET) {
                if (ipcomps->ipv4) {
                    if (inet_pton(AF_INET, ipcomps->ipv4, &addr) >= 1) {
                        newCfg->dev.ip = ip_addr_in(&addr);
                        newCfg->dev.set |= PUMP_INTFINFO_HAS_IP;
                    }
                }
            } else if (primary == AF_INET6) {
                if (ipcomps->ipv6) {
                    if (inet_pton(AF_INET6, ipcomps->ipv6, &addr) >= 1) {
                        newCfg->dev.ip = ip_addr_in6(&addr6);
                        newCfg->dev.set |= PUMP_INTFINFO_HAS_IP;
                    }
                }
            }
        }

        if (ipcomps->ns) {
            if (inet_pton(AF_INET, ipcomps->ns, &addr) >= 1) {
                cfg->dev.dnsServers[0] = ip_addr_in(&addr);
                cfg->dev.set |= PUMP_NETINFO_HAS_DNS;
                if (cfg->dev.numDns < 1)
                    cfg->dev.numDns = 1;
            } else if (inet_pton(AF_INET6, ipcomps->ns, &addr6) >= 1) {
                cfg->dev.dnsServers[0] = ip_addr_in6(&addr6);
                cfg->dev.set |= PUMP_NETINFO_HAS_DNS;
                if (cfg->dev.numDns < 1)
                    cfg->dev.numDns = 1;
            }
        }

        /* user selected back, but we've saved what they entered already */
        if (answer == back) {
            newtFormDestroy(f);
            newtPopWindow();
            return LOADER_BACK;
        }

        /* we might be done now */
        if (have[IPV4] != 2) {
            newtWinMessage(_("Missing Information"), _("Retry"),
                           _("You must enter both a valid IPv4 address and a "
                             "network mask or CIDR prefix."));
        }

        if (have[IPV6] != 2) {
            newtWinMessage(_("Missing Information"), _("Retry"),
                           _("You must enter both a valid IPv6 address and a "
                             "CIDR prefix."));
        }

        strcpy(newCfg->dev.device, device);
        newCfg->isDynamic = 0;
    }

    newtFormDestroy(f);
    newtPopWindow();

    return LOADER_OK;
}

void debugNetworkInfo(struct networkDeviceConfig *cfg) {
    int i;
    char *buf = NULL;

    logMessage(DEBUGLVL, "device = %s", cfg->dev.device);

    if (cfg->dev.set & PUMP_INTFINFO_HAS_IPV4_IP)
        logMessage(DEBUGLVL, "ipv4 = %s", ip_text(cfg->dev.ipv4, buf, 0));

    if (cfg->dev.set & PUMP_INTFINFO_HAS_BROADCAST)
        logMessage(DEBUGLVL,"broadcast = %s",ip_text(cfg->dev.broadcast,buf,0));

    if (cfg->dev.set & PUMP_INTFINFO_HAS_NETMASK)
        logMessage(DEBUGLVL, "netmask = %s", ip_text(cfg->dev.netmask, buf, 0));

    if (cfg->dev.set & PUMP_INTFINFO_HAS_NETWORK)
        logMessage(DEBUGLVL, "network = %s", ip_text(cfg->dev.network, buf, 0));

    if (cfg->dev.set & PUMP_INTFINFO_HAS_IPV6_IP)
        logMessage(DEBUGLVL, "ipv6 = %s", ip_text(cfg->dev.ipv6, buf, 0));

    if (cfg->dev.set & PUMP_INTFINFO_HAS_IPV6_PREFIX)
        logMessage(DEBUGLVL, "ipv6_prefixlen = %d", cfg->dev.ipv6_prefixlen);

    if (cfg->dev.set & PUMP_NETINFO_HAS_GATEWAY)
        logMessage(DEBUGLVL, "gateway = %s", ip_text(cfg->dev.gateway, buf, 0));

    if (cfg->dev.set & PUMP_NETINFO_HAS_DNS)
        for (i=0; i < cfg->dev.numDns; i++)
            logMessage(DEBUGLVL, "dns[%d] = %s", i,
                       ip_text(cfg->dev.dnsServers[i], buf, 0));
}

int setupWireless(struct networkDeviceConfig *dev) {
    /* wireless config needs to be set up before we can bring the interface
     * up */
    if (!is_wireless_interface(dev->dev.device))
        return 0;

    if (dev->essid) {
        logMessage(INFO, "setting essid for %s to %s", dev->dev.device,
                   dev->essid);
        if (set_essid(dev->dev.device, dev->essid) < 0) {
            logMessage(ERROR, "failed to set essid: %s", strerror(errno));
        }
        if (dev->wepkey) {
            logMessage(INFO, "setting encryption key for %s", dev->dev.device);
            if (set_wep_key(dev->dev.device, dev->wepkey) < 0) {
                logMessage(ERROR, "failed to set wep key: %s", strerror(errno));
        }

        }
    }

    return 0;
}

void netlogger(void *arg, int priority, char *fmt, va_list va) {
    int p;
    char *buf = NULL;

    if (priority == LOG_ERR)
        p = ERROR;
    else if (priority == LOG_INFO)
        p = INFO;
    else if (priority == LOG_DEBUG)
        p = DEBUGLVL;
    else if (priority == LOG_FATAL)
        p = CRITICAL;
    else
        p = INFO;

    if (vasprintf(&buf, fmt, va) != -1) {
        logMessage(p, "%s", buf);
        free(buf);
    } else {
        logMessage(ERROR, "unable to log network message");
    }

    return;
}

char *doDhcp(struct networkDeviceConfig *dev) {
    struct pumpNetIntf *i;
    char *r = NULL, *class = NULL;
    time_t timeout = 45;
    int loglevel;
    DHCP_Preference pref = 0;

    i = &dev->dev;

    if (dev->vendor_class != NULL)
        class = dev->vendor_class;
    else
        class = "anaconda";

    if (getLogLevel() == DEBUGLVL)
        loglevel = LOG_DEBUG;
    else
        loglevel = LOG_INFO;

	/* dhcp preferences are in /usr/include/libdhcp/dhcp_nic.h */

    /* calling function should catch ipv4Choice & ipv6Choice both being ' ' */
    if (dev->noipv4 || dev->ipv4method == IPV4_MANUAL_METHOD) {
        /* IPv4 disabled entirely -or- manual IPv4 config selected */
        pref |= DHCPv4_DISABLE;
    }

    if (!dev->noipv6 && dev->ipv6method == IPV6_AUTO_METHOD) {
        /* IPv6 enabled -and- auto neighbor discovery selected */
        pref |= DHCPv6_DISABLE_ADDRESSES;
    } else if (dev->noipv6 || dev->ipv6method == IPV6_MANUAL_METHOD) {
        /* IPv6 disabled entirely -or- manual IPv6 config selected */
        pref |= DHCPv6_DISABLE;
    }

	/* disable some things for this DHCP call */
	pref |= DHCPv6_DISABLE_RESOLVER | DHCPv4_DISABLE_HOSTNAME_SET;

    r = pumpDhcpClassRun(i,0L,class,pref,0,timeout,netlogger,loglevel);
    return r;
}

int configureNetwork(struct networkDeviceConfig * dev) {
    char *rc;

    setupWireless(dev);
    rc = pumpSetupInterface(&dev->dev);
    if (rc != NULL) {
        logMessage(INFO, "result of pumpSetupInterface is %s", rc);
        return 1;
    }

    /* we need to wait for a link after setting up the interface as some
     * switches decide to reconfigure themselves after that (#115825)
     */
    waitForLink((char *)&dev->dev.device);
    return 0;
}

int writeNetInfo(const char * fn, struct networkDeviceConfig * dev) {
    FILE * f;
    int i;
    struct device ** devices;
    char ret[48];
    ip_addr_t *tip;

    devices = probeDevices(CLASS_NETWORK, BUS_UNSPEC, PROBE_LOADED);
    if (!devices)
        return 0;

    for (i = 0; devices[i]; i++)
        if (!strcmp(devices[i]->device, dev->dev.device)) break;
    
    if (!(f = fopen(fn, "w"))) return -1;

    fprintf(f, "DEVICE=%s\n", dev->dev.device);

    fprintf(f, "ONBOOT=yes\n");

    if (dev->isDynamic) {
        fprintf(f, "BOOTPROTO=dhcp\n");
    } else {
        fprintf(f, "BOOTPROTO=static\n");

        tip = &(dev->dev.ip);
        inet_ntop(tip->sa_family, IP_ADDR(tip), ret, IP_STRLEN(tip));
        fprintf(f, "IPADDR=%s\n", ret);

        tip = &(dev->dev.netmask);
        inet_ntop(tip->sa_family, IP_ADDR(tip), ret, IP_STRLEN(tip));
        fprintf(f, "NETMASK=%s\n", ret);

        if (dev->dev.set & PUMP_NETINFO_HAS_GATEWAY) {
            tip = &(dev->dev.gateway);
            inet_ntop(tip->sa_family, IP_ADDR(tip), ret, IP_STRLEN(tip));
            fprintf(f, "GATEWAY=%s\n", ret);
        }

        if (dev->dev.set & PUMP_INTFINFO_HAS_BROADCAST) {
          tip = &(dev->dev.broadcast);
          inet_ntop(tip->sa_family, IP_ADDR(tip), ret, IP_STRLEN(tip));
          fprintf(f, "BROADCAST=%s\n", ret);
        }
    }

    if (dev->dev.set & PUMP_NETINFO_HAS_HOSTNAME)
        fprintf(f, "HOSTNAME=%s\n", dev->dev.hostname);
    if (dev->dev.set & PUMP_NETINFO_HAS_DOMAIN)
        fprintf(f, "DOMAIN=%s\n", dev->dev.domain);
    if (dev->mtu)
        fprintf(f, "MTU=%d\n", dev->mtu);
    if (dev->peerid)
        fprintf(f, "PEERID=%s\n", dev->peerid);
    if (dev->subchannels)
        fprintf(f, "SUBCHANNELS=%s\n", dev->subchannels);
    if (dev->portname)
        fprintf(f, "PORTNAME=%s\n", dev->portname);
    if (dev->nettype)
        fprintf(f, "NETTYPE=%s\n", dev->nettype);
    if (dev->ctcprot)
        fprintf(f, "CTCPROT=%s\n", dev->ctcprot);

    if (dev->essid)
        fprintf(f, "ESSID=%s\n", dev->essid);
    if (dev->wepkey)
        fprintf(f, "KEY=%s\n", dev->wepkey);
    
    fclose(f);

    return 0;
}

int writeResolvConf(struct networkDeviceConfig * net) {
    char * filename = "/etc/resolv.conf";
    FILE * f;
    int i;
    char ret[48];
    ip_addr_t *tip;
#if defined(__s390__) || defined(__s390x__)
    return 0;
#endif

    if (!(net->dev.set & PUMP_NETINFO_HAS_DOMAIN) && !net->dev.numDns)
        return LOADER_ERROR;

    f = fopen(filename, "w");
    if (!f) {
        logMessage(ERROR, "Cannot create %s: %s\n", filename, strerror(errno));
        return LOADER_ERROR;
    }

    if (net->dev.set & PUMP_NETINFO_HAS_DOMAIN)
        fprintf(f, "search %s\n", net->dev.domain);

    for (i = 0; i < net->dev.numDns; i++) {
        tip = &(net->dev.dnsServers[i]);
        inet_ntop(tip->sa_family, IP_ADDR(tip), ret, IP_STRLEN(tip));
        fprintf(f, "nameserver %s\n", ret);
    }

    fclose(f);

    res_init();         /* reinit the resolver so DNS changes take affect */

    return 0;
}

int findHostAndDomain(struct networkDeviceConfig * dev) {
    char * name, * chptr;
    char ret[48];
    ip_addr_t *tip;

    if (!FL_TESTING(flags)) {
        writeResolvConf(dev);
    }

    if (dev->dev.numDns == 0) {
        logMessage(ERROR, "no DNS servers, can't look up hostname");
        return 1;
    }

    if (!(dev->dev.set & PUMP_NETINFO_HAS_HOSTNAME)) {
        if (!FL_CMDLINE(flags))
            winStatus(50, 3, NULL, 
                      _("Determining host name and domain..."));
        else
            printf("Determining host name and domain...\n");

        tip = &(dev->dev.ip);
        inet_ntop(tip->sa_family, IP_ADDR(tip), ret, IP_STRLEN(tip));
        name = mygethostbyaddr(ret, tip->sa_family);

        if (!FL_CMDLINE(flags))
            newtPopWindow();

        if (!name) {
            logMessage(WARNING, "reverse name lookup failed");
            return 1;
        }

        logMessage(INFO, "reverse name lookup worked");

        dev->dev.hostname = strdup(name);
        dev->dev.set |= PUMP_NETINFO_HAS_HOSTNAME;
    } else {
        name = dev->dev.hostname;
    }

    if (!(dev->dev.set & PUMP_NETINFO_HAS_DOMAIN)) {
        for (chptr = name; *chptr && (*chptr != '.'); chptr++) ;
        if (*chptr == '.') {
            if (dev->dev.domain) free(dev->dev.domain);
            dev->dev.domain = strdup(chptr + 1);
            dev->dev.set |= PUMP_NETINFO_HAS_DOMAIN;
        }
    }

    return 0;
}

void setKickstartNetwork(struct loaderData_s * loaderData, int argc, 
                         char ** argv) {
    char * arg, * bootProto = NULL, * device = NULL, *ethtool = NULL, * class = NULL;
    char * essid = NULL, * wepkey = NULL, * onboot = NULL;
    int noDns = 0, noksdev = 0, rc, mtu = 0, noipv4 = 0, noipv6 = 0;
    poptContext optCon;

    struct poptOption ksOptions[] = {
        { "bootproto", '\0', POPT_ARG_STRING, &bootProto, 0, NULL, NULL },
        { "device", '\0', POPT_ARG_STRING, &device, 0, NULL, NULL },
        { "dhcpclass", '\0', POPT_ARG_STRING, &class, 0, NULL, NULL },
        { "gateway", '\0', POPT_ARG_STRING, NULL, 'g', NULL, NULL },
        { "ip", '\0', POPT_ARG_STRING, NULL, 'i', NULL, NULL },
        { "mtu", '\0', POPT_ARG_INT, &mtu, 0, NULL, NULL },
        { "nameserver", '\0', POPT_ARG_STRING, NULL, 'n', NULL, NULL },
        { "netmask", '\0', POPT_ARG_STRING, NULL, 'm', NULL, NULL },
        { "noipv4", '\0', POPT_ARG_NONE, &noipv4, 0, NULL, NULL },
        { "noipv6", '\0', POPT_ARG_NONE, &noipv6, 0, NULL, NULL },
        { "nodns", '\0', POPT_ARG_NONE, &noDns, 0, NULL, NULL },
        { "hostname", '\0', POPT_ARG_STRING, NULL, 'h', NULL, NULL},
        { "ethtool", '\0', POPT_ARG_STRING, &ethtool, 0, NULL, NULL },
        { "essid", '\0', POPT_ARG_STRING, &essid, 0, NULL, NULL },
        { "wepkey", '\0', POPT_ARG_STRING, &wepkey, 0, NULL, NULL },
        { "onboot", '\0', POPT_ARG_STRING, &onboot, 0, NULL, NULL },
        { "notksdevice", '\0', POPT_ARG_NONE, &noksdev, 0, NULL, NULL },
        { 0, 0, 0, 0, 0, 0, 0 }
    };
    
    optCon = poptGetContext(NULL, argc, (const char **) argv, 
                            ksOptions, 0);    
    while ((rc = poptGetNextOpt(optCon)) >= 0) {
        arg = (char *) poptGetOptArg(optCon);

        switch (rc) {
        case 'g':
            loaderData->gateway = strdup(arg);
            break;
        case 'i':
            loaderData->ip = strdup(arg);
            break;
        case 'n':
            loaderData->dns = strdup(arg);
            break;
        case 'm':
            loaderData->netmask = strdup(arg);
            break;
        case 'h':
            if (loaderData->hostname) 
                free(loaderData->hostname);
            loaderData->hostname = strdup(arg);
            break;
        }
    }
    
    if (rc < -1) {
        newtWinMessage(_("Kickstart Error"), _("OK"),
                       _("Bad argument to kickstart network command %s: %s"),
                       poptBadOption(optCon, POPT_BADOPTION_NOALIAS), 
                       poptStrerror(rc));
    } else {
        poptFreeContext(optCon);
    }

    /* if they've specified dhcp/bootp or haven't specified anything, 
     * use dhcp for the interface */
    if ((bootProto && (!strncmp(bootProto, "dhcp", 4) || 
                       !strncmp(bootProto, "bootp", 4))) ||
        (!bootProto && !loaderData->ip)) {
        loaderData->ip = strdup("dhcp");
        loaderData->ipinfo_set = 1;
    } else if (loaderData->ip) {
        /* JKFIXME: this assumes a bit... */
        loaderData->ipinfo_set = 1;
    }

    /* now make sure the specified bootproto is valid */
    if (bootProto && strcmp(bootProto, "dhcp") && strcmp(bootProto, "bootp") &&
        strcmp(bootProto, "static") && strcmp(bootProto, "query")) {
        newtWinMessage(_("Kickstart Error"), _("OK"),
                       _("Bad bootproto %s specified in network command"),
                       bootProto);
    } 

    if (!noksdev) {
        if (device) {
            loaderData->netDev = strdup(device);
            loaderData->netDev_set = 1;
        }

        if (class) {
            loaderData->netCls = strdup(class);
            loaderData->netCls_set = 1;
        }

        if (ethtool) {
            if (loaderData->ethtool)
                free(loaderData->ethtool);
            loaderData->ethtool = strdup(ethtool);
            free(ethtool);
        }

        if (essid) {
            if (loaderData->essid)
                free(loaderData->essid);
            loaderData->essid = strdup(essid);
            free(essid);
        }

        if (wepkey) {
            if (loaderData->wepkey)
                free(loaderData->wepkey);
            loaderData->wepkey = strdup(wepkey);
            free(wepkey);
        }

        if (mtu) {
           loaderData->mtu = mtu;
        }

        if (noipv4)
            loaderData->noipv4 = 1;

        if (noipv6)
            loaderData->noipv6 = 1;
    }

    if (noDns) {
        loaderData->noDns = 1;
    }
}

/* if multiple interfaces get one to use from user.   */
/* NOTE - uses kickstart data available in loaderData */
int chooseNetworkInterface(struct loaderData_s * loaderData) {
    int i, rc;
    unsigned int max = 40;
    int deviceNums = 0;
    int deviceNum;
    char ** devices;
    char ** deviceNames;
    int foundDev = 0;
    struct device ** devs;
    char * ksMacAddr = NULL;
#ifdef SDSC
    char * mac;
#endif

    devs = probeDevices(CLASS_NETWORK, BUS_UNSPEC, PROBE_LOADED);
    if (!devs) {
        logMessage(ERROR, "no network devices in choose network device!");
        return LOADER_ERROR;
    }

    for (i = 0; devs[i]; i++);

    devices = alloca((i + 1) * sizeof(*devices));
    deviceNames = alloca((i + 1) * sizeof(*devices));
    if (loaderData->netDev && (loaderData->netDev_set) == 1) {
        if ((loaderData->bootIf && (loaderData->bootIf_set) == 1) && !strcasecmp(loaderData->netDev, "bootif")) {
            ksMacAddr = strdup(loaderData->bootIf);
        } else {
            ksMacAddr = strdup(loaderData->netDev);
        }

        ksMacAddr = str2upper(ksMacAddr);
    }

    for (i = 0; devs[i]; i++) {
        if (!devs[i]->device)
            continue;
        if (devs[i]->desc) {
                deviceNames[deviceNums] = alloca(strlen(devs[i]->device) +
                                          strlen(devs[i]->desc) + 4);
                sprintf(deviceNames[deviceNums],"%s - %s",
                        devs[i]->device, devs[i]->desc);
                if (strlen(deviceNames[deviceNums]) > max)
                        max = strlen(deviceNames[deviceNums]);
                devices[deviceNums++] = devs[i]->device;
        } else {
            devices[deviceNums] = devs[i]->device;
            deviceNames[deviceNums++] = devs[i]->device;
        }

        /* this device has been set and we don't really need to ask 
         * about it again... */
        if (loaderData->netDev && (loaderData->netDev_set == 1)) {
            if (!strcmp(loaderData->netDev, devs[i]->device)) {
                foundDev = 1;
            } else if (ksMacAddr != NULL) {
                /* maybe it's a mac address */
                char *devmacaddr = NULL;
                devmacaddr = netlink_interfaces_mac2str(devs[i]->device);
                if ((devmacaddr != NULL) && !strcmp(ksMacAddr, devmacaddr)) {
                    foundDev = 1;
                    free(loaderData->netDev);
                    loaderData->netDev = devs[i]->device;
                    if (devmacaddr != NULL)
                        free(devmacaddr);
                    break;
                }

                if (devmacaddr != NULL)
                    free(devmacaddr);
            }
        }
    }

#ifdef SDSC
    /*
     * If you haven't chosen a ksdevice but did give the mac address
     * of the device you want to use, search for it here.
     */
    if (!loaderData->netDev && loaderData->mac) {
            mac = netlink_interfaces_mac2str(devs[i]->device);
            if (mac && !strcasecmp(mac, loaderData->mac)) {
                    logMessage(INFO,
			    "chooseNetworkInterface: matched MAC %s, using %s",
			    mac, devs[i]->device);

                    foundDev = 1;
                    free(loaderData->netDev);
                    loaderData->netDev = devs[i]->device;
                    loaderData->netDev_set = 1;
            }
    }
#endif

    if (ksMacAddr)
        free(ksMacAddr);
    if (foundDev == 1)
        return LOADER_NOOP;

    devices[deviceNums] = NULL;
    deviceNames[deviceNums] = NULL;
    qsort(devices, deviceNums, sizeof(*devices), simpleStringCmp);
    qsort(deviceNames, deviceNums, sizeof(*devices), simpleStringCmp);

    /* ASSERT: we should *ALWAYS* have a network device when we get here */
    if (!deviceNums) {
        logMessage(CRITICAL, "no network device in chooseNetworkInterface");
        return LOADER_ERROR;
    }

    /* JKFIXME: if we only have one interface and it doesn't have link,
     * do we go ahead? */
    if (deviceNums == 1) {
        logMessage(INFO, "only have one network device: %s", devices[0]);
        loaderData->netDev = devices[0];
        return LOADER_NOOP;
    }

    if ((loaderData->netDev && (loaderData->netDev_set == 1)) &&
        !strcmp(loaderData->netDev, "link")) {
        logMessage(INFO, "looking for first netDev with link");
        for (rc = 0; rc < 5; rc++) {
            for (i = 0; i < deviceNums; i++) {
                if (get_link_status(devices[i]) == 1) {
                    loaderData->netDev = devices[i];
                    logMessage(INFO, "%s has link, using it", devices[i]);
                    return LOADER_NOOP;
                }
            }
            sleep(1);
        }
        logMessage(WARNING, "wanted netdev with link, but none present.  prompting");
    }

    startNewt();

    if (max > 70)
        max = 70;

    /* JKFIXME: should display link status */
    deviceNum = 0;
    rc = newtWinMenu(_("Networking Device"), 
		     _("You have multiple network devices on this system. "
		       "Which would you like to install through?"), max, 10, 10,
		     deviceNums < 6 ? deviceNums : 6, deviceNames,
		     &deviceNum, _("OK"), _("Back"), NULL);
    if (rc == 2)
        return LOADER_BACK;

    loaderData->netDev = devices[deviceNum];

    /* turn off the non-active interface.  this should keep things from
     * breaking when we need the interface to do the install as long as
     * you keep using that device */
    for (i = 0; devs[i]; i++) {
        if (strcmp(loaderData->netDev, devices[i]))
            if (!FL_TESTING(flags))
                pumpDisableInterface(devs[i]->device);
    }

    return LOADER_OK;
}

#ifdef	SDSC

/* This function works like kickstartNetworkUp, but is non-interactive (does
 * not call chooseNetworkInterface) and tries every network interface for a
 * DHCP response.
 */
int
rocksNetworkUp(struct loaderData_s * loaderData,
	struct networkDeviceConfig *netCfgPtr)
{
	struct device	**devs;
	int		i;
	int		rc;
	int		tries;
	int		numdevices;
	int		found = 0;

	initLoopback();

	memset(netCfgPtr, 0, sizeof(*netCfgPtr));
	netCfgPtr->isDynamic = 1;

	/* we don't want to end up asking about interface more than once
	 * if we're in a kickstart-ish case (#100724) */
	loaderData->netDev_set = 1;

	/* JKFIXME: this is kind of crufty, we depend on the fact that the
	 * ip is set and then just get the network up.  we should probably
	 * add a way to do asking about static here and not be such a hack */
	if (!loaderData->ip) {
		loaderData->ip = strdup("dhcp");
	} 
	loaderData->ipinfo_set = 1;

	devs = probeDevices(CLASS_NETWORK, BUS_UNSPEC, PROBE_LOADED);
	if (!devs) {
		logMessage(ERROR,
			"%s:no network devices in choose network device!",
			"ROCKS:rocksNetworkUp");
		return LOADER_ERROR;
	}

	/*
	 * probeDevices() gives us the list in reverse order (that is, eth1
	 * first, then eth0). so count the devices first, then use the
	 * value to look at the devices in the 'correct' order.
	 */
	numdevices = 0;
	for (i = 0; devs[i]; i++) {
		++numdevices;
	}

	/*
	 * try sending a DHCP on all the network devices, the first one
	 * get an answer on, we'll use.
	 */
	for (tries = 0; !found && tries < 3; tries++) {
		for (i = 0; i < numdevices; ++i) {
			if (!devs[i]->device) {
				continue;
			}

			loaderData->netDev = strdup(devs[i]->device);
			strcpy(netCfgPtr->dev.device, loaderData->netDev);

			logMessage(INFO, "%s: looking at device (%s)", 
				"ROCKS:rocksNetworkUp", loaderData->netDev);

			setupNetworkDeviceConfig(netCfgPtr, loaderData);

			if (netCfgPtr->preset != 1) {
				logMessage(INFO,
					"%s:no DHCP response on device (%s)",
					"ROCKS:rocksNetworkUp",
					loaderData->netDev);
				continue;
			}

			rc = readNetConfig(loaderData->netDev, netCfgPtr,
				loaderData->netCls, loaderData->method);

			/*
			 * if there is no bootfile provided from the DHCP
			 * response, then this is definitely not a frontend,
			 * so continue and go to the next device
			 */
			if (!(netCfgPtr->dev.set & PUMP_INTFINFO_HAS_BOOTFILE)){
				continue;
			}

			if ((rc == LOADER_OK) || (rc == LOADER_NOOP)) {
				found = 1;
				break;
			}
		}
	}

	if (found == 0) {
		logMessage(ERROR,
			"kickstartNetworkUp:unable to setup networking");
		return -1;
	}

	return 0;
}

#endif

/* JKFIXME: bad name.  this function brings up networking early on a 
 * kickstart install so that we can do things like grab the ks.cfg from
 * the network */
int kickstartNetworkUp(struct loaderData_s * loaderData,
                       struct networkDeviceConfig *netCfgPtr) {
    int rc;

    initLoopback();

    memset(netCfgPtr, 0, sizeof(*netCfgPtr));

    do {
        /* this is smart and does the right thing based on whether or not
         * we have ksdevice= specified */
        rc = chooseNetworkInterface(loaderData);
        
        if (rc == LOADER_ERROR) {
            /* JKFIXME: ask for a driver disk? */
            logMessage(ERROR, "no network drivers for doing kickstart");
            return -1;
        } else if (rc == LOADER_BACK) {
            return -1;
        }

        /* insert device into pump structure */
        strcpy(netCfgPtr->dev.device, loaderData->netDev);

        break;
    } while (1);

    /* we don't want to end up asking about interface more than once
     * if we're in a kickstart-ish case (#100724) */
    loaderData->netDev_set = 1;

    /* JKFIXME: this is kind of crufty, we depend on the fact that the
     * ip is set and then just get the network up.  we should probably
     * add a way to do asking about static here and not be such a hack */
    if (!loaderData->ip) {
        loaderData->ip = strdup("dhcp");
    } 
    loaderData->ipinfo_set = 1;

    setupNetworkDeviceConfig(netCfgPtr, loaderData);

    rc = readNetConfig(loaderData->netDev, netCfgPtr, loaderData->netCls,
                       loaderData->method);
    if ((rc == LOADER_BACK) || (rc == LOADER_ERROR)) {
        logMessage(ERROR, "unable to setup networking");
        return -1;
    }

#ifdef	SDSC
{
	/*
	 * write the networking info to a file that will be used by
	 * later stages of the installer
	 */
	FILE		*f;
	int		i;
	char		ret[48];
	ip_addr_t	*tip;

	if ((f = fopen("/tmp/netinfo-rocks", "w")) == NULL) {
		logMessage(ERROR,
			"ROCKS:unable to open file /tmp/netinfo-rocks");
	} else {
		if (netCfgPtr->dev.set & PUMP_INTFINFO_HAS_IP) {
			fprintf(f, "<var name=\"Kickstart_PublicAddress\" ");

			tip = &(netCfgPtr->dev.ip);
			inet_ntop(tip->sa_family, IP_ADDR(tip), ret,
				IP_STRLEN(tip));

			fprintf(f, "val=\"%s\"/>\n", ret);
		}

		if (netCfgPtr->dev.set & PUMP_INTFINFO_HAS_NETMASK) {
			fprintf(f, "<var name=\"Kickstart_PublicNetmask\" ");

			tip = &(netCfgPtr->dev.netmask);
			inet_ntop(tip->sa_family, IP_ADDR(tip), ret,
				IP_STRLEN(tip));

			fprintf(f, "val=\"%s\"/>\n", ret);
		}

		if (netCfgPtr->dev.set & PUMP_NETINFO_HAS_GATEWAY) {
			fprintf(f, "<var name=\"Kickstart_PublicGateway\" ");

			tip = &(netCfgPtr->dev.gateway);
			inet_ntop(tip->sa_family, IP_ADDR(tip), ret,
				IP_STRLEN(tip));

			fprintf(f, "val=\"%s\"/>\n", ret);
		}

		if (netCfgPtr->dev.numDns > 0) {
			fprintf(f, "<var name=\"Kickstart_PublicDNSServers\" ");
			fprintf(f, "val=\"");
		}
		for (i = 0; i < netCfgPtr->dev.numDns; i++) {
			tip = &(netCfgPtr->dev.dnsServers[i]);
			inet_ntop(tip->sa_family, IP_ADDR(tip), ret,
				IP_STRLEN(tip));

			fprintf(f, "%s", ret);

			/*
			 * if this isn't the last entry and if there are
			 * more than one entry, then write a comma
			 */
			if ((i + 1) < netCfgPtr->dev.numDns) {
				fprintf(f, ",");
			}
		}
		if (netCfgPtr->dev.numDns > 0) {
			fprintf(f, "\"/>\n");
		}

		fclose(f);
	}
}
#endif

    return 0;
}

static int strcount (char *str, int ch)
{
    int retval = 0;
    char *tmp = str;

    do {
        if ((tmp = index(tmp, ch)) != NULL) {
            tmp++;
            retval++;
        }
    } while (tmp != NULL);

    return retval;
}

void splitHostname (char *str, char **host, char **port)
{
    char *rightbrack = strchr(str, ']');

    *host = NULL;
    *port = NULL;

    if (*str == '[' && rightbrack) {
        /* An IPv6 address surrounded by brackets, optionally with a colon and
         * port number.
         */
        char *colon = strrchr(rightbrack, ':');

        if (colon) {
            *host = strndup(str+1, rightbrack-1-str);
            *port = strdup(colon+1);
        }
        else
            *host = strndup(str+1, rightbrack-1-str);
    } else if (strcount(str, ':') > 1) {
        /* An IPv6 address without brackets.  Don't make the user surround the
         * address with brackets if there's no port number.
         */
        *host = strdup(str);
    } else {
        /* An IPv4 address, optionally with a colon and port number. */
        char *colon = strrchr(str, ':');

        if (colon) {
            *host = strndup(str, colon-str);
            *port = strdup(colon+1);
        }
        else
            *host = strdup(str);
    }
}

/* vim:set shiftwidth=4 softtabstop=4: */
