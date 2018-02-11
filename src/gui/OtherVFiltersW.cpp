#include <OtherVFiltersW.hpp>
#include <Settings.hpp>
#include <Module.hpp>
#include <Main.hpp>

OtherVFiltersW::OtherVFiltersW()
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setSelectionMode(QAbstractItemView::ExtendedSelection);
	setDragDropMode(QAbstractItemView::InternalMove);

	QPair< QStringList, QList< bool > > videoFilters;
	foreach (const QString &filter, QMPlay2Core.getSettings().get("VideoFilters").toStringList())
	{
		videoFilters.first += filter.mid(1);
		videoFilters.second += filter.left(1).toInt();
	}

	QList< QPair< Module *, Module::Info > > pluginsInstances;
	for (int i = 0; i < videoFilters.first.count(); ++i)
		pluginsInstances += QPair< Module *, Module::Info >();

	foreach (Module *pluginInstance, QMPlay2Core.getPluginsInstance())
		foreach (Module::Info moduleInfo, pluginInstance->getModulesInfo())
			if ((moduleInfo.type & 0xF) == Module::VIDEOFILTER && !(moduleInfo.type & Module::DEINTERLACE))
			{
				const int idx = videoFilters.first.indexOf(moduleInfo.name);
				if (idx > -1)
				{
					if (videoFilters.second[idx])
						moduleInfo.type |= Module::USERFLAG;
					pluginsInstances[idx] = qMakePair(pluginInstance, moduleInfo);
				}
				else
					pluginsInstances += qMakePair(pluginInstance, moduleInfo);
			}

	for (int i = 0; i < pluginsInstances.count(); i++)
	{
		Module *module = pluginsInstances[i].first;
		Module::Info &moduleInfo = pluginsInstances[i].second;
		if (!module || moduleInfo.name.isEmpty())
			continue;
		QListWidgetItem *item = new QListWidgetItem(this);
		item->setData(Qt::UserRole, module->name());
		item->setData(Qt::ToolTipRole, moduleInfo.description);
		item->setIcon(QMPlay2GUI.getIcon(moduleInfo.img.isNull() ? module->image() : moduleInfo.img));
		item->setCheckState(moduleInfo.type & Module::USERFLAG ? Qt::Checked : Qt::Unchecked);
		item->setText(moduleInfo.name);
	}
}

void OtherVFiltersW::writeSettings()
{
	QStringList filters;
	for (int i = 0; i < count(); ++i)
		filters += (item(i)->checkState() == Qt::Checked ? '1' : '0') + item(i)->text();
	QMPlay2Core.getSettings().set("VideoFilters", filters);
}
