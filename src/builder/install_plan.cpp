#include "rb4pc/builder/install_plan.hpp"
#include <sstream>
#include <stdexcept>
#include <utility>

namespace rb4pc::builder {
InstallPlan create_install_plan(const CustomSongCatalog& catalog, const std::filesystem::path& target) {
    if (target.empty()) throw std::runtime_error("install-plan target is empty");
    InstallPlan plan; plan.target = target;
    for (const auto& song : catalog.songs) {
        if (!song.ready) continue;
        for (const auto& source : {song.header, song.archive}) {
            InstallOperation operation{source, target / source.filename(), std::filesystem::file_size(source)};
            operation.collision = std::filesystem::exists(operation.destination);
            if (operation.collision) ++plan.collisions;
            plan.total_bytes += operation.size; plan.operations.push_back(std::move(operation));
        }
    }
    return plan;
}
std::string format_json(const InstallPlan& plan) {
    std::ostringstream out; out << "{\n  \"schemaVersion\": 1,\n  \"target\": \"" << plan.target.string()
        << "\",\n  \"operationCount\": " << plan.operations.size() << ",\n  \"collisionCount\": " << plan.collisions
        << ",\n  \"totalBytes\": " << plan.total_bytes << ",\n  \"operations\": [";
    for (std::size_t i = 0; i < plan.operations.size(); ++i) { const auto& op = plan.operations[i];
        out << (i ? ",\n" : "\n") << "    {\"source\": \"" << op.source.string() << "\", \"destination\": \""
            << op.destination.string() << "\", \"size\": " << op.size << ", \"collision\": " << (op.collision ? "true" : "false") << '}'; }
    if (!plan.operations.empty()) out << '\n';
    out << "  ]\n}\n";
    return out.str();
}
} // namespace rb4pc::builder
