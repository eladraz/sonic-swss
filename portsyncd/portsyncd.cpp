#include "common/dbconnector.h"
#include "common/select.h"
#include "common/netdispatcher.h"
#include "common/netlink.h"
#include "common/producertable.h"
#include "portsyncd/linksync.h"

#include <getopt.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <set>
#include <map>

#define DEFAULT_PORT_CONFIG_FILE "port_config.ini"

using namespace std;
using namespace swss;

void usage(char **argv)
{
    cout << "Usage: " << argv[0] << " [-f config_file]" << endl;
}

int main(int argc, char **argv)
{
    int opt;
    string port_config_file = DEFAULT_PORT_CONFIG_FILE;

    while ((opt = getopt(argc, argv, "f:")) != -1 )
    {
        switch (opt)
        {
        case 'f':
            port_config_file.assign(optarg);
            break;
        case 'h':
            usage(argv);
            return 1;
        default: /* '?' */
            usage(argv);
            return EXIT_FAILURE;
        }
    }

    DBConnector db(0, "localhost", 6379, 0);
    ProducerTable p(&db, APP_PORT_TABLE_NAME);

    ifstream infile(port_config_file);
    if (!infile.is_open())
    {
        cout << "Port configuration file not found! " << port_config_file << endl;
        usage(argv);
        return EXIT_FAILURE;
    }

    string line;
    while (getline(infile, line))
    {
        if (line.at(0) == '#')
        {
            continue;
        }

        istringstream iss(line);
        string alias, lanes;
        iss >> alias >> lanes;

        FieldValueTuple lanes_attr("lanes", lanes);
        vector<FieldValueTuple> attrs = { lanes_attr };
        p.set(alias, attrs);
    }

    LinkSync sync(&db);
    NetDispatcher::getInstance().registerMessageHandler(RTM_NEWLINK, &sync);
    NetDispatcher::getInstance().registerMessageHandler(RTM_DELLINK, &sync);

    while (true)
    {
        try
        {
            NetLink netlink;
            Select s;

            netlink.registerGroup(RTNLGRP_LINK);
            cout << "Listens to link messages..." << endl;
            netlink.dumpRequest(RTM_GETLINK);

            s.addSelectable(&netlink);
            while (true)
            {
                Selectable *temps;
                int tempfd;
                s.select(&temps, &tempfd);
            }
        }
        catch (...)
        {
            cout << "Exception had been thrown in deamon" << endl;
            return EXIT_FAILURE;
        }
    }

    return 1;
}
