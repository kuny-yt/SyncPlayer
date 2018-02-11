#include <Settings.hpp>

Settings::Settings(const QString &name) :
	QSettings(QMPlay2Core.getSettingsDir() + name + ".ini", QSettings::IniFormat)
{}
Settings::~Settings()
{
	QMutexLocker mL(&mutex);
	flushCache();
}

void Settings::flush()
{
	QMutexLocker mL(&mutex);
	flushCache();
	sync();
}

void Settings::init(const QString &key, const QVariant &val)
{
	QMutexLocker mL(&mutex);
	if (!cache.contains(key) && !QSettings::contains(key))
		cache[key] = val;
	toRemove.remove(key);
}
bool Settings::contains(const QString &key) const
{
	QMutexLocker mL(&mutex);
	if (cache.contains(key))
		return true;
	return QSettings::contains(key);
}
void Settings::set(const QString &key, const QVariant &val)
{
	QMutexLocker mL(&mutex);
	toRemove.remove(key);
	cache[key] = val;
}
void Settings::remove(const QString &key)
{
	QMutexLocker mL(&mutex);
	toRemove.insert(key);
	cache.remove(key);
}

QVariant Settings::get(const QString &key, const QVariant &def) const
{
	QMutexLocker mL(&mutex);
	SettingsMap::const_iterator it = cache.find(key);
	if (it != cache.end())
		return it.value();
	return QSettings::value(key, def);
}

void Settings::flushCache()
{
	foreach (const QString &key, toRemove)
		QSettings::remove(key);
	toRemove.clear();

	for (SettingsMap::const_iterator it = cache.constBegin(), end_it = cache.constEnd(); it != end_it; ++it)
		QSettings::setValue(it.key(), it.value());
	cache.clear();
}
