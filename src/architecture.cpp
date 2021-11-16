#include "architecture.hpp"

#include <boost/lexical_cast.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <fstream>
#include <iostream>

#include "cognitive_function.hpp"
#include "json/json.h"

using namespace std;

Architecture::Architecture(boost::uuids::uuid uuid)
    : uuid(uuid),
      name("NoName Architecture"),
      version("0.0.1"),
      description(
          "The NoName architecture is based on [this "
          "article](http://example.org) with the following modifications...") {}

Architecture::Architecture()
    : Architecture(boost::uuids::random_generator()()) {}

NodePtr Architecture::createNode() {
    cout << name << ": creating node" << endl;
    auto node = std::make_shared<Node>();
    _nodes.insert(node);

    return node;
}

NodePtr Architecture::createNode(const boost::uuids::uuid& uuid) {
    cout << name << ": creating node (with UUID)" << endl;
    auto node = std::make_shared<Node>(uuid);
    _nodes.insert(node);

    return node;
}

void Architecture::addNode(NodePtr node) {
    cout << name << ": adding node " << node->name() << endl;

    _nodes.insert(node);
}

void Architecture::removeNode(NodePtr node) {
    // first, delete all connections involving this node
    set<ConnectionPtr> to_remove;
    for (auto c : _connections) {
        if (c->from.node.lock() == node || c->to.node.lock() == node) {
            to_remove.insert(c);
        }
    }
    for (auto c : to_remove) _connections.erase(c);

    _nodes.erase(node);
}

NodePtr Architecture::node(const boost::uuids::uuid& uuid) {
    for (auto n : _nodes) {
        if (n->uuid == uuid) return n;
    }
    return nullptr;
}

ConnectionPtr Architecture::createConnection(Socket from, Socket to) {
    auto connection = std::make_shared<Connection>();
    connection->from = from;
    connection->to = to;

    for (auto c : _connections) {
        if ((*c) == (*connection)) {
            return c;
        }
    }

    _connections.insert(connection);
    return connection;
}

ConnectionPtr Architecture::createConnection(const boost::uuids::uuid& uuid,
                                             Socket from, Socket to) {
    auto connection = std::make_shared<Connection>(uuid);
    connection->from = from;
    connection->to = to;

    for (auto c : _connections) {
        if ((*c) == (*connection)) {
            return c;
        }
    }

    _connections.insert(connection);
    return connection;
}

void Architecture::removeConnection(Socket from, Socket to) {
    for (auto c : _connections) {
        if (c->to == to && c->from == from) {
            _connections.erase(c);
            break;
        }
    }
}

