#include <Module.hpp>

class Chiptune : public Module
{
public:
	Chiptune();
private:
	QList< Info > getModulesInfo(const bool) const;
	void *createInstance(const QString &);

	SettingsWidget *getSettingsWidget();
};

/**/

#include <QCoreApplication>

class QCheckBox;
class QSpinBox;

class ModuleSettingsWidget : public Module::SettingsWidget
{
	Q_DECLARE_TR_FUNCTIONS(ModuleSettingsWidget)
public:
	ModuleSettingsWidget(Module &);
private:
	void saveSettings();

#ifdef USE_GME
	QCheckBox *gmeB;
#endif
#ifdef USE_SIDPLAY
	QCheckBox *sidB;
#endif
	QSpinBox *lengthB;
};
