#ifndef RSTVISITOR_HPP
#define RSTVISITOR_HPP

#include <filesystem>
#include <inja/inja.hpp>
#include <memory>
#include <sstream>
#include <string>

#include "architecture.hpp"  // Node
#include "label.hpp"
#include "node.hpp"
#include "visitor.hpp"

class RstVisitor : public Visitor {
   public:
    RstVisitor(const Architecture& architecture, const std::string& ws_path);

   private:
    void startUp() override;
    void beginNodes() override;
    void onNode(std::shared_ptr<const Node>) override;
    void endNodes() override;
    void beginConnections() override;
    void onConnection(std::shared_ptr<const Connection>) override;
    void tearDown() override;

   private:
    std::vector<ConstNodePtr> nodes_;

    std::filesystem::path ws_path;

    std::unique_ptr<inja::Environment> env_;
    nlohmann::json data_;
};

#endif