Architecture::ToAddToRemove Architecture::load(const Json::Value& json,
                                               bool clearFirst,
                                               bool recreateUUIDs,
                                               bool metadata) {
    set<NodePtr> newnodes;
    set<ConnectionPtr> newconnections;

    set<NodePtr> killednodes;
    set<ConnectionPtr> killedconnections;

    DEBUG("Reading the architecture..." << endl);

    if (metadata) {
        name = json.get("name", "<no name>").asString();
        version = json.get("version", "0.0.1").asString();
        description =
            json.get("description", "(no description yet)").asString();

        if (!recreateUUIDs) {
            uuid = get_uuid(json["uuid"].asString(), "Architecture");
        }
    }

    set<boost::uuids::uuid> existing_uuids;

    if (clearFirst) {
        killednodes = _nodes;
        killedconnections = _connections;
        _nodes.clear();
        _connections.clear();
    }

    for (auto n : _nodes) {
        existing_uuids.insert(n->uuid);
    }
    for (auto c : _connections) {
        existing_uuids.insert(c->uuid);
    }

    //////////////////////////////////////////
    /////   NODES
    //////////////////////////////////////////
    for (auto n : json["nodes"]) {
        auto uuid = get_uuid(n["uuid"].asString(), "Node");

        NodePtr node;

        if (!recreateUUIDs && existing_uuids.count(uuid)) {
            cerr << "Already existing UUID <" << uuid << ">! ";
            cerr << "Skipping this node." << endl;
            continue;
        }

        if (!recreateUUIDs) {
            node = createNode(uuid);
        } else {
            node = createNode();
        }

        DEBUG("Adding node <" << n["name"].asString() << ">" << endl);

        node->cognitive_function(get_cognitive_function_by_name(
            n.get("cognitive_function", "").asString()));

        if (n.isMember("position")) {
            node->x(n["position"][0].asDouble());
            node->y(n["position"][1].asDouble());
        }

        if (n.isMember("size")) {
            node->width(n["size"][0].asDouble());
            node->height(n["size"][1].asDouble());
        }

        for (auto p : n["ports"]) {
            Port::Type type;
            if (p["type"].asString() == "latent")
                type = Port::Type::LATENT;
            else if (p["type"].asString() == "explicit")
                type = Port::Type::EXPLICIT;
            else
                type = Port::Type::OTHER;

            node->createPort({p["name"].asString(),
                              p["direction"].asString() == "in"
                                  ? Port::Direction::IN
                                  : Port::Direction::OUT,
                              type});
        }
        node->name(n["name"].asString());
        newnodes.insert(node);
    }

    //////////////////////////////////////////
    /////   CONNECTIONS
    //////////////////////////////////////////
    for (auto c : json["connections"]) {
        auto uuid = get_uuid(c["uuid"].asString(), "Connection");

        if (!recreateUUIDs && existing_uuids.count(uuid)) {
            cerr << "Already existing UUID <" << uuid << ">! ";
            cerr << "Skipping this connection." << endl;
            continue;
        }

        auto from_uuid_str = c["from"].asString().substr(0, 36);
        auto from_uuid = get_uuid(from_uuid_str, "Connection 'from'");
        auto from = node(from_uuid);
        auto from_port = c["from"].asString().substr(37);

        auto to_uuid_str = c["to"].asString().substr(0, 36);
        auto to_uuid = get_uuid(to_uuid_str, "Connection 'to'");
        auto to = node(to_uuid);
        auto to_port = c["to"].asString().substr(37);

        DEBUG("Creating connection between: "
              << from->name() << ":" << from_port << " and " << to->name()
              << ":" << to_port << endl);

        ConnectionPtr connection;

        if (!recreateUUIDs) {
            connection = createConnection(uuid, {from, from->port(from_port)},
                                          {to, to->port(to_port)});
        } else {
            connection = createConnection({from, from->port(from_port)},
                                          {to, to->port(to_port)});
        }

        connection->name = c.get("name", Connection::ANONYMOUS).asString();

        newconnections.insert(connection);
    }

    return {{newnodes, newconnections}, {killednodes, killedconnections}};
}

Architecture::ToAddToRemove Architecture::load(const std::string& filename) {
    Json::Value root;
    ifstream json_file(filename);

    json_file >> root;

    // 'Dummy' load to make sure the JSON is valid, without impacting the
    // current arch
    // -> prevent going in a 'semi-loaded' state
    Architecture new_arch;
    new_arch.load(root);

    // if we reach this point, no exception was raised while loading the arch
    // from the JSON file. We can load it into ourselves.

    this->filename = filename;
    return load(root);
}

boost::uuids::uuid Architecture::get_uuid(const std::string& uuid,
                                          const string& ctxt) {
    if (uuid.empty()) {
        throw runtime_error((ctxt.empty() ? ctxt : ctxt + ": ") + "No UUID!");
    }

    try {
        return boost::lexical_cast<boost::uuids::uuid>(uuid);
    } catch (const boost::bad_lexical_cast& e) {
        throw runtime_error((ctxt.empty() ? ctxt : ctxt + ": ") +
                            "Invalid UUID: " + uuid);
    }
}
