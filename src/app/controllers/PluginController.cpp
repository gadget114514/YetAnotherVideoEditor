#include "PluginController.h"

#include "../../plugin/PluginManager.h"

namespace yave {

PluginController::PluginController(QObject* parent) : QObject(parent)
{
    auto& pm = plugin::PluginManager::instance();
    connect(&pm, &plugin::PluginManager::scanStarted,
            this, &PluginController::scanStarted);
    connect(&pm, &plugin::PluginManager::scanFinished,
            this, &PluginController::scanFinished);
}

void PluginController::rescan()
{
    plugin::PluginManager::instance().scanAsync();
}

bool PluginController::isAviUtlSupported() const
{
    return plugin::PluginManager::isAviUtlSupported();
}

} // namespace yave
