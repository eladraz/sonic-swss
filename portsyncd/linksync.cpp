#include <string.h>
#include <errno.h>
#include <system_error>
#include <sys/socket.h>
#include <linux/if.h>
#include <netlink/route/link.h>
#include "common/logger.h"
#include "common/netmsg.h"
#include "common/dbconnector.h"
#include "common/producertable.h"
#include "common/scheme.h"
#include "common/linkcache.h"
#include "portsyncd/linksync.h"

#include <iostream>

using namespace std;
using namespace swss;

LinkSync::LinkSync(DBConnector *db) :
    m_portTableProducer(db, APP_PORT_TABLE_NAME),
    m_vlanTableProducer(db, APP_VLAN_TABLE_NAME),
    m_lagTableProducer(db, APP_LAG_TABLE_NAME),
    m_portTableConsumer(db, APP_PORT_TABLE_NAME),
    m_vlanTableConsumer(db, APP_VLAN_TABLE_NAME),
    m_lagTableConsumer(db, APP_LAG_TABLE_NAME)
{
}

void LinkSync::onMsg(int nlmsg_type, struct nl_object *obj)
{
    std::vector<FieldValueTuple> temp;
    struct rtnl_link *link = (struct rtnl_link *)obj;

    if ((nlmsg_type != RTM_NEWLINK) && (nlmsg_type != RTM_GETLINK) &&
        (nlmsg_type != RTM_DELLINK))
        return;

    string key = rtnl_link_get_name(link);
    if (nlmsg_type == RTM_DELLINK) /* Will be sync by other application */
        return;

    bool admin_state = rtnl_link_get_flags(link) & IFF_UP;
    bool oper_state = rtnl_link_get_flags(link) & IFF_LOWER_UP;
    unsigned int mtu = rtnl_link_get_mtu(link);

    std::vector<FieldValueTuple> fvVector;
    FieldValueTuple a("admin_status", admin_state ? "up" : "down");
    FieldValueTuple o("oper_status", oper_state ? "up" : "down");
    FieldValueTuple m("mtu", to_string(mtu));
    fvVector.push_back(a);
    fvVector.push_back(o);
    fvVector.push_back(m);

    if (m_lagTableConsumer.get(key, temp))
        m_lagTableProducer.set(key, fvVector);
    else if (m_vlanTableConsumer.get(key, temp))
        m_vlanTableProducer.set(key, fvVector);
    else if (m_portTableConsumer.get(key, temp))
        m_portTableProducer.set(key, fvVector);
    /* else discard managment or untracked netdev state */
}
